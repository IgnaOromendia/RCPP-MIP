#!/usr/bin/env python3
"""Regression tests for the standalone BFS cluster generator."""

from pathlib import Path
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent.parent
GENERATOR = ROOT / "clusterGeneration" / "generate_clusters.py"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def run_generator(solution: Path, percentage: str, directory: Path, *extra: str):
    return subprocess.run(
        [sys.executable, GENERATOR, solution, "--percentage", percentage, *extra],
        cwd=directory,
        capture_output=True,
        text=True,
        timeout=10,
    )


def main():
    with tempfile.TemporaryDirectory(prefix="cluster-generation-") as raw_directory:
        directory = Path(raw_directory)
        solution = directory / "out_6.dat"
        solution.write_text(
            "OBJ: 12\n\n"
            "---- X ----\n"
            "X_1_2_1 = 1\nX_2_3_1 = 1\nX_3_4_1 = 1\nX_8_9_1 = 0\n"
            "\n---- Y ----\n"
            "Y_1_2_1 = 2\nY_4_5_1 = 1\nY_5_6_1 = 1\nY_8_9_1 = 1\n"
            "\n---- YDK & YKD ----\n"
            "Y_D_1_1 = 1\nY_6_D_1 = 1\n"
            "\n---- F ----\nF_1_2_1 = 0.5\n"
            "\n---- FDK ----\nF_D_1_1 = 0.5\n",
            encoding="utf-8",
        )
        graph = directory / "graph_6.dat"
        graph.write_text(
            "1 6 1 3 0\n1\n"
            "1 2 0 1 0\n2 3 0 1 0\n3 4 0 1 0\n",
            encoding="utf-8",
        )
        coordinates = directory / "graph_6.coords.csv"
        coordinates.write_text(
            "node_id,x,y\n1,0,0\n2,2,0\n3,2,1\n4,0,1\n5,1,2\n6,3,2\n",
            encoding="utf-8",
        )
        result = run_generator(
            solution, "30", directory,
            "--graph", graph, "--coords", coordinates,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        output = directory / "data" / "clusters" / "clusters_6.dat"
        preview = directory / "data" / "clusters" / "clusters_6.svg"
        raster = directory / "data" / "clusters" / "clusters_6.png"
        check(output.exists(), "default clusters_N.dat was not created")
        check(preview.exists(), "default clusters_N.svg was not created")
        check(raster.read_bytes().startswith(b"\x89PNG\r\n\x1a\n"),
              "default clusters_N.png is not a PNG")
        check(ET.parse(preview).getroot().tag == "{http://www.w3.org/2000/svg}svg",
              "cluster preview is not valid SVG")
        preview_text = preview.read_text(encoding="utf-8")
        check("Cluster 1" in preview_text and "pasadas 3" in preview_text,
              "cluster preview lost its legend or edge metadata")
        check('stroke-dasharray="8 6"' in preview_text,
              "edges crossing between clusters are not dotted")
        check('stroke="#d1d5db"' in preview_text,
              "original graph geometry is not drawn in the background")
        lines = [line for line in output.read_text(encoding="utf-8").splitlines()
                 if line and not line.startswith("#")]
        check(lines[0] == "arista cluster origen destino vehiculo servicio recorridos pasadas",
              "invalid output header")
        rows = [line.split() for line in lines[1:]]
        check(len(rows) == 8, "zero-pass edge was not filtered or an active edge was lost")
        check(rows[0][-1] == "3", "X and Y multiplicities were not added")
        clusters = [int(row[1]) for row in rows]
        check(max(clusters.count(cluster) for cluster in set(clusters)) <= 3,
              "cluster exceeded ceil(30% of 8 edges)")
        edge_nodes = {int(row[0]): {row[2], row[3]} for row in rows}
        for cluster in set(clusters):
            members = [int(row[0]) for row in rows if int(row[1]) == cluster]
            reached = {members[0]}
            while True:
                expanded = reached | {
                    candidate for candidate in members
                    if any(edge_nodes[candidate] & edge_nodes[current] for current in reached)
                }
                if expanded == reached:
                    break
                reached = expanded
            check(reached == set(members), f"cluster {cluster} is not edge-connected")

        invalid = run_generator(solution, "0", directory, "--output", "invalid.dat")
        check(invalid.returncode == 1 and "k debe ser" in invalid.stderr,
              "invalid percentage was accepted")
        check(not (directory / "invalid.dat").exists(), "invalid input created output")
        check(not (directory / "invalid.svg").exists(), "invalid input created SVG output")
        check(not (directory / "invalid.png").exists(), "invalid input created PNG output")

        missing_percentage = subprocess.run(
            [sys.executable, GENERATOR, solution], cwd=directory,
            capture_output=True, text=True, timeout=10,
        )
        check(missing_percentage.returncode != 0 and "--percentage" in
              missing_percentage.stderr,
              "missing percentage was accepted")

        incomplete_geometry = run_generator(
            solution, "30", directory, "--graph", graph, "--output", "incomplete.dat"
        )
        check(incomplete_geometry.returncode == 1 and "deben indicarse juntos" in
              incomplete_geometry.stderr,
              "a partial coordinate configuration was accepted")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
