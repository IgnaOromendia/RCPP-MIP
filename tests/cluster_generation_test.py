#!/usr/bin/env python3
"""Regression tests for the standalone BFS cluster generator."""

from pathlib import Path
import math
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parent.parent
GENERATOR = ROOT / "clusterGeneration" / "generate_clusters.py"
sys.path.insert(0, str(GENERATOR.parent))

from generate_clusters import Edge, CoordinateGeometry, bfs_clusters, coordinate_layout


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
    path_edges = [
        Edge(index, str(index), str(index + 1), 0, 0, 0)
        for index in range(1, 8)
    ]
    check(bfs_clusters(path_edges, 40) == [1, 1, 1, 2, 2, 2, 2],
          "a neighboring 3:1 cluster imbalance was not absorbed")
    check(bfs_clusters(path_edges[:5], 60) == [1, 1, 1, 2, 2],
          "clusters differing by less than 2:1 were merged")
    disconnected_edges = path_edges[:3] + [Edge(4, "10", "11", 0, 0, 0)]
    check(bfs_clusters(disconnected_edges, 75) == [1, 1, 1, 2],
          "disconnected clusters were merged by size")

    map_geometry = CoordinateGeometry(
        node_count=3,
        coordinates={"1": (1000.0, 2000.0), "2": (4000.0, 2000.0),
                     "3": (2500.0, 5000.0)},
        original_edges=[("1", "2"), ("2", "3")],
        virtual_to_original={},
        virtual_to_edge={},
        deposit_adjacent="1",
    )
    map_positions = coordinate_layout(map_geometry)
    deposit_distance = math.dist(map_positions["D"], map_positions["1"])
    check(math.isclose(deposit_distance, 18.0),
          "map depot was not placed with a fixed visual offset")
    check(map_positions["1"] == (50.0, 90.0) and map_positions["3"] == (390.0, 770.0),
          "synthetic depot changed the projection of real map coordinates")

    with tempfile.TemporaryDirectory(prefix="cluster-generation-") as raw_directory:
        directory = Path(raw_directory)
        solution = directory / "min_dist_6.dat"
        solution.write_text(
            "OBJ: 12\n\n"
            "---- X ----\n"
            "X_1_2_1 = 1\nX_3_4_1 = 1\nX_5_6_1 = 1\nX_9_10_1 = 1\n"
            "\n---- Y ----\n"
            "Y_1_2_1 = 2\nY_2_5_1 = 1\nY_6_9_1 = 1\nY_8_9_1 = 0\n"
            "\n---- YDK & YKD ----\n"
            "Y_D_1_1 = 1\nY_10_D_1 = 1\n"
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
        output = directory / "data" / "6" / "clusters_6.dat"
        preview = directory / "data" / "6" / "clusters_6.svg"
        raster = directory / "data" / "6" / "clusters_6.png"
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
        cluster_by_arc = {(row[2], row[3]): int(row[1]) for row in rows}
        check(cluster_by_arc[("1", "2")] == cluster_by_arc[("3", "4")],
              "the two orientations of one original edge received different clusters")
        check(len({cluster_by_arc[("1", "2")], cluster_by_arc[("5", "6")],
                   cluster_by_arc[("9", "10")]}) == 3,
              "the percentage was not applied to the three original edges")
        check(cluster_by_arc[("2", "5")] == cluster_by_arc[("5", "6")],
              "turn connector did not inherit the entered original edge cluster")
        check(cluster_by_arc[("6", "9")] == cluster_by_arc[("9", "10")],
              "cross-cluster turn connector did not inherit the next edge cluster")
        check(cluster_by_arc[("D", "1")] == cluster_by_arc[("1", "2")],
              "deposit departure did not inherit the next edge cluster")
        check(cluster_by_arc[("10", "D")] == cluster_by_arc[("9", "10")],
              "deposit arrival did not inherit the previous edge cluster")
        comments = output.read_text(encoding="utf-8")
        check("# aristas_originales_activas 3" in comments and
              "# aristas_supergrafo_activas 8" in comments and
              "# aristas_objetivo_por_cluster 1" in comments and
              "# factor_absorcion_clusters 2" in comments and
              "# max_aristas_por_cluster 1" in comments,
              "cluster metadata is not expressed in original-graph edges")

        invalid = run_generator(
            solution, "0", directory, "--graph", graph, "--output", "invalid.dat"
        )
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

        graph_only = run_generator(
            solution, "30", directory, "--graph", graph, "--output", "graph-only.dat"
        )
        check(graph_only.returncode == 0, graph_only.stdout + graph_only.stderr)
        check((directory / "graph-only.dat").exists() and
              (directory / "graph-only.svg").exists(),
              "clustering without optional coordinates did not produce output")

        pre_solver_output = directory / "pre-solver.dat"
        pre_solver = subprocess.run(
            [sys.executable, GENERATOR, "--graph-only", "--percentage", "30",
             "--graph", graph, "--output", pre_solver_output],
            cwd=directory, capture_output=True, text=True, timeout=10,
        )
        check(pre_solver.returncode == 0, pre_solver.stdout + pre_solver.stderr)
        pre_solver_rows = [
            line.split() for line in pre_solver_output.read_text(encoding="utf-8").splitlines()
            if line and not line.startswith("#")
        ][1:]
        check(len(pre_solver_rows) == 3,
              "pre-solver clustering did not include every original graph edge")
        check([(row[2], row[3]) for row in pre_solver_rows] ==
              [("1", "2"), ("2", "3"), ("3", "4")],
              "pre-solver cluster rows are not aligned with graph record IDs")
        check(all(row[4:] == ["1", "0", "0", "0"] for row in pre_solver_rows),
              "pre-solver clusters have unexpected passage metadata")

        missing_graph = run_generator(
            solution, "30", directory, "--output", "missing-graph.dat"
        )
        check(missing_graph.returncode != 0 and "--graph" in missing_graph.stderr,
              "clustering without the required original graph was accepted")

        mixed_solution = directory / "min_dist_3.dat"
        mixed_solution.write_text(
            "---- X ----\n"
            "X_1_2_1 = 1\nX_3_4_1 = 1\nX_5_6_1 = 1\n"
            "---- Y ----\n"
            "Y_2_5_1 = 1\n"
            "---- YDK & YKD ----\n"
            "Y_D_1_1 = 1\nY_6_D_1 = 1\n",
            encoding="utf-8",
        )
        mixed_graph = directory / "graph_3.dat"
        mixed_graph.write_text(
            "1 3 1 1 1\n1\n"
            "1 2 -1 1 1\n"
            "2 3 -1 1 1\n",
            encoding="utf-8",
        )
        mixed_output = directory / "mixed.dat"
        mixed = run_generator(
            mixed_solution, "50", directory,
            "--graph", mixed_graph, "--output", mixed_output,
        )
        check(mixed.returncode == 0, mixed.stdout + mixed.stderr)
        mixed_rows = [
            line.split() for line in mixed_output.read_text(encoding="utf-8").splitlines()
            if line and not line.startswith("#")
        ][1:]
        mixed_clusters = {(row[2], row[3]): int(row[1]) for row in mixed_rows}
        check(mixed_clusters[("1", "2")] == mixed_clusters[("3", "4")],
              "mixed graph lost the pair of an undirected original edge")
        check(mixed_clusters[("5", "6")] != mixed_clusters[("1", "2")],
              "directed arc was mapped to the wrong global original edge")
        check(mixed_clusters[("2", "5")] == mixed_clusters[("5", "6")],
              "turn connector did not inherit the directed original arc cluster")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
