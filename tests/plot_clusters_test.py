#!/usr/bin/env python3
"""Regression test for the interactive OSM cluster map."""

from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
PLOTTER = ROOT / "generator" / "plot_clusters.py"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    with tempfile.TemporaryDirectory(prefix="plot-clusters-") as raw_directory:
        directory = Path(raw_directory)
        coordinates = directory / "coords.csv"
        coordinates.write_text(
            "node_id,x,y\n"
            "1,-34.50,-58.50\n"
            "2,-34.50,-58.49\n"
            "3,-34.49,-58.49\n"
            "4,-34.49,-58.50\n"
            "5,-34.48,-58.50\n",
            encoding="utf-8",
        )
        clusters = directory / "clusters.dat"
        clusters.write_text(
            "# porcentaje_objetivo 10\n"
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n"
            "1 1 1 2 1 0 0 0\n"
            "2 2 2 3 1 0 0 0\n"
            "3 2 3 4 1 0 0 0\n"
            "4 2 4 5 1 0 0 0\n",
            encoding="utf-8",
        )
        graph = directory / "graph.dat"
        graph.write_text(
            "1 5 1 4 0\n1\n"
            "1 2 -1 1 1\n"
            "2 3 0 1 0\n"
            "3 4 -1 1 1\n"
            "4 5 0 1 0\n",
            encoding="utf-8",
        )
        neutral_nodes = directory / "neutral.dat"
        neutral_nodes.write_text(
            "node_id reasons\n"
            "2 avenida,primary\n"
            "3 divider\n"
            "4 divider\n"
            "5 avenida\n",
            encoding="utf-8",
        )
        output = directory / "clusters.html"
        result = subprocess.run(
            [sys.executable, PLOTTER, "--clusters", clusters, "--graph", graph,
             "--coords", coordinates, "--neutral-nodes", neutral_nodes,
             "--output", output],
            cwd=ROOT, capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        html = output.read_text(encoding="utf-8")
        check("leaflet" in html.lower() and "OpenStreetMap" in html,
              "the output is not an interactive Leaflet map")
        check("Cluster 1" in html, "cluster labels are missing")
        check("Nodo neutro 2" in html and "avenida, primary" in html,
              "neutral-node metadata is missing")
        check("Nodos neutros" in html and "Clusters" in html,
              "interactive layers are missing")
        check("Calles divisorias" in html and "Calle divisoria" in html,
              "divider streets are not highlighted")
        check("Tramo 2" not in html and "Tramo 4" not in html,
              "optional-zone roads are visible in the cluster map")
        check("Nodo neutro 5" not in html,
              "a neutral node used only by optional roads is visible")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
