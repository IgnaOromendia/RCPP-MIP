#!/usr/bin/env python3
"""Regression tests for the pre-solver cluster-crossing cost tuner."""

from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
TUNER = ROOT / "tune_cluster_crossings.py"
sys.path.insert(0, str(ROOT))

from tune_cluster_crossings import read_edge_clusters, read_graph, tune_crossings


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
            "3 4 0 2 0\n"
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
        tuned, crossing_ids, crossing_cost = tune_crossings(graph, assignments)
        check(crossing_ids == [3, 5], f"unexpected crossing records: {crossing_ids}")
        check(crossing_cost == 14, "crossing cost is not twice the graph maximum")
        check([record.cost for record in tuned.records] == [1.5, 7, 14, 3, 14],
              "non-crossing or crossing costs were tuned incorrectly")

        output = directory / "graph.tuned.dat"
        result = subprocess.run(
            [sys.executable, TUNER, graph_path, clusters_path, output],
            capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        reread = read_graph(output)
        check([record.cost for record in reread.records] == [1.5, 7, 14, 3, 14],
              "CLI output cannot be read back with the expected costs")
        check("2 cruces" in result.stdout and "costo 14" in result.stdout,
              "CLI summary does not report tuned crossings")
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
