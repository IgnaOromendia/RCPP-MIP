"""Tests for the self-contained route HTML generator."""

from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from generate_route_html import (build_html, encode_background, generate,
                                 read_bounds, read_graph_neutral_nodes)


class RouteHtmlTest(unittest.TestCase):
    def test_build_html_embeds_route_and_escapes_title(self):
        html = build_html([(1, 1, 1, 2)], {1: (0, 0), 2: (1, 1)},
                          '</script><b>ruta</b>', duration=8)
        self.assertTrue(html.startswith('<!doctype html>'))
        self.assertIn('"duration":8', html)
        self.assertNotIn('</script><b>ruta</b>', html)
        self.assertIn('requestAnimationFrame', html)
        self.assertIn('return{x:0,y:0,w,h}', html)
        self.assertIn('object-fit:contain', html)
        self.assertIn('x:(w-width)/2,y:0,w:width,h', html)
        self.assertNotIn('object-fit:cover', html)
        self.assertIn('rgba(30,41,59,.52)', html)
        self.assertIn('id="map-background"', html)
        self.assertIn('ctx.clearRect(0,0,w,h)', html)
        self.assertIn('id="route-pending"', html)
        self.assertIn('stroke="#64748b"', html)
        self.assertIn('drawBaseRoute(w,h)', html)

    def test_default_animation_duration_is_48_seconds(self):
        html = build_html([(1, 1, 1, 2)], {1: (0, 0), 2: (1, 1)},
                          'Ruta')
        self.assertIn('"duration":48', html)

    def test_edges_between_neutral_nodes_turn_gray_only_when_traversed(self):
        segments = [
            (1, 1, 1, 2),
            (1, 2, 2, 3),
            (1, 3, 3, 4),
        ]
        coordinates = {1: (0, 0), 2: (1, 0), 3: (2, 0), 4: (3, 0)}
        html = build_html(segments, coordinates, 'Ruta', clusters=[1, 3, 2])
        self.assertIn('"neutralEdgeColor":"#d1d5db"', html)
        self.assertIn(
            '"source":2,"target":3,"cluster":3,"neutral":true', html)
        self.assertIn('if(segment.neutral)return DATA.neutralEdgeColor', html)
        self.assertIn('"neutralNodeColor":"#9ca3af"', html)
        self.assertIn('"neutralNodeOutline":"#ff7a00"', html)
        self.assertIn('"neutralNodes":[2,3]', html)
        self.assertIn('neutral?neutralRadius:radius', html)
        self.assertIn('neutralRadius=Math.max(14,radius*8)', html)
        self.assertIn('ctx.setLineDash([])', html)
        self.assertNotIn('for(const s of DATA.segments){if(!s.neutral)continue', html)
        self.assertNotIn('neutral?Math.max(5', html)

    def test_graph_clusters_define_neutral_nodes_from_the_complete_graph(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'graph_clusters.dat'
            path.write_text(
                'arista cluster origen destino vehiculo servicio recorridos pasadas\n'
                '1 1 1 2 1 0 0 0\n'
                '2 2 2 3 1 0 0 0\n'
                '3 2 3 4 1 0 0 0\n', encoding='utf-8')
            self.assertEqual(read_graph_neutral_nodes(path), {2})
            html = build_html(
                [(1, 1, 1, 2)], {1: (0, 0), 2: (1, 0)}, 'Ruta',
                clusters=[1], neutral_nodes=read_graph_neutral_nodes(path))
            self.assertIn('"neutralNodes":[2]', html)

    def test_generate_uses_node_number_and_conventional_output_name(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            coords = directory / 'coords.csv'
            segments = directory / 'segments.csv'
            output = directory / 'route_3.html'
            coords.write_text('node_id,x,y\n1,0,0\n2,1,1\n', encoding='utf-8')
            segments.write_text(
                'vehiculo,orden,nodo_origen,nodo_destino,cluster\n'
                '1,1,1,2,2\n', encoding='utf-8')
            result = generate(3, segments, coords, output)
            self.assertEqual(result, output)
            html = output.read_text(encoding='utf-8')
            self.assertIn('Recorrido RCPP · 3 nodos', html)
            self.assertIn('"cluster":2', html)
            self.assertIn('"interactiveMap":true', html)

    def test_interactive_map_uses_plot_zone_leaflet_projection(self):
        html = build_html(
            [(1, 1, 1, 2)],
            {1: (-34.48, -58.50), 2: (-34.47, -58.49)},
            'Mapa', interactive_map=True,
        )
        self.assertIn('leaflet@1.9.3/dist/leaflet.js', html)
        self.assertIn('setView([center.x,center.y],14)', html)
        self.assertIn('tile.openstreetmap.org/{z}/{x}/{y}.png', html)
        self.assertIn('routeMap.latLngToContainerPoint([p.x,p.y])', html)
        self.assertIn("routeMap.on('move zoom',redraw)", html)

    def test_rejects_invalid_node_count(self):
        with self.assertRaisesRegex(ValueError, 'nodes debe ser positivo'):
            generate(0)

    def test_embeds_map_and_geographic_bounds(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            image = directory / 'map.png'
            bounds = directory / 'map.bounds'
            image.write_bytes(b'\x89PNG\r\n\x1a\nmap')
            bounds.write_text('-58.6 -58.4 -34.6 -34.4\n', encoding='utf-8')
            background = encode_background(image, bounds)
            html = build_html([(1, 1, 1, 2)],
                              {1: (-34.5, -58.5), 2: (-34.4, -58.4)},
                              'Mapa', background=background)
            self.assertIn('data:image/png;base64,', html)
            self.assertIn('"bounds":[-58.6,-58.4,-34.6,-34.4]', html)
            self.assertIn('(p.y-minLon)', html)
            self.assertIn(
                'background.naturalWidth/background.naturalHeight', html)
            self.assertNotIn('"width":1', html)

    def test_rejects_malformed_bounds(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'bad.bounds'
            path.write_text('1 2 3\n', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'bounds debe contener'):
                read_bounds(path)


if __name__ == '__main__':
    unittest.main()
