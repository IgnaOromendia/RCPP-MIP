#!/usr/bin/env python3
"""Exercise comparison pipelines without requiring CPLEX."""

import csv
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / "tools/compare_cluster_crossings.py"
sys.path.insert(0, str(ROOT / "tools"))
from compare_cluster_crossings import count_crossings, inputs, prepare_partition
from clusterGeneration.io_impl import read_neutral_nodes


def fixture(directory, name):
    target = directory / name / "streets"
    target.mkdir(parents=True)
    (target / "graph_3.dat").write_text(
        "1 3 1 0 3\n1\n1 2 1 2 1\n2 3 1 3 1\n3 1 1 4 1\n")
    (target / "graph_3.turns.dat").write_text("0 0\n")
    (target / "graph_clusters_3.dat").write_text(
        "# porcentaje_objetivo 34\n"
        "arista cluster origen destino vehiculo servicio recorridos pasadas\n"
        "1 1 1 2 0 0 0 0\n2 2 2 3 0 0 0 0\n3 3 3 1 0 0 0 0\n")
    (target / "graph_3.cluster-neutral-nodes.dat").write_text(
        "node_id reasons\n1 boundary\n2 boundary\n3 boundary\n")


class ComparisonTest(unittest.TestCase):
    def test_shared_route_crossings_use_effective_neighborhoods(self):
        with tempfile.TemporaryDirectory() as raw:
            route = Path(raw) / "route.csv"
            header = "vehiculo,orden,cluster,arista"
            rows = ["1,2,1,1", "1,4,-1,2", "1,6,-1,3", "1,8,2,4"]
            route.write_text("\n".join([header, *rows]) + "\n")
            members = {2: (1, 2), 3: (1, 2)}
            self.assertEqual(count_crossings(route, members), 1)

            effective = [1, 2, 1, 2]
            route.write_text("\n".join([header + ",cluster_efectivo",
                *(f"{row},{side}" for row, side in zip(rows, effective))]) + "\n")
            self.assertEqual(count_crossings(route, members), 3)

    def test_partition_completes_missing_neutral_boundaries(self):
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            fixture(directory, "martinezA")
            nodes, paths = inputs(directory, "martinezA", "streets")
            original = paths["neutral"].read_text().replace("2 boundary\n", "")
            paths["neutral"].write_text(original)
            prepared = prepare_partition(nodes, paths, directory / "prepared")
            neutral = read_neutral_nodes(prepared["neutral"], nodes)
            self.assertEqual(set(neutral), {1, 2, 3})
            self.assertEqual(neutral[1], ("boundary",))
            self.assertEqual(neutral[2], ("cluster_boundary",))
            self.assertEqual(paths["neutral"].read_text(), original)
            self.assertNotEqual(prepared["neutral"], paths["neutral"])

    def test_partition_still_rejects_invalid_records(self):
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            fixture(directory, "martinezA")
            nodes, paths = inputs(directory, "martinezA", "streets")
            original = paths["clusters"].read_text()
            for index, replacement in enumerate((
                    "1 1 1 2 1 1 0 1",  # Actual service must not be erased.
                    "1 1 1 2 1 0 1 1",  # Actual deadhead must not be erased.
                    "1 1 1 2 2 0 0 0",  # Unknown historical vehicle.
                    "1 1 2 1 1 0 0 0",  # Incorrect physical endpoints.
                    "2 1 1 2 1 0 0 0",  # Duplicate/missing graph record.
            )):
                with self.subTest(replacement=replacement):
                    paths["clusters"].write_text(original.replace("1 1 1 2 0 0 0 0", replacement))
                    with self.assertRaises(ValueError):
                        prepare_partition(nodes, paths, directory / f"invalid-{index}")

    def test_comparison_and_failed_second_map(self):
        with tempfile.TemporaryDirectory() as raw:
            directory = Path(raw)
            for name in ("martinezA", "martinezB"):
                fixture(directory / "data", name)
            legacy = directory / "data/martinezA/streets/graph_clusters_3.dat"
            legacy_contents = legacy.read_text().replace("0 0 0 0\n", "1 0 0 0\n")
            legacy.write_text(legacy_contents)
            binaries = directory / "bin"
            binaries.mkdir()
            scripts = {
                "solverExec": '''from pathlib import Path
p = Path("data/3/min_dist_3.dat")
p.parent.mkdir(parents=True)
p.write_text("OBJ: 9\\n\\n---- X ----\\nX_1_2_1 = 1\\nX_3_4_1 = 1\\nX_5_6_1 = 1\\n")
print("RCPP_RESULT elapsed_ms=1 has_solution=true optimal=true objective=9")
''',
                "pathSortClusterExec": '''import sys
from pathlib import Path
assert "--expanded-graph" not in sys.argv
assert "arista cluster" in Path(sys.argv[4]).read_text()
Path(sys.argv[6]).write_text("vehiculo,orden,nodo_origen,nodo_destino,cluster\\n"
    "1,8,2,3,2\\n1,2,1,2,1\\n1,6,3,1,1\\n1,4,2,3,2\\n")
print("CPLEX no obtuvo un orden optimizado; se usa el orden factible de Hierholzer.")
''',
                "bestClusterPathExec": '''import sys
from pathlib import Path
partition = Path(sys.argv[sys.argv.index("--clusters") + 1]).read_text()
assert all(line.split()[4:] == ["0"] * 4
           for line in partition.splitlines() if line and not line.startswith(("#", "arista")))
if "martinezB" in sys.argv[1]:
    print("BEST_CLUSTER_PATH_RESULT elapsed_ms=1 has_solution=false optimal=false objective=NA crossings=NA distance=NA")
    sys.exit(2)
print("BEST_CLUSTER_PATH_RESULT elapsed_ms=1 has_solution=true optimal=false objective=1 crossings=1 distance=9")
''',
            }
            for name, script in scripts.items():
                path = binaries / name
                path.write_text(f"#!{sys.executable}\n" + script)
                path.chmod(0o755)
            output = directory / "results"
            command = [sys.executable, RUNNER, "--skip-build", "--data-root",
                       directory / "data", "--bin-dir", binaries,
                       "--output-dir", output]
            result = subprocess.run(command, capture_output=True, text=True, timeout=15)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertTrue((output / "summary.csv").exists(), result.stdout + result.stderr)
            with (output / "summary.csv").open() as source:
                rows = list(csv.DictReader(source))
            self.assertEqual(len(rows), 2)
            self.assertEqual(rows[0]["sorter_crossings"], "3", result.stdout + result.stderr)
            self.assertEqual(rows[0]["best_crossings"], "1")
            self.assertEqual(rows[0]["reduction"], "2")
            self.assertEqual(rows[0]["sorter_source"], "hierholzer")
            self.assertEqual(rows[0]["best_optimal"], "false")
            self.assertEqual(legacy.read_text(), legacy_contents)
            self.assertEqual(rows[1]["sorter_crossings"], "3")
            self.assertEqual(rows[1]["best_crossings"], "")
            self.assertIn("bestClusterPath", rows[1]["error"])
            # Reusing an output directory must not turn stale files into results.
            repeated = subprocess.run(command, capture_output=True, text=True, timeout=15)
            self.assertNotEqual(repeated.returncode, 0)
            self.assertEqual((output / "summary.csv").read_text().count("martinezA"), 1)

            # A killed solver cannot contribute crossings or reuse the previous run.
            (binaries / "bestClusterPathExec").write_text(
                f"#!{sys.executable}\nimport time\ntime.sleep(5)\n")
            timed_output = directory / "timed-results"
            command[command.index("--output-dir") + 1] = timed_output
            command += ["--maps", "martinezA", "--timeout", "1"]
            timed = subprocess.run(command, capture_output=True, text=True, timeout=15)
            self.assertEqual(timed.returncode, 1, timed.stdout + timed.stderr)
            with (timed_output / "summary.csv").open() as source:
                timed_row = next(csv.DictReader(source))
            self.assertEqual(timed_row["sorter_crossings"], "3")
            self.assertEqual(timed_row["best_crossings"], "")
            self.assertIn("timeout", timed_row["error"])


if __name__ == "__main__":
    unittest.main()
