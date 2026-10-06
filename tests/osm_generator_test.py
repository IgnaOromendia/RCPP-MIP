"""Regression tests for the geographic/OSM generator output contract."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent.parent
GENERATOR = ROOT / "generator"
sys.path.insert(0, str(GENERATOR))

from exporter import Exporter  # noqa: E402
from fetcher import Fetcher  # noqa: E402
import generator as generator_module  # noqa: E402
from generator import Generator  # noqa: E402
from graph import Graph  # noqa: E402
from importer import Importer  # noqa: E402
from mapToOsmGraph import parse_arguments  # noqa: E402
from osmGraph import OsmGraph  # noqa: E402
from paths import GeneratorPaths  # noqa: E402


class OsmGeneratorTest(unittest.TestCase):
    reader = None

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="rcpp-osm-generator-")
        self.repository = Path(self.temporary.name).resolve()
        (self.repository / "generator" / "polygons").mkdir(parents=True)
        self.paths = GeneratorPaths.for_repository(self.repository)

    def tearDown(self):
        self.temporary.cleanup()

    @staticmethod
    def sample_graph():
        graph = Graph(4)  # índice reservado 0 y nodos viales 1..3
        graph.addWeightedEdge(1, 2, 1.25, 3.5, -1)
        graph.addWeightedEdge(2, 3, 2.75, 0, 0)
        return graph

    def export_sample(self):
        exporter = Exporter("sample", self.paths)
        graph = self.sample_graph()
        graph_path = exporter.exportAsInput(graph, [1], 2)
        turns_path = exporter.exportCurves(
            [(1, 2, 3)], [(3, 2, 1)], node_count=3
        )
        return graph_path, turns_path

    def test_output_root_must_be_inside_data(self):
        with self.assertRaisesRegex(ValueError, "dentro de"):
            GeneratorPaths.for_repository(
                self.repository, data_root=self.repository / "generated"
            )

    def test_export_uses_canonical_node_count_and_data_root(self):
        graph_path, turns_path = self.export_sample()
        self.assertEqual(graph_path, self.repository / "data/generator/input/sample.dat")
        self.assertEqual(turns_path,
                         self.repository / "data/generator/input/sample.turns.dat")
        self.assertEqual(graph_path.read_text().splitlines()[0], "2 3 1 2 0")
        for legacy_directory in ("input", "coords", "curves", "idMaps",
                                 "distancias", "streetAttributes", "osmMaps"):
            self.assertFalse((self.repository / legacy_directory).exists())

    def test_export_is_accepted_by_cpp_reader(self):
        if self.reader is None:
            self.skipTest("No InstanceReader binary was supplied")
        graph_path, turns_path = self.export_sample()
        result = subprocess.run(
            [self.reader, graph_path, turns_path], capture_output=True,
            text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout.strip(), "3 2 1 1 1")

    def test_import_and_zonification_round_trip(self):
        graph_path, _ = self.export_sample()
        self.paths.cell_mapping("sample").parent.mkdir(parents=True, exist_ok=True)
        self.paths.cell_mapping("sample").write_text("1 0\n2 0\n3 1\n")
        self.paths.zonification("sample").write_text("2\n0 0\n1 1\n")

        imported, nearest = Importer("sample", self.paths).importGraph()
        self.assertEqual(imported.n, 4)
        self.assertEqual(nearest, [1])
        output = Generator(paths=self.paths).applyZonificationTo("sample")
        self.assertEqual(output, graph_path)

        vehicles, nodes, _, edges, _ = Importer(
            "sample", self.paths
        ).importGraphRecords()
        self.assertEqual((vehicles, nodes), (2, 3))
        self.assertEqual(edges[0][2], 1)
        self.assertEqual(edges[1][2], 0)

    def test_invalid_turn_is_rejected_before_writing(self):
        exporter = Exporter("invalid", self.paths)
        with self.assertRaisesRegex(ValueError, "giro inválido"):
            exporter.exportCurves([(1, 2, 1)], [], node_count=3)
        self.assertFalse(self.paths.turns("invalid").exists())

    def test_curve_detection_ignores_u_turns(self):
        osm_graph = OsmGraph.__new__(OsmGraph)
        osm_graph.graph = SimpleNamespace(
            neighbours=lambda node: [1] if node == 2 else []
        )
        osm_graph.wayName = {(1, 2): "Calle A", (2, 1): "Calle B"}
        osm_graph.curves = []
        osm_graph.illegal_curves = []

        osm_graph.detectCurves()

        self.assertEqual(osm_graph.curves, [])
        self.assertEqual(osm_graph.illegal_curves, [])

    def test_grid_edges_keep_their_one_based_ids(self):
        grid = SimpleNamespace(
            numberOfCells=2,
            numberOfEdges=2,
            cellDemand=[1.5, 2.5],
            adjGrid=[[], [2], [1]],
        )
        output = Exporter("grid", self.paths).exportGridGraph(grid)
        self.assertEqual(
            output.read_text().splitlines(),
            ["2 2", "1.5000", "2.5000", "1 2", "2 1"],
        )

    def test_failed_reexport_preserves_previous_instance(self):
        graph_path, _ = self.export_sample()
        previous = graph_path.read_text()
        with self.assertRaisesRegex(ValueError, "fuera de"):
            Exporter("sample", self.paths).exportAsInput(
                self.sample_graph(), [1], 2, zones=[-1, 3, 3, 3]
            )
        self.assertEqual(graph_path.read_text(), previous)

    def test_cli_accepts_map_only_without_grid(self):
        options = parse_arguments(["sample", "--sin-grilla"])
        self.assertTrue(options.sin_grilla)
        self.assertIsNone(options.celdas)
        self.assertIsNone(options.calle)

        compatible = parse_arguments([
            "sample", "100", "Reference street", "--sin-grilla"
        ])
        self.assertTrue(compatible.sin_grilla)
        self.assertEqual(compatible.celdas, 100)

    def test_fetcher_identifies_itself_and_limits_the_overpass_query(self):
        class Response:
            def raise_for_status(self):
                pass

            def json(self):
                return {"elements": []}

        class HttpClient:
            def __init__(self):
                self.call = None

            def post(self, url, **kwargs):
                self.call = (url, kwargs)
                return Response()

        importer = SimpleNamespace(importGeoJSON=lambda _: {
            "features": [{"geometry": {
                "coordinates": [[[-58.5, -34.5], [-58.4, -34.4]]]
            }}]
        })
        http_client = HttpClient()

        result = Fetcher(importer=importer, http_client=http_client).fetchZoneFrom(
            "sampleTotal"
        )

        self.assertEqual(result, {"elements": []})
        _, request = http_client.call
        self.assertEqual(request["headers"]["User-Agent"], Fetcher.USER_AGENT)
        query = request["data"]["data"]
        self.assertIn('way["highway"~', query)
        self.assertNotIn("node(poly:", query)
        self.assertIn("residential", query)

    def test_generation_without_grid_does_not_construct_grid(self):
        generator = Generator(paths=self.paths)
        with patch.object(generator_module, "OsmGraph") as osm_graph, \
                patch.object(generator_module, "Grid") as grid:
            generated_osm = osm_graph.return_value
            generator.generateFrom2GeoJSON(
                {}, {"features": [{"geometry": {}}]}, None, "sample",
                generateGrid=False,
            )
        grid.assert_not_called()
        generated_osm.angleOfStreet.assert_not_called()
        self.assertIsNone(generator.grid)

    def test_export_without_grid_keeps_binary_required_zones(self):
        generator = Generator(paths=self.paths)
        generator.graph = self.sample_graph()
        generator.osmGraph = SimpleNamespace(
            osm=ET.Element("osm", version="0.6"),
            nearstNodesToDepo=[1],
            nodesCoordinates={101: (-34.5, -58.5)},
            nodeOsmToNodeMap={101: 1},
            curves=[(1, 2, 3)],
            illegal_curves=[],
        )

        written = generator.export("no_grid")
        self.assertEqual(len(written), 7)
        self.assertFalse(self.paths.grid_graph("no_grid").exists())
        self.assertFalse(self.paths.grid_coordinates("no_grid").exists())
        self.assertFalse(self.paths.cell_mapping("no_grid").exists())
        self.assertFalse(self.paths.cell_distances("no_grid").exists())

        _, _, _, edges, _ = Importer(
            "no_grid", self.paths
        ).importGraphRecords()
        self.assertEqual([edge[2] for edge in edges], [-1, 0])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--reader", type=Path)
    options, unittest_arguments = parser.parse_known_args()
    OsmGeneratorTest.reader = options.reader.resolve() if options.reader else None
    unittest.main(argv=[sys.argv[0], *unittest_arguments])


if __name__ == "__main__":
    main()
