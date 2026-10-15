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

from tune_cluster_crossing import expand_neutral_nodes, read_edge_clusters, read_graph


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

        graph = read_graph(graph_path)
        assignments = read_edge_clusters(clusters_path, graph)
        expansion = expand_neutral_nodes(graph, assignments)
        expanded = expansion.graph

        check(expansion.neutral_nodes == (3, 4), "unexpected neutral nodes")
        check(expansion.cluster_costs == {1: Decimal("12.5"), 2: Decimal("5")},
              "cluster costs do not use every original record exactly once")
        check(expanded.nodes == 7 and expanded.edge_count == 5 and expanded.arc_count == 5,
              "expanded graph header is inconsistent")
        check(expansion.transition_count == 4,
              "each neutral node needs both directed cluster transitions")

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
        check({record.cost for record in transitions} == {Decimal("17.5")},
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

        output = directory / "graph.tuned.dat"
        mapping = directory / "graph.tuned.mapping.dat"
        result = subprocess.run(
            [sys.executable, TUNER, graph_path, clusters_path, output, mapping],
            capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        reread = read_graph(output)
        check(reread == expanded, "CLI output cannot be read back losslessly")
        check("2 nodos neutros" in result.stdout and "4 transiciones" in result.stdout,
              "CLI summary does not report the expansion")
        mapping_rows = mapping.read_text(encoding="utf-8").splitlines()
        check(mapping_rows[0] == "expanded_node original_node" and
              mapping_rows[-1] == "30 -",
              "CLI did not export the virtual-node mapping")
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


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
