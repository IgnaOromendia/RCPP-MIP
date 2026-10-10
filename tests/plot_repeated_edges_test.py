#!/usr/bin/env python3
"""Regression tests for the repeated-edge plotter."""

from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent.parent
PLOTTER = ROOT / "tools" / "plot_repeated_edges.py"
sys.path.insert(0, str(ROOT / "tools"))

from plot_repeated_edges import passages_by_original_edge

sys.path.insert(0, str(ROOT / "clusterGeneration"))
from generate_clusters import read_coordinate_geometry, read_solution


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    with tempfile.TemporaryDirectory(prefix="repeated-edge-plot-") as raw_directory:
        directory = Path(raw_directory)
        solution = directory / "out_3.dat"
        solution.write_text(
            "---- X ----\n"
            "X_1_2_1 = 1\nX_5_6_1 = 1\n"
            "---- Y ----\n"
            "Y_3_4_1 = 1\nY_2_5_1 = 8\n"
            "---- YDK & YKD ----\n"
            "Y_D_1_1 = 1\nY_6_D_1 = 1\n",
            encoding="utf-8",
        )
        graph = directory / "graph_3.dat"
        graph.write_text(
            "1 3 1 1 1\n1\n"
            "1 2 -1 1 1\n"
            "2 3 -1 1 1\n",
            encoding="utf-8",
        )
        coordinates = directory / "graph_3.coords.csv"
        coordinates.write_text(
            "node_id,x,y\n1,0,0\n2,1,0\n3,2,1\n",
            encoding="utf-8",
        )

        geometry = read_coordinate_geometry(graph, coordinates)
        passages = passages_by_original_edge(read_solution(solution), geometry)
        check(passages == {0: 2, 1: 1},
              "passages were not aggregated over orientations or a turn was counted")

        output = directory / "plots" / "repeated.svg"
        result = subprocess.run(
            [sys.executable, PLOTTER, solution, "--graph", graph,
             "--coords", coordinates, "--output", output],
            cwd=directory, capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        png = output.with_suffix(".png")
        check(output.exists(), "SVG plot was not created")
        check(png.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"),
              "PNG plot was not created")
        check(ET.parse(output).getroot().tag == "{http://www.w3.org/2000/svg}svg",
              "plot is not valid SVG")
        svg = output.read_text(encoding="utf-8")
        check('class="repeated" data-edge-id="0" data-passages="2"' in svg and
              'stroke="#dc2626"' in svg,
              "repeated original edge was not marked in red")
        check('class="single" data-edge-id="1" data-passages="1"' in svg and
              'stroke="#2563eb"' in svg,
              "single-pass original edge was not distinguished")
        check("×2" in svg and "Repetidas: 1" in svg,
              "passage label or summary was not written")

        no_coordinates = directory / "without-coordinates.svg"
        result = subprocess.run(
            [sys.executable, PLOTTER, solution, "--graph", graph,
             "--output", no_coordinates],
            cwd=directory, capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        check(no_coordinates.exists() and no_coordinates.with_suffix(".png").exists(),
              "plot without optional coordinates was not created")

        invalid_output = directory / "invalid.jpg"
        result = subprocess.run(
            [sys.executable, PLOTTER, solution, "--graph", graph,
             "--output", invalid_output],
            cwd=directory, capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 1 and "--output debe terminar" in result.stderr,
              "invalid output extension was accepted")
        check(not invalid_output.exists(), "invalid output created a file")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
