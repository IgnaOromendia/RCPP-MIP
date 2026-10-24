"""Regression checks for experiment 2, without network or CPLEX."""

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
import distance_objective_experiment as experiment


class DistanceObjectiveExperimentTest(unittest.TestCase):
    def test_measure_uses_distance_for_best_cluster_path(self):
        outputs = {
            "mip": "RCPP_RESULT elapsed_ms=12 has_solution=true optimal=true objective=45.5\n",
            "bestClusterPath": (
                "BEST_CLUSTER_PATH_RESULT elapsed_ms=20 has_solution=true "
                "optimal=false objective=3 crossings=3 distance=41.25\n"),
        }
        with tempfile.TemporaryDirectory() as directory:
            for model, output in outputs.items():
                with mock.patch.object(
                        experiment.subprocess, "run",
                        return_value=subprocess.CompletedProcess([], 0, output, "")):
                    row = experiment.measure(["solver"], Path(directory) / model, model)
                self.assertEqual(row["outcome"], "completed")
                self.assertEqual(row["objective"],
                                 "41.25" if model == "bestClusterPath" else "45.5")
                self.assertEqual(row["model_objective"],
                                 "3" if model == "bestClusterPath" else "45.5")
                self.assertEqual(row["crossings"],
                                 "3" if model == "bestClusterPath" else "")

    def test_missing_best_cluster_distance_is_an_error(self):
        output = ("BEST_CLUSTER_PATH_RESULT elapsed_ms=20 has_solution=true "
                  "optimal=true objective=3\n")
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch.object(
                    experiment.subprocess, "run",
                    return_value=subprocess.CompletedProcess([], 0, output, "")):
            row = experiment.measure(["solver"], Path(directory), "bestClusterPath")
        self.assertEqual(row["outcome"], "error")
        self.assertEqual(row["objective"], "")

    def test_runner_saves_and_resumes_all_models(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "solver"
            binary.write_text("#!/bin/sh\nexit 0\n")
            binary.chmod(0o755)
            data = root / "data"
            for path in experiment.source_paths(data, "small").values():
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("input")

            def prepare(source, target, clustering, percentage):
                target.mkdir(parents=True, exist_ok=True)
                files = {key: target / key
                         for key in ("graph", "turns", "in_out", "arc_clusters")}
                for path in files.values():
                    path.write_text("input")
                return dict(files, nodes=4, edges=6, required_edges=3)

            def measure(command, target, model):
                target.mkdir(parents=True, exist_ok=True)
                (target / "solver.log").write_text("completed")
                objective = "40" if model == "bestClusterPath" else "50"
                return dict(objective=objective, model_objective="2", crossings="2",
                            elapsed_ms="12", wall_ms="15", has_solution="true",
                            optimal="true", returncode=0, outcome="completed")

            arguments = ["demo", "--maps", "small", "--data-root", str(data),
                         "--solver", str(binary), "--best-solver", str(binary),
                         "--no-plots"]
            with mock.patch.object(experiment, "ROOT", root), \
                    mock.patch.object(experiment, "prepare_instance", side_effect=prepare), \
                    mock.patch.object(experiment, "measure", side_effect=measure) as run:
                experiment.main(arguments)
                self.assertEqual(run.call_count, 10)
                rows = experiment.read_rows(root / "experiments/demo/resultados.csv")
                self.assertEqual(len(rows), 10)
                experiment.main(arguments)
                self.assertEqual(run.call_count, 10)

    def test_rejects_conflicting_plot_options(self):
        with self.assertRaisesRegex(ValueError, "no se pueden combinar"):
            experiment.main(["--plot-only", "--no-plots"])

    @unittest.skipUnless(importlib.util.find_spec("matplotlib"),
                         "matplotlib no instalado")
    def test_plot_orders_maps_by_edge_count(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            os.environ["MPLCONFIGDIR"] = str(output / ".matplotlib")
            rows = []
            for clustering in ("bfs", "streets"):
                for name, edges, objective in (("large", 8, 80), ("small", 3, 30)):
                    rows.append({"map": name, "clustering": clustering,
                                 "percentage": "20.0", "edges": str(edges),
                                 "model": "mip", "objective": str(objective),
                                 "outcome": "completed"})
            paths = experiment.plot_results(rows, output, ["large", "small"], 20.0)
            self.assertEqual([path.name for path in paths],
                             ["experimento_2_objetivo_bfs.png",
                              "experimento_2_objetivo_streets.png"])
            self.assertTrue(all(path.is_file() for path in paths))


if __name__ == "__main__":
    unittest.main()
