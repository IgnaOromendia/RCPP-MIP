"""Tests for route CSV parsing and GIF/MP4 export diagnostics."""

from pathlib import Path
from unittest import mock
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from generate_route_video import (_screen_coordinates, read_coordinates,
                                  read_segments, save_animation,
                                  validate_node_ids)


class RouteVideoTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.directory = Path(self.temporary.name)
        self.coords = self.directory / 'coords.csv'
        self.segments = self.directory / 'segments.csv'
        self.coords.write_text('node_id,x,y\n1,0,0\n2,1,0\n3,1,1\n')
        self.segments.write_text(
            'vehiculo,orden,nodo_origen,nodo_destino\n'
            '2,4,2,3\n1,2,1,2\n2,3,3,2\n')

    def tearDown(self):
        self.temporary.cleanup()

    def test_read_and_sort_multiple_vehicles_and_repeated_edges(self):
        coordinates = read_coordinates(self.coords)
        segments = read_segments(self.segments)
        self.assertEqual(segments, [(1, 2, 1, 2), (2, 3, 3, 2), (2, 4, 2, 3)])
        validate_node_ids(segments, coordinates)

    def test_missing_coordinate_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Faltan coordenadas.*4'):
            validate_node_ids([(1, 1, 1, 4)], read_coordinates(self.coords))

    def test_duplicate_order_is_rejected(self):
        self.segments.write_text(
            'vehiculo,orden,nodo_origen,nodo_destino\n'
            '1,1,1,2\n2,1,2,3\n')
        with self.assertRaisesRegex(ValueError, 'orden duplicado'):
            read_segments(self.segments)

    def test_missing_ffmpeg_has_actionable_error(self):
        with mock.patch('generate_route_video.shutil.which', return_value=None):
            with self.assertRaisesRegex(RuntimeError, 'FFmpeg no esta instalado'):
                save_animation([(1, 1, 1, 2)], {1: (0, 0), 2: (1, 1)},
                               self.directory / 'route.mp4')

    def test_coordinate_origin_is_rendered_at_the_top_left(self):
        points = _screen_coordinates({1: (0, 0), 2: (1, 1)}, 240, 180)
        self.assertLess(points[1][0], points[2][0])
        self.assertLess(points[1][1], points[2][1])

    def test_gif_export(self):
        try:
            import PIL  # noqa: F401
        except ImportError:
            self.skipTest('Pillow is not installed')
        output = self.directory / 'output' / 'videos' / 'route.gif'
        save_animation(read_segments(self.segments), read_coordinates(self.coords),
                       output, fps=5, width=240, height=180)
        self.assertTrue(output.read_bytes().startswith(b'GIF'))


if __name__ == '__main__':
    unittest.main()
