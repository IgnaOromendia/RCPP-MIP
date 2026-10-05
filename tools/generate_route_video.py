#!/usr/bin/env python3
"""Animate ordered RCPP route segments from the generated coordinate CSV."""

import argparse
import csv
import math
from pathlib import Path
import shutil
import subprocess
import tempfile


PALETTE = (
    '#e11d48', '#2563eb', '#16a34a', '#ea580c', '#9333ea',
    '#0891b2', '#ca8a04', '#db2777', '#4f46e5', '#0f766e',
)


def read_coordinates(path):
    coordinates = {}
    with Path(path).open(newline='', encoding='utf-8') as source:
        reader = csv.DictReader(source)
        if reader.fieldnames != ['node_id', 'x', 'y']:
            raise ValueError('coords debe tener las columnas node_id,x,y')
        for line_number, row in enumerate(reader, start=2):
            try:
                node_id = int(row['node_id'])
                point = (float(row['x']), float(row['y']))
            except (TypeError, ValueError) as error:
                raise ValueError(f'coords: linea {line_number} invalida') from error
            if node_id <= 0 or not all(math.isfinite(value) for value in point):
                raise ValueError(f'coords: linea {line_number} invalida')
            if node_id in coordinates:
                raise ValueError(f'coords: node_id duplicado: {node_id}')
            coordinates[node_id] = point
    if not coordinates:
        raise ValueError('coords no contiene nodos')
    return coordinates


def read_segments(path):
    segments = []
    with Path(path).open(newline='', encoding='utf-8') as source:
        reader = csv.DictReader(source)
        expected = ['vehiculo', 'orden', 'nodo_origen', 'nodo_destino']
        if reader.fieldnames != expected:
            raise ValueError('segments debe tener las columnas ' + ','.join(expected))
        positions = set()
        for line_number, row in enumerate(reader, start=2):
            try:
                segment = tuple(int(row[field]) for field in expected)
            except (TypeError, ValueError) as error:
                raise ValueError(f'segments: linea {line_number} invalida') from error
            vehicle, order, source_node, target_node = segment
            if min(vehicle, order, source_node, target_node) <= 0:
                raise ValueError(f'segments: linea {line_number} invalida')
            if order in positions:
                raise ValueError(f'segments: orden duplicado: {order}')
            positions.add(order)
            segments.append(segment)
    if not segments:
        raise ValueError('segments no contiene tramos para animar')
    return sorted(segments, key=lambda segment: segment[1])


def validate_node_ids(segments, coordinates):
    used = {node for _, _, source, target in segments for node in (source, target)}
    missing = sorted(used - coordinates.keys())
    if missing:
        raise ValueError('Faltan coordenadas para los nodos: ' +
                         ', '.join(map(str, missing)))


def _screen_coordinates(coordinates, width, height, padding=55):
    x_values = [point[0] for point in coordinates.values()]
    y_values = [point[1] for point in coordinates.values()]
    x_min, x_max = min(x_values), max(x_values)
    y_min, y_max = min(y_values), max(y_values)
    x_span = x_max - x_min or 1
    y_span = y_max - y_min or 1
    scale = min((width - 2 * padding) / x_span, (height - 2 * padding) / y_span)
    x_offset = (width - x_span * scale) / 2
    y_offset = (height - y_span * scale) / 2
    return {
        node: (round(x_offset + (x - x_min) * scale),
               round(height - y_offset - (y - y_min) * scale))
        for node, (x, y) in coordinates.items()
    }


def build_frames(segments, coordinates, width=960, height=720):
    try:
        from PIL import Image, ImageDraw
    except ImportError as error:
        raise RuntimeError('Pillow no esta instalado; ejecute: python3 -m pip install pillow') from error

    points = _screen_coordinates(coordinates, width, height)
    base = Image.new('RGB', (width, height), 'white')
    drawing = ImageDraw.Draw(base)
    for x, y in points.values():
        drawing.ellipse((x - 2, y - 2, x + 2, y + 2), fill='#94a3b8')

    vehicle_colors = {
        vehicle: PALETTE[index % len(PALETTE)]
        for index, vehicle in enumerate(sorted({segment[0] for segment in segments}))
    }
    frames = []
    for vehicle, _, source, target in segments:
        drawing.line((*points[source], *points[target]),
                     fill=vehicle_colors[vehicle], width=5)
        frames.append(base.copy())
    return frames


def save_animation(segments, coordinates, output, fps=8, width=960, height=720):
    output = Path(output)
    extension = output.suffix.lower()
    if extension not in ('.gif', '.mp4'):
        raise ValueError('output debe terminar en .gif o .mp4')
    if fps <= 0:
        raise ValueError('fps debe ser positivo')
    ffmpeg = None
    if extension == '.mp4':
        ffmpeg = shutil.which('ffmpeg')
        if ffmpeg is None:
            raise RuntimeError('FFmpeg no esta instalado; use un .gif o instale ffmpeg')

    frames = build_frames(segments, coordinates, width, height)
    output.parent.mkdir(parents=True, exist_ok=True)
    if extension == '.gif':
        frames[0].save(output, save_all=True, append_images=frames[1:],
                       duration=round(1000 / fps), loop=0, disposal=2)
        return

    with tempfile.TemporaryDirectory(prefix='rcpp-route-') as directory:
        frame_dir = Path(directory)
        for index, frame in enumerate(frames):
            frame.save(frame_dir / f'frame_{index:06d}.png')
        command = [
            ffmpeg, '-hide_banner', '-loglevel', 'error', '-y',
            '-framerate', str(fps), '-i', str(frame_dir / 'frame_%06d.png'),
            '-c:v', 'libx264', '-pix_fmt', 'yuv420p', str(output),
        ]
        try:
            subprocess.run(command, check=True)
        except subprocess.CalledProcessError as error:
            raise RuntimeError(f'FFmpeg no pudo generar {output}') from error


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--segments', required=True, type=Path)
    parser.add_argument('--coords', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--fps', type=int, default=8)
    parser.add_argument('--width', type=int, default=960)
    parser.add_argument('--height', type=int, default=720)
    args = parser.parse_args(argv)
    try:
        if args.width < 100 or args.height < 100:
            raise ValueError('width y height deben ser al menos 100')
        coordinates = read_coordinates(args.coords)
        segments = read_segments(args.segments)
        validate_node_ids(segments, coordinates)
        save_animation(segments, coordinates, args.output, args.fps,
                       args.width, args.height)
    except (OSError, ValueError, RuntimeError) as error:
        parser.error(str(error))
    print(f'Video guardado en {args.output}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
