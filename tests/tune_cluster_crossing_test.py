#!/usr/bin/env python3
"""Regression tests for the singular pre-solver cluster-crossing command."""

from decimal import Decimal
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
TUNER = ROOT / "tune_cluster_crossing.py"
sys.path.insert(0, str(ROOT))

from tune_cluster_crossing import (
    expand_neutral_nodes,
    expand_turns,
    read_edge_clusters,
    read_graph,
    read_turns,
    write_expansion_svg,
)
from clusterGeneration.generate_clusters import read_coordinate_geometry


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    with tempfile.TemporaryDirectory(prefix="cluster-crossings-") as raw_directory:
        directory = Path(raw_directory)
        graph_path = directory / "graph.dat"
        graph_path.write_text(
            "1 5 1 4 1\n1\n"
            "1 2 0 1.5 0\n"
            "2 3 -1 7 2\n"
            "3 4 -1 2 3\n"
            "4 5 0 3 0\n"
            "2 4 0 4 0\n",
            encoding="utf-8",
        )
        clusters_path = directory / "clusters.dat"
        clusters_path.write_text(
            "# pre-solver\n"
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n"
            "1 1 1 2 1 0 0 0\n"
            "2 1 2 3 1 0 0 0\n"
            "3 2 3 4 1 0 0 0\n"
            "4 2 4 5 1 0 0 0\n"
            "5 1 2 4 1 0 0 0\n",
            encoding="utf-8",
        )
        coordinates_path = directory / "graph.coords.csv"
        coordinates_path.write_text(
            "node_id,x,y\n"
            "1,0,0\n"
            "2,1,0\n"
            "3,2,0\n"
            "4,3,0\n"
            "5,4,0\n",
            encoding="utf-8",
        )

        graph = read_graph(graph_path)
        assignments = read_edge_clusters(clusters_path, graph)
        expansion = expand_neutral_nodes(graph, assignments)
        expanded = expansion.graph

        check(expansion.neutral_nodes == (3, 4), "unexpected neutral nodes")
        check(expansion.record_clusters == (1, 1, None, 2, 1),
              "neutral-to-neutral edges must not belong to a cluster")
        check(expansion.cluster_costs == {1: Decimal("12.5"), 2: Decimal("3")},
              "neutral-to-neutral edge costs leaked into a cluster")
        check(expanded.nodes == 7 and expanded.edge_count == 5 and expanded.arc_count == 5,
              "expanded graph header is inconsistent")
        check(expansion.transition_count == 4,
              "each neutral node needs both directed cluster transitions")
        check(expansion.transition_clusters == ((1, 2), (2, 1), (1, 2), (2, 1)),
              "cluster-transition directions were not retained")

        neutral_copies = [record for record in expanded.records[:expanded.edge_count]
                          if record.cost == Decimal("2")]
        check(len(neutral_copies) == 2,
              "neutral-to-neutral physical edge was not copied into both layers")
        check(sorted((record.zone, record.demand) for record in neutral_copies) ==
              [(-1, Decimal("3")), (0, Decimal("0"))],
              "a copied required edge must be serviced exactly in its assigned layer")

        transitions = expanded.records[-expansion.transition_count:]
        check(all(record.zone == 0 and record.demand == 0 for record in transitions),
              "synthetic transitions must be optional")
        check({record.cost for record in transitions} == {Decimal("15.5")},
              "cluster transitions do not cost S_i + S_j")
        check({(record.source, record.target) for record in transitions} ==
              {(3, 6), (6, 3), (4, 7), (7, 4)},
              "neutral in/out copies are connected incorrectly")
        check(len(expansion.virtual_node_mapping) == 30,
              "not every expanded in/out node was mapped")
        check(expansion.virtual_node_mapping[21] == 17 and
              expansion.virtual_node_mapping[22] == 18,
              "the copied directed arc was not mapped to its original in/out nodes")
        check(all(expansion.virtual_node_mapping[node] is None for node in range(23, 31)),
              "synthetic transition nodes must not map to physical in/out nodes")
        check(expand_turns([(2, 3, 4)], expansion.original_node_copies) == [
            (2, 3, 4), (2, 3, 7), (2, 6, 4), (2, 6, 7)
        ], "turns were not translated to every expanded node copy")

        preview_svg = directory / "preview.svg"
        geometry = read_coordinate_geometry(graph_path, coordinates_path)
        write_expansion_svg(preview_svg, expansion, geometry)
        svg = preview_svg.read_text(encoding="utf-8")
        check("Cluster 1" in svg and "Cluster 2" in svg,
              "expansion preview does not identify its clusters")
        check('data-kind="arista neutra"' in svg and 'data-neutral="true"' in svg,
              "expansion preview does not identify neutral edges and nodes")
        check('data-kind="transición sintética"' in svg,
              "expansion preview does not identify synthetic transitions")
        check('data-map-background="true"' in svg and 'cx="50.00"' in svg,
              "expansion preview does not use original graph coordinates")
        output = directory / "graph.tuned.dat"
        mapping = directory / "graph.tuned.mapping.dat"
        in_out = directory / "graph.tuned.in-out.dat"
        turns = directory / "graph.turns.dat"
        turns.write_text("1 1\n2 3 4\n3 4 5\n", encoding="utf-8")
        expanded_turns = directory / "graph.tuned.turns.dat"
        result = subprocess.run(
            [sys.executable, TUNER, graph_path, clusters_path, output, mapping,
             "--coords", coordinates_path,
             "--turns", turns, "--expanded-turns", expanded_turns],
            capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        png = output.with_suffix(".png")
        check(png.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"),
              "CLI did not render the expanded graph PNG")
        reread = read_graph(output)
        check(reread == expanded, "CLI output cannot be read back losslessly")
        check("2 nodos neutros" in result.stdout and "4 transiciones" in result.stdout,
              "CLI summary does not report the expansion")
        check(str(png) in result.stdout, "CLI summary does not report the PNG path")
        mapping_rows = mapping.read_text(encoding="utf-8").splitlines()
        check(mapping_rows[0] == "expanded_node original_node" and
              mapping_rows[-1] == "30 -",
              "CLI did not export the virtual-node mapping")
        in_out_rows = in_out.read_text(encoding="utf-8").splitlines()
        check(in_out_rows[0] == "cluster in_node out_node",
              "CLI did not export the in/out header")
        check(in_out_rows == [
            "cluster in_node out_node",
            "1 23 24",
            "2 25 26",
            "1 27 28",
            "2 29 30",
        ], "CLI did not export the directed synthetic cluster transitions")
        translated, translated_illegal = read_turns(expanded_turns, expanded.nodes)
        check(translated == [(2, 3, 4), (2, 3, 7), (2, 6, 4), (2, 6, 7)],
              "CLI did not translate listed turns")
        check(translated_illegal == [(3, 4, 5), (3, 7, 5),
                                     (6, 4, 5), (6, 7, 5)],
              "CLI did not translate prohibited turns")
        check(graph_path.read_text(encoding="utf-8").splitlines()[3].split()[3] == "7",
              "the original graph was modified")

        invalid = clusters_path.with_name("invalid.dat")
        invalid.write_text(
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n"
            "1 1 9 2 1 0 0 0\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(
            [sys.executable, TUNER, graph_path, invalid, directory / "bad.dat"],
            capture_output=True, text=True, timeout=10,
        )
        check(rejected.returncode == 1 and "extremos no coinciden" in rejected.stderr,
              "a cluster file for a different graph was accepted")

        three_node_graph = directory / "three-node.dat"
        three_node_graph.write_text(
            "1 3 2 2 0\n1\n3\n"
            "1 2 1 7 3\n"
            "2 3 1 11 4\n",
            encoding="utf-8",
        )
        three_node_clusters = directory / "three-node-clusters.dat"
        three_node_clusters.write_text(
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n"
            "1 1 1 2 1 0 0 0\n"
            "2 2 2 3 1 0 0 0\n",
            encoding="utf-8",
        )
        three_node_output = directory / "three-node.tuned.dat"
        three_node_result = subprocess.run(
            [sys.executable, TUNER, three_node_graph, three_node_clusters,
             three_node_output],
            capture_output=True, text=True, timeout=10,
        )
        check(three_node_result.returncode == 0,
              three_node_result.stdout + three_node_result.stderr)
        check(three_node_output.with_suffix(".in-out.dat").read_text(
            encoding="utf-8").splitlines() == [
                "cluster in_node out_node",
                "1 9 10",
                "2 11 12",
            ], "the three-node path did not expose both directed cluster-change Ys")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
