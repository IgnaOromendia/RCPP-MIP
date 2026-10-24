"""Regression checks for experiment 1, without network or CPLEX."""

from pathlib import Path
import importlib.util
import os
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import distance_time_experiment as experiment


class DistanceTimeExperimentTest(unittest.TestCase):
    def test_only_best_cluster_path_has_clustering_variants(self):
        self.assertEqual(
            experiment.CONFIGURATIONS,
            (("mip", "none"),
             ("fixAndOptimize:random", "none"),
             ("fixAndOptimize:maxDeadheadCost", "none"),
             ("fixAndOptimize:topKDeadheadCost", "none"),
             ("bestClusterPath", "bfs"),
             ("bestClusterPath", "streets")))

    def test_csv_survives_interruption_and_resumes_without_plots(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "solver"
            binary.write_text("#!/bin/sh\nexit 0\n")
            binary.chmod(0o755)
            source = experiment.source_paths(root / "data", "small")
            for path in source.values():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("input")
            source["graph"].write_text("1 2 1 1 0\n1\n1 2 -1 1 1\n")
            instance = {key: source["graph"] for key in
                        ("graph", "turns", "in_out", "arc_clusters")}
            instance.update(nodes=3, edges=3, required_edges=2)
            def prepare(source, target, clustering, percentage):
                target.mkdir(parents=True, exist_ok=True)
                return instance
            measurement = dict(elapsed_ms="12", wall_ms="15", has_solution="true",
                               optimal="true", returncode=0, outcome="completed",
                               objective="10", model_objective="10", crossings="")

            def measure(command, target, model):
                target.mkdir(parents=True, exist_ok=True)
                (target / "solver.log").write_text("completed")
                return measurement.copy()

            arguments = ["demo", "--maps", "small", "--data-root", str(root / "data"),
                         "--solver", str(binary), "--best-solver", str(binary),
                         "--no-plots"]
            with mock.patch.object(experiment, "ROOT", root), \
                    mock.patch.object(experiment, "prepare_instance", side_effect=prepare), \
                    mock.patch.object(experiment, "plot_results") as plot:
                calls = 0
                def interrupt(command, target, model):
                    nonlocal calls
                    calls += 1
                    if calls == 2:
                        raise KeyboardInterrupt
                    return measure(command, target, model)
                with mock.patch.object(experiment, "measure", side_effect=interrupt):
                    with self.assertRaises(KeyboardInterrupt):
                        experiment.main(arguments)
                csv_path = root / "experiments/demo/resultados.csv"
                self.assertEqual(len(experiment.read_rows(csv_path)), 1)
                with mock.patch.object(experiment, "measure", side_effect=measure) as run:
                    experiment.main(arguments)
                    self.assertEqual(run.call_count, 5)
                    self.assertEqual(len(experiment.read_rows(csv_path)), 6)
                    experiment.main(arguments)
                    self.assertEqual(run.call_count, 5)
                plot.assert_not_called()

    def test_rejects_conflicting_plot_options(self):
        with self.assertRaisesRegex(ValueError, "no se pueden combinar"):
            experiment.main(["--plot-only", "--no-plots"])

    def test_best_cluster_result_and_no_solution(self):
        with tempfile.TemporaryDirectory() as directory:
            for model, prefix in (("mip", "RCPP_RESULT"),
                                  ("bestClusterPath", "BEST_CLUSTER_PATH_RESULT")):
                output = f"{prefix} elapsed_ms=123 has_solution=false optimal=false objective=NA"
                if model == "bestClusterPath":
                    output += " crossings=NA distance=NA"
                with mock.patch.object(experiment.subprocess, "run", return_value=
                                       subprocess.CompletedProcess([], 2, output, "")):
                    row = experiment.measure(["solver"], Path(directory), model)
                self.assertEqual(row["outcome"], "completed")
                self.assertEqual(row["elapsed_ms"], "123.000000")
                self.assertEqual(row["has_solution"], "false")
            with mock.patch.object(experiment.subprocess, "run", return_value=
                                   subprocess.CompletedProcess([], 1, output, "error")):
                row = experiment.measure(["solver"], Path(directory), "bestClusterPath")
            self.assertEqual(row["outcome"], "error")

    def test_tuner_and_partition_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source = experiment.source_paths(base, "small")
            for path in source.values():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("")
            source["graph"].write_text("1 3 1 1 1\n1\n1 2 -1 1 1\n2 3 0 1 0\n")
            source["coordinates"].write_text("100 0 0\n200 0 1\n300 1 1\n")
            source["mapping"].write_text("100 1\n200 2\n300 3\n")
            for strategy in ("bfs", "streets"):
                with mock.patch.object(experiment, "run_command") as run:
                    instance = experiment.prepare_instance(source, base / strategy, strategy, 20)
                self.assertEqual(instance["edges"], 2)
                self.assertEqual(instance["required_edges"], 1)
                cluster, tune = [call.args[0] for call in run.call_args_list]
                self.assertEqual(cluster[cluster.index("--strategy") + 1], strategy)
                self.assertIn(ROOT / "tune_cluster_crossing.py", tune)
                self.assertEqual("--neutral-nodes" in tune, strategy == "streets")
                for model in experiment.MODELS:
                    command = experiment.solver_command(model, instance, Path("solver"), Path("best"))
                    self.assertEqual(command[1:3], [instance["graph"], instance["turns"]])
                    if model == "bestClusterPath":
                        self.assertEqual(command[3:], [instance["in_out"], instance["arc_clusters"]])

    @unittest.skipUnless(importlib.util.find_spec("matplotlib"), "matplotlib no instalado")
    def test_full_runner_resume_retry_and_plot_only(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            os.environ["MPLCONFIGDIR"] = str(root / ".matplotlib")
            # Import before replacing subprocess.run, also used by font discovery.
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot
            data = root / "data/generator"
            for name in ("small", "large"):
                for path in experiment.source_paths(data, name).values():
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("")
                experiment.source_paths(data, name)["graph"].write_text(
                    "1 2 1 1 0\n1\n1 2 -1 1 1\n")
            binary = root / "solver"
            binary.write_text("#!/bin/sh\nexit 0\n")
            binary.chmod(0o755)
            def prepare(source, target, clustering, percentage):
                target.mkdir(parents=True, exist_ok=True)
                files = {key: target / key for key in ("graph", "turns", "in_out", "arc_clusters")}
                for path in files.values():
                    path.write_text("input")
                return dict(files, nodes=4, edges=6 if "large" in str(target) else 3,
                            required_edges=3)
            def solve(command, **kwargs):
                prefix = "BEST_CLUSTER_PATH_RESULT" if len(command) == 5 and command[-1].endswith("arc_clusters") else "RCPP_RESULT"
                suffix = " crossings=1 distance=10" if prefix == "BEST_CLUSTER_PATH_RESULT" else ""
                return subprocess.CompletedProcess(command, 0,
                    f"{prefix} elapsed_ms=20 has_solution=true optimal=true objective=1{suffix}\n", "")
            arguments = ["demo", "--maps", "large", "small", "--data-root", str(data),
                         "--solver", str(binary), "--best-solver", str(binary)]
            with mock.patch.object(experiment, "ROOT", root), \
                    mock.patch.object(experiment, "prepare_instance", side_effect=prepare), \
                    mock.patch.object(experiment.subprocess, "run", side_effect=solve) as run:
                experiment.main(arguments)
                self.assertEqual(run.call_count, 12)
                output = root / "experiments/demo"
                self.assertEqual(len(experiment.read_rows(output / "resultados.csv")), 12)
                self.assertEqual(len(list(output.glob("*.png"))), 2)
                experiment.main(arguments)
                self.assertEqual(run.call_count, 12)
                # Errors are retried; successful configurations are resumed.
                rows = experiment.read_rows(output / "resultados.csv")
                failed = dict(rows[0], outcome="error")
                experiment.save_row(output / "resultados.csv", rows, failed)
                experiment.main(arguments)
                self.assertEqual(run.call_count, 13)
                experiment.main(arguments + ["--plot-only"])
                self.assertEqual(run.call_count, 13)
                binary.write_text("#!/bin/sh\n# changed\nexit 0\n")
                experiment.main(arguments)
                self.assertEqual(run.call_count, 25)


if __name__ == "__main__":
    unittest.main()
