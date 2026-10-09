"""Tests for the self-contained route HTML generator."""

from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from generate_route_html import (build_html, encode_background, generate,
                                 read_bounds)


class RouteHtmlTest(unittest.TestCase):
    def test_build_html_embeds_route_and_escapes_title(self):
        html = build_html([(1, 1, 1, 2)], {1: (0, 0), 2: (1, 1)},
                          '</script><b>ruta</b>', duration=8)
        self.assertTrue(html.startswith('<!doctype html>'))
        self.assertIn('"duration":8', html)
        self.assertNotIn('</script><b>ruta</b>', html)
        self.assertIn('requestAnimationFrame', html)

    def test_default_animation_duration_is_48_seconds(self):
        html = build_html([(1, 1, 1, 2)], {1: (0, 0), 2: (1, 1)},
                          'Ruta')
        self.assertIn('"duration":48', html)

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

    def test_rejects_malformed_bounds(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'bad.bounds'
            path.write_text('1 2 3\n', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'bounds debe contener'):
                read_bounds(path)


if __name__ == '__main__':
    unittest.main()
