#!/usr/bin/env python3
"""Regression tests for street-divider clustering and its file adapters."""

from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
GENERATOR = ROOT / "clusterGeneration" / "generate_clusters.py"
sys.path.insert(0, str(GENERATOR.parent))

from generate_clusters import (Edge, CoordinateGeometry,
                               straight_divider_clusters,
                               read_osm_street_names)
from divider_impl import _local_components


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
    # Two neutral streets delimit one connected region.  The named street
    # through its centre reaches the opposite boundary and becomes the divider.
    divider_geometry = CoordinateGeometry(
        node_count=9,
        coordinates={
            "1": (0.0, 0.0), "2": (0.0, 2.0), "3": (0.0, 4.0),
            "4": (6.0, 0.0), "5": (6.0, 2.0), "6": (6.0, 4.0),
            "7": (3.0, 0.0), "8": (3.0, 2.0), "9": (3.0, 4.0),
        },
        original_edges=[
            ("1", "2"), ("2", "3"), ("4", "5"), ("5", "6"),
            ("1", "7"), ("7", "4"), ("2", "8"), ("8", "5"),
            ("3", "9"), ("9", "6"), ("7", "8"), ("8", "9"),
        ],
        virtual_to_original={}, virtual_to_edge={}, deposit_adjacent="1",
    )
    divider_names = {
        ("1", "2"): "limite oeste", ("2", "3"): "limite oeste",
        ("4", "5"): "limite este", ("5", "6"): "limite este",
        ("1", "7"): "norte", ("4", "7"): "norte",
        ("2", "8"): "central", ("5", "8"): "central",
        ("3", "9"): "sur", ("6", "9"): "sur",
        ("7", "8"): "eje", ("8", "9"): "eje",
    }
    divider_edges, divider_assignments, _, effective_neutral, divider_count = (
        straight_divider_clusters(
            divider_geometry, 40,
            {
                1: ("avenida",), 2: ("avenida",), 3: ("avenida",),
                4: ("primary",), 5: ("primary",), 6: ("primary",),
            },
            divider_names,
        )
    )
    by_endpoints = {
        (edge.source, edge.target): cluster
        for edge, cluster in zip(divider_edges, divider_assignments)
    }
    top_cluster = by_endpoints[("1", "7")]
    bottom_cluster = by_endpoints[("3", "9")]
    check(divider_count == 1 and {2, 5, 8}.issubset(effective_neutral),
          "the centre street reaching the opposite boundary was not neutralized")
    check("divider" in effective_neutral[2] and
          "divider" in effective_neutral[5] and
          "divider" in effective_neutral[8],
          "divider nodes did not retain their reason")
    check("divider_1" in effective_neutral[2] and
          "divider_1" in effective_neutral[5] and
          "divider_1" in effective_neutral[8],
          "divider nodes did not retain their path identifier")
    check(top_cluster != bottom_cluster and
          by_endpoints[("7", "8")] == top_cluster and
          by_endpoints[("8", "9")] == bottom_cluster,
          "the centre street did not split the region into two clusters")

    endpoint_geometry = CoordinateGeometry(
        node_count=5,
        coordinates={
            "1": (0.0, 0.0), "2": (0.0, 1.0), "3": (1.0, 1.0),
            "4": (2.0, 0.0), "5": (2.0, 2.0),
        },
        original_edges=[("1", "2"), ("2", "3"), ("3", "4"), ("3", "5")],
        virtual_to_original={}, virtual_to_edge={}, deposit_adjacent="1",
    )
    _, _, endpoint_maximum, endpoint_neutral, endpoint_dividers = (
        straight_divider_clusters(
            endpoint_geometry, 34, {1: ("avenida",), 2: ("avenida",)},
            {
                ("1", "2"): "avenida limite", ("2", "3"): "necochea",
                ("3", "4"): "oeste", ("3", "5"): "este",
            },
        )
    )
    check(endpoint_dividers == 1 and endpoint_maximum == 1 and
          "divider_1" in endpoint_neutral[3],
          "a straight street endpoint that splits the cluster was rejected")

    motorway_only = {node: ("motorway",) for node in range(1, 7)}
    _, motorway_assignments, _, motorway_neutral, motorway_dividers = (
        straight_divider_clusters(
            divider_geometry, 100, motorway_only, divider_names
        )
    )
    check(motorway_dividers == 0 and len(set(motorway_assignments)) == 1 and
          not motorway_neutral,
          "a road other than an avenue or primary seeded the neutral network")
    try:
        straight_divider_clusters(divider_geometry, 40, motorway_only, divider_names)
    except ValueError as error:
        check("no existe una calle recta" in str(error),
              "missing straight divider produced an unrelated error")
    else:
        raise RuntimeError("clustering without a straight neutral divider succeeded")

    zoned_geometry = CoordinateGeometry(
        node_count=divider_geometry.node_count,
        coordinates=divider_geometry.coordinates,
        original_edges=divider_geometry.original_edges,
        virtual_to_original={}, virtual_to_edge={}, deposit_adjacent="1",
        zones=[-1] * 6 + [0] * 6,
    )
    _, zoned_assignments, zoned_maximum, _, _ = straight_divider_clusters(
        zoned_geometry, 100, {}, divider_names
    )
    check(zoned_maximum <= 6 and len(zoned_assignments) == 12,
          "required clusters exceeded their limit or optional edges were lost")

    ladder_edges = [
        Edge(1, "1", "2", 0, 0, 0),
        Edge(2, "3", "4", 0, 0, 0),
        Edge(3, "2", "3", 0, 0, 0),
    ]
    ladder_components = _local_components(
        ladder_edges, {"1", "2", "3", "4"},
        boundary_positions={0, 1},
    )
    check(ladder_components == [{2}],
          "a cross street between divider paths became a boundary")

    with tempfile.TemporaryDirectory(prefix="cluster-generation-") as raw_directory:
        directory = Path(raw_directory)
        osm = directory / "streets.osm"
        osm.write_text(
            '<osm><way id="1"><tag k="name" v="Calle Continua"/>'
            '<nd ref="100"/><nd ref="200"/></way></osm>', encoding="utf-8"
        )
        node_mapping = directory / "nodes.dat"
        node_mapping.write_text("100 1\n200 2\n", encoding="utf-8")
        name_geometry = CoordinateGeometry(
            node_count=2, coordinates={}, original_edges=[("1", "2")],
            virtual_to_original={}, virtual_to_edge={}, deposit_adjacent="1",
        )
        check(
            read_osm_street_names(osm, node_mapping, name_geometry)
            == {("1", "2"): "calle continua"},
            "OSM street names were not mapped to internal graph nodes",
        )
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
            "1 2 -1 1 0\n2 3 -1 1 0\n3 4 -1 1 0\n",
            encoding="utf-8",
        )
        coordinates = directory / "graph_6.coords.csv"
        coordinates.write_text(
            "node_id,x,y\n1,0,0\n2,2,0\n3,2,1\n4,0,1\n5,1,2\n6,3,2\n",
            encoding="utf-8",
        )
        pre_solver_output = directory / "pre-solver.dat"
        neutral_nodes = directory / "neutral-nodes.dat"
        neutral_nodes.write_text(
            "node_id reasons\n2 avenida,primary\n", encoding="utf-8"
        )
        pre_solver = subprocess.run(
            [sys.executable, GENERATOR, "--graph-only", "--percentage", "34",
             "--graph", graph, "--coords", coordinates,
             "--neutral-nodes", neutral_nodes, "--output", pre_solver_output],
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
        check("# nodos_neutros_forzados 1" in pre_solver_output.read_text(),
              "pre-solver output did not record its forced neutral nodes")
        check("# aristas_objetivo_por_cluster 1" in pre_solver_output.read_text(),
              "cluster size was not derived from the input edge count")
        check(not pre_solver_output.with_suffix(".svg").exists() and
              not pre_solver_output.with_suffix(".png").exists(),
              "pre-solver clustering created a static visualization")

        existing_partition = directory / "existing-partition.dat"
        existing_partition.write_text(
            "# porcentaje_objetivo 17\n"
            "arista cluster origen destino vehiculo servicio recorridos pasadas\n"
            "1 7 1 2 1 0 0 0\n"
            "2 2 2 3 1 0 0 0\n"
            "3 7 3 4 1 0 0 0\n",
            encoding="utf-8",
        )
        projected_output = directory / "projected.dat"
        projected = subprocess.run(
            [sys.executable, GENERATOR, solution,
             "--graph", graph, "--graph-clusters", existing_partition,
             "--output", projected_output],
            cwd=directory, capture_output=True, text=True, timeout=10,
        )
        check(projected.returncode == 0, projected.stdout + projected.stderr)
        projected_rows = [
            line.split()
            for line in projected_output.read_text(encoding="utf-8").splitlines()
            if line and not line.startswith("#")
        ][1:]
        projected_clusters = {
            (row[2], row[3]): int(row[1]) for row in projected_rows
        }
        check(projected_clusters[("1", "2")] == 7 and
              projected_clusters[("3", "4")] == 7 and
              projected_clusters[("5", "6")] == 2 and
              projected_clusters[("9", "10")] == 7,
              "the pre-solver partition was recomputed instead of projected")
        check(projected_clusters[("2", "5")] == 2 and
              projected_clusters[("6", "9")] == 7 and
              projected_clusters[("D", "1")] == 7 and
              projected_clusters[("10", "D")] == 7,
              "connectors did not inherit the existing graph partition")
        projected_comments = projected_output.read_text(encoding="utf-8")
        check("# porcentaje_objetivo 17" in projected_comments and
              "# max_aristas_por_cluster 2" in projected_comments,
              "metadata from the pre-solver partition was not preserved")

        missing_graph = run_generator(
            solution, "30", directory, "--output", "missing-graph.dat"
        )
        check(missing_graph.returncode != 0 and "--graph" in missing_graph.stderr,
              "clustering without the required original graph was accepted")

if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
