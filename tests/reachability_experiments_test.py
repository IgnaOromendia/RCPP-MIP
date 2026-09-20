"""Tests for the reachability experiment runner CLI and output paths."""

import argparse
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from reachability_experiments import (completed_keys, configurations_for,
                                      experiment_output_directory, generate_instance,
                                      parse_arguments, plot_series, run_solver)


class ReachabilityExperimentsTest(unittest.TestCase):
    def test_experiment_name_is_a_positional_argument(self):
        options = parse_arguments(["prueba_reachability", "--sizes", "1000"])

        self.assertEqual(options.experiment_name, "prueba_reachability")
        self.assertEqual(options.sizes, [1000])
        self.assertEqual(options.demand_type, "fixed")
        self.assertEqual(
            experiment_output_directory(options.experiment_name),
            ROOT / "experiments" / "prueba_reachability",
        )

    def test_experiment_name_cannot_be_a_path(self):
        for name in ("", ".", "..", "grupo/prueba", "grupo\\prueba"):
            with self.subTest(name=name), self.assertRaises(argparse.ArgumentTypeError):
                experiment_output_directory(name)

    def test_seed_and_demand_type_are_forwarded_to_generator(self):
        options = parse_arguments([
            "prueba_reachability", "--seed", "7", "--demand-type", "integer",
        ])

        self.assertEqual(options.seed, 7)
        self.assertEqual(options.demand_type, "integer")
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch("reachability_experiments.subprocess.run") as run:
            run.return_value = mock.Mock(returncode=0, stdout="", stderr="")
            generate_instance(Path("generator.py"), Path(directory), 100, 7,
                              options.demand_type, regenerate=True)

        command = run.call_args.args[0]
        self.assertEqual(command[-4:], ["--seed", "7", "--demand-type", "integer"])

    def test_default_solver_run_passes_mip_strategy(self):
        completed = mock.Mock(returncode=0, stdout="", stderr="")
        with mock.patch("reachability_experiments.subprocess.run", return_value=completed) as run:
            run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"), None,
                       10, Path("run"), "mip")

        self.assertEqual(run.call_args.args[0],
                         ["solver", "graph.dat", "turns.dat", "mip"])

    def test_fix_and_optimize_run_passes_strategy(self):
        completed = mock.Mock(returncode=0, stdout="", stderr="")
        with mock.patch("reachability_experiments.subprocess.run", return_value=completed) as run:
            run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"), 20,
                       10, Path("run"), "fixAndOptimize")

        self.assertEqual(run.call_args.args[0],
                         ["solver", "graph.dat", "turns.dat", "fixAndOptimize", "20"])

    def test_default_and_reachability_have_distinct_resume_keys(self):
        rows = [
            {"seed": "0", "size": "100", "reachability_percentage": "default",
             "repetition": "1"},
            {"seed": "0", "size": "100", "reachability_percentage": "5",
             "repetition": "1"},
        ]

        self.assertEqual(completed_keys(rows),
                         {(0, 100, "default", 1), (0, 100, "5", 1)})

    def test_default_runs_once_before_all_reachabilities(self):
        self.assertEqual(
            configurations_for(100, [5, 10], 2),
            [
                ("default", None, 1, "mip"),
                ("5", 5, 1, "fixAndOptimize"),
                ("5", 5, 2, "fixAndOptimize"),
                ("10", 10, 1, "fixAndOptimize"),
                ("10", 10, 2, "fixAndOptimize"),
            ],
        )
        self.assertEqual(plot_series([5, 10]), ["default", "5", "10"])


if __name__ == "__main__":
    unittest.main()
