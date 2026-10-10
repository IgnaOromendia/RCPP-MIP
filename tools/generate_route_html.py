#!/usr/bin/env python3
"""Generate a self-contained HTML animation for an ordered RCPP route."""

import argparse
import base64
import html
import json
import mimetypes
from pathlib import Path

from generate_route_video import (read_coordinates, read_segment_clusters,
                                  read_segments, validate_node_ids)


ROOT = Path(__file__).resolve().parent.parent

HTML_TEMPLATE = r'''<!doctype html>
<html lang="es">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>__DOCUMENT_TITLE__</title>
<style>
:root{color-scheme:dark;--bg:#07101d;--panel:rgba(8,15,27,.86);--line:rgba(148,163,184,.2);--text:#f8fafc;--muted:#94a3b8;--accent:#38bdf8}
*{box-sizing:border-box}html,body{height:100%}body{margin:0;overflow:hidden;color:var(--text);background:radial-gradient(circle at 15% 10%,#0c4a6e88,transparent 35rem),var(--bg);font:14px/1.4 Inter,system-ui,sans-serif}
main{position:relative;width:100%;height:100%}canvas{display:block;width:100%;height:100%}
.top{position:absolute;top:24px;left:24px;right:24px;display:flex;align-items:flex-start;justify-content:space-between;gap:20px;pointer-events:none}
.title,.stats,.controls{border:1px solid var(--line);background:var(--panel);box-shadow:0 16px 50px #0006;backdrop-filter:blur(16px)}
.title{max-width:min(650px,70vw);padding:15px 18px;border-radius:15px}.eyebrow{color:#22d3ee;font-size:10px;font-weight:800;letter-spacing:.18em;text-transform:uppercase}.title h1{margin:3px 0 0;overflow:hidden;font-size:clamp(20px,3vw,34px);line-height:1.05;letter-spacing:-.035em;text-overflow:ellipsis;white-space:nowrap}
.stats{display:flex;gap:18px;padding:12px 16px;border-radius:13px}.stat{display:grid}.stat b{font-size:17px;font-variant-numeric:tabular-nums}.stat span{color:var(--muted);font-size:10px;letter-spacing:.08em;text-transform:uppercase}
.controls{position:absolute;left:50%;bottom:22px;width:min(850px,calc(100% - 32px));padding:11px 13px;border-radius:15px;transform:translateX(-50%)}
.row{display:flex;align-items:center;gap:10px}button,select{border:1px solid var(--line);border-radius:9px;color:var(--text);background:#1e293bcc;font:inherit}button{min-width:39px;height:38px;padding:0 12px;font-weight:800;cursor:pointer}button:hover{border-color:var(--accent)}button.primary{color:#06111a;border:0;background:linear-gradient(135deg,#38bdf8,#22d3ee)}input[type=range]{flex:1;accent-color:var(--accent)}output{width:88px;color:var(--muted);font-variant-numeric:tabular-nums;text-align:right}.options{display:flex;align-items:center;gap:8px;margin-top:8px;color:var(--muted);font-size:11px}.options select{padding:5px 7px}.spacer{flex:1}
@media(max-width:650px){.top{top:12px;left:12px;right:12px}.stats{display:none}.title{max-width:100%}.controls{bottom:10px}.skip{display:none}}
</style>
</head>
<body><main>
<canvas id="route"></canvas>
<div class="top"><section class="title"><div class="eyebrow">RCPP · Recorrido animado</div><h1 id="title"></h1></section><section class="stats"><div class="stat"><b id="nodes"></b><span>Nodos</span></div><div class="stat"><b id="segments"></b><span>Tramos</span></div><div class="stat"><b id="vehicles"></b><span>Vehículos</span></div></section></div>
<section class="controls"><div class="row"><button class="skip" id="back">−10</button><button class="primary" id="play" aria-label="Reproducir o pausar">▶</button><button class="skip" id="forward">+10</button><input id="progress" type="range" min="0" max="1000" value="0"><output id="counter">0 / 0</output></div><div class="options"><span>Velocidad</span><select id="speed"><option value=".5">0,5×</option><option value="1" selected>1×</option><option value="2">2×</option><option value="4">4×</option></select><span class="spacer"></span><button id="restart">Reiniciar</button><button id="fullscreen">Pantalla completa</button></div></section>
</main>
<script>
'use strict';
const DATA=__ROUTE_DATA__;
const COLORS=['#fb7185','#38bdf8','#4ade80','#fb923c','#c084fc','#22d3ee','#facc15','#f472b6','#818cf8','#2dd4bf'];
const CLUSTERS=['#3b82f6','#ef4444','#22c55e','#a855f7','#f97316','#06b6d4','#ec4899','#84cc16','#6366f1','#eab308','#14b8a6','#d946ef'];
const $=id=>document.getElementById(id),canvas=$('route'),ctx=canvas.getContext('2d',{alpha:false});
const coords=new Map(DATA.coordinates.map(p=>[p.id,p])),vehicles=[...new Set(DATA.segments.map(s=>s.vehicle))];
let progress=0,playing=false,startedAt=0,startProgress=0,raf=0;
$('title').textContent=DATA.title;$('nodes').textContent=DATA.coordinates.length;$('segments').textContent=DATA.segments.length;$('vehicles').textContent=vehicles.length;
const background=new Image();if(DATA.background){background.onload=fit;background.src=DATA.background.data}
function fit(){const w=innerWidth,h=innerHeight,dpr=Math.min(devicePixelRatio||1,3);canvas.width=Math.round(w*dpr);canvas.height=Math.round(h*dpr);canvas.style.width=w+'px';canvas.style.height=h+'px';ctx.setTransform(dpr,0,0,dpr,0,0);draw()}
function mapRect(){const w=innerWidth,h=innerHeight,left=Math.max(22,w*.035),right=left,top=Math.max(105,h*.13),bottom=Math.max(115,h*.16),availableW=w-left-right,availableH=h-top-bottom;if(!DATA.background)return{x:left,y:top,w:availableW,h:availableH};const imageRatio=DATA.background.width/DATA.background.height,availableRatio=availableW/availableH;if(availableRatio>imageRatio){const width=availableH*imageRatio;return{x:(w-width)/2,y:top,w:width,h:availableH}}const height=availableW/imageRatio;return{x:left,y:top+(availableH-height)/2,w:availableW,h:height}}
function layout(){const values=[...coords.values()],rect=mapRect();if(DATA.background){const[minLon,maxLon,minLat,maxLat]=DATA.background.bounds;return new Map(values.map(p=>[p.id,{x:rect.x+(p.y-minLon)/(maxLon-minLon)*rect.w,y:rect.y+(maxLat-p.x)/(maxLat-minLat)*rect.h}]))}const xs=values.map(p=>p.x),ys=values.map(p=>p.y),minX=Math.min(...xs),maxX=Math.max(...xs),minY=Math.min(...ys),maxY=Math.max(...ys),spanX=maxX-minX||1,spanY=maxY-minY||1,scale=Math.min(rect.w/spanX,rect.h/spanY),ox=rect.x+(rect.w-spanX*scale)/2,oy=rect.y+(rect.h-spanY*scale)/2;return new Map(values.map(p=>[p.id,{x:ox+(p.x-minX)*scale,y:oy+(p.y-minY)*scale}]))}
function color(segment){if(segment.cluster!==null)return CLUSTERS[(segment.cluster-1)%CLUSTERS.length];return COLORS[vehicles.indexOf(segment.vehicle)%COLORS.length]}
function draw(){const w=innerWidth,h=innerHeight;ctx.fillStyle='#07101d';ctx.fillRect(0,0,w,h);const rect=mapRect();if(DATA.background&&background.complete){ctx.save();ctx.shadowColor='#000';ctx.shadowBlur=35;ctx.drawImage(background,rect.x,rect.y,rect.w,rect.h);ctx.restore()}else{const grid=Math.max(44,Math.min(w,h)*.075);ctx.save();ctx.globalAlpha=.12;ctx.strokeStyle='#38bdf8';ctx.lineWidth=1;for(let x=0;x<w;x+=grid){ctx.beginPath();ctx.moveTo(x,0);ctx.lineTo(x,h);ctx.stroke()}for(let y=0;y<h;y+=grid){ctx.beginPath();ctx.moveTo(0,y);ctx.lineTo(w,y);ctx.stroke()}ctx.restore()}const points=layout();ctx.save();ctx.lineCap='round';ctx.strokeStyle=DATA.background?'#0f172a55':'#94a3b82d';ctx.lineWidth=Math.max(1.5,Math.min(w,h)*.003);for(const s of DATA.segments){const a=points.get(s.source),b=points.get(s.target);ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(b.x,b.y);ctx.stroke()}ctx.restore();const exact=progress*DATA.segments.length,complete=Math.floor(exact),fraction=exact-complete;ctx.save();ctx.lineCap='round';ctx.lineJoin='round';ctx.lineWidth=Math.max(3,Math.min(w,h)*.007);ctx.shadowBlur=Math.min(w,h)*.012;for(let i=0;i<Math.min(complete+(fraction>0?1:0),DATA.segments.length);i++){const s=DATA.segments[i],a=points.get(s.source),b=points.get(s.target),end=i<complete?1:fraction;ctx.strokeStyle=color(s);ctx.shadowColor='#fff';ctx.beginPath();ctx.moveTo(a.x,a.y);ctx.lineTo(a.x+(b.x-a.x)*end,a.y+(b.y-a.y)*end);ctx.stroke()}ctx.restore();ctx.save();ctx.globalAlpha=DATA.background?.28:.5;ctx.fillStyle=DATA.background?'#172033':'#cbd5e1';const radius=Math.max(.8,Math.min(w,h)*.0015);for(const p of points.values()){ctx.beginPath();ctx.arc(p.x,p.y,radius,0,Math.PI*2);ctx.fill()}ctx.restore();if(exact>0&&complete<DATA.segments.length){const s=DATA.segments[complete],a=points.get(s.source),b=points.get(s.target),x=a.x+(b.x-a.x)*fraction,y=a.y+(b.y-a.y)*fraction;ctx.fillStyle='#fff';ctx.shadowColor=color(s);ctx.shadowBlur=22;ctx.beginPath();ctx.arc(x,y,Math.max(5,Math.min(w,h)*.009),0,Math.PI*2);ctx.fill();ctx.shadowBlur=0}$('progress').value=Math.round(progress*1000);$('counter').textContent=`${Math.min(DATA.segments.length,Math.ceil(exact))} / ${DATA.segments.length}`}
function pause(){playing=false;cancelAnimationFrame(raf);$('play').textContent='▶'}
function play(){if(playing){pause();return}if(progress>=1)progress=0;playing=true;startProgress=progress;startedAt=performance.now();$('play').textContent='Ⅱ';tick(startedAt)}
function tick(now){if(!playing)return;const duration=DATA.duration*1000/Number($('speed').value);progress=Math.min(1,startProgress+(now-startedAt)/duration);draw();if(progress<1)raf=requestAnimationFrame(tick);else pause()}
function move(amount){pause();progress=Math.max(0,Math.min(1,progress+amount/DATA.segments.length));draw()}
$('play').onclick=play;$('back').onclick=()=>move(-10);$('forward').onclick=()=>move(10);$('restart').onclick=()=>{pause();progress=0;draw()};$('progress').oninput=()=>{pause();progress=Number($('progress').value)/1000;draw()};$('fullscreen').onclick=()=>document.fullscreenElement?document.exitFullscreen():document.documentElement.requestFullscreen();addEventListener('resize',fit);addEventListener('keydown',e=>{if(e.code==='Space'){e.preventDefault();play()}else if(e.key==='ArrowRight')move(1);else if(e.key==='ArrowLeft')move(-1)});fit();
</script></body></html>'''


def read_bounds(path):
    """Read min_lon, max_lon, min_lat and max_lat from a bounds file."""
    values = [float(value) for value in Path(path).read_text(
        encoding='utf-8').split()]
    if len(values) != 4:
        raise ValueError('bounds debe contener min_lon max_lon min_lat max_lat')
    min_lon, max_lon, min_lat, max_lat = values
    if min_lon >= max_lon or min_lat >= max_lat:
        raise ValueError('bounds contiene limites invalidos')
    return values


def encode_background(image_path, bounds_path):
    """Return an embeddable map image and its geographic bounds."""
    image_path = Path(image_path)
    mime_type = mimetypes.guess_type(image_path.name)[0] or 'image/png'
    encoded = base64.b64encode(image_path.read_bytes()).decode('ascii')
    return {
        'data': f'data:{mime_type};base64,{encoded}',
        'bounds': read_bounds(bounds_path),
        'width': 1,
        'height': 1,
    }


def build_html(segments, coordinates, title, duration=48, clusters=None,
               background=None):
    """Return a standalone HTML document containing the complete route."""
    if duration <= 0:
        raise ValueError('duration debe ser positiva')
    if clusters is not None and len(clusters) != len(segments):
        raise ValueError('La cantidad de clusters no coincide con los tramos')
    payload = {
        'title': title,
        'duration': duration,
        'background': background,
        'coordinates': [
            {'id': node, 'x': point[0], 'y': point[1]}
            for node, point in sorted(coordinates.items())
        ],
        'segments': [
            {
                'vehicle': vehicle,
                'order': order,
                'source': source,
                'target': target,
                'cluster': clusters[index] if clusters is not None else None,
            }
            for index, (vehicle, order, source, target) in enumerate(segments)
        ],
    }
    route_data = json.dumps(payload, ensure_ascii=False, separators=(',', ':'))
    route_data = route_data.replace('<', '\\u003c')
    document_title = html.escape(title, quote=False)
    return (HTML_TEMPLATE.replace('__ROUTE_DATA__', route_data)
            .replace('__DOCUMENT_TITLE__', document_title))


def _map_name_for_nodes(nodes):
    for graph_path in sorted((ROOT / 'data/generator/input').glob('*.dat')):
        if graph_path.name.endswith('.turns.dat'):
            continue
        fields = graph_path.read_text(encoding='utf-8').split(None, 3)
        if len(fields) >= 2 and fields[1].isdigit() and int(fields[1]) == nodes:
            return graph_path.stem
    return None


def _default_sources(nodes):
    map_name = _map_name_for_nodes(nodes)
    output_directory = ROOT / 'data' / str(nodes)
    generic_coords = output_directory / f'graph_{nodes}.coords.csv'
    coordinates = generic_coords
    if map_name and not generic_coords.exists():
        coordinates = (ROOT / 'data/generator/coordinates' /
                       f'nodes_{map_name}.coords.csv')
    normal_segments = output_directory / f'route_segments_{nodes}.csv'
    segments = normal_segments
    if not normal_segments.exists():
        segments = output_directory / f'route_segments_{nodes}_h.csv'
    return map_name, segments, coordinates


def _default_map_files(map_name):
    if not map_name:
        return None, None
    directories = [
        ROOT / 'data/generator/maps',
        ROOT.parent / 'RESIDUOS-SI/pngMaps',
    ]
    for directory in directories:
        image = directory / f'{map_name}.png'
        bounds = directory / f'{map_name}.bounds'
        if image.exists() and bounds.exists():
            return image, bounds
    return None, None


def generate(nodes, segments_path=None, coordinates_path=None, output_path=None,
             title=None, duration=48, map_image=None, bounds_path=None):
    """Read conventional route files for ``nodes`` and write its HTML."""
    if nodes <= 0:
        raise ValueError('nodes debe ser positivo')
    map_name, default_segments, default_coordinates = _default_sources(nodes)
    segments_path = Path(segments_path or default_segments)
    coordinates_path = Path(coordinates_path or default_coordinates)
    output_path = Path(output_path or ROOT / 'data' / str(nodes) /
                       f'route_{nodes}.html')
    coordinates = read_coordinates(coordinates_path)
    segments = read_segments(segments_path)
    validate_node_ids(segments, coordinates)
    with segments_path.open(encoding='utf-8') as source:
        header = source.readline().strip().split(',')
    clusters = read_segment_clusters(segments_path) if 'cluster' in header else None
    default_image, default_bounds = _default_map_files(map_name)
    map_image = Path(map_image) if map_image else default_image
    bounds_path = Path(bounds_path) if bounds_path else default_bounds
    if bool(map_image) != bool(bounds_path):
        raise ValueError('map-image y bounds deben indicarse juntos')
    background = (encode_background(map_image, bounds_path)
                  if map_image and bounds_path else None)
    html = build_html(segments, coordinates,
                      title or f'Recorrido RCPP · {nodes} nodos', duration,
                      clusters, background)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(html, encoding='utf-8')
    return output_path


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('nodes', type=int, help='cantidad de nodos del grafo')
    parser.add_argument('--segments', type=Path)
    parser.add_argument('--coords', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--title')
    parser.add_argument('--duration', type=float, default=48,
                        help='duracion base de la animacion en segundos')
    parser.add_argument('--map-image', type=Path,
                        help='imagen PNG/JPG del mapa de fondo')
    parser.add_argument('--bounds', type=Path,
                        help='archivo con min_lon max_lon min_lat max_lat')
    args = parser.parse_args(argv)
    try:
        output = generate(args.nodes, args.segments, args.coords, args.output,
                          args.title, args.duration, args.map_image, args.bounds)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f'Animacion HTML guardada en {output}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
