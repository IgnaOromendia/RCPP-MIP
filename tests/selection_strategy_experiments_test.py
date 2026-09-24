"""Tests for the selection-strategy experiment runner."""

import argparse
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from selection_strategy_experiments import (FIELDS, SELECTION_STRATEGIES,
                                             append_row, completed_keys,
                                             configurations_for, parse_arguments,
                                             plot_series, read_rows, run_solver,
                                             validate)


class SelectionStrategyExperimentsTest(unittest.TestCase):
    def test_reachability_is_not_a_runner_parameter(self):
        options = parse_arguments(["prueba"])
        self.assertFalse(hasattr(options, "reachability"))
        with self.assertRaises(SystemExit):
            parse_arguments(["prueba", "--reachability", "7"])

    def test_configurations_include_default_and_every_strategy(self):
        self.assertEqual(
            configurations_for(2),
            [("default", 1, "mip"),
             ("random", 1, "fixAndOptimize"),
             ("random", 2, "fixAndOptimize"),
             ("maxDeadheadCost", 1, "fixAndOptimize"),
             ("maxDeadheadCost", 2, "fixAndOptimize"),
             ("topKDeadheadCost", 1, "fixAndOptimize"),
             ("topKDeadheadCost", 2, "fixAndOptimize")],
        )
        self.assertEqual(plot_series(), ["default", *SELECTION_STRATEGIES])

    def test_configurations_can_compare_only_default_and_top_k(self):
        self.assertEqual(
            configurations_for(2, ["topKDeadheadCost"]),
            [("default", 1, "mip"),
             ("topKDeadheadCost", 1, "fixAndOptimize"),
             ("topKDeadheadCost", 2, "fixAndOptimize")],
        )
        self.assertEqual(plot_series(["topKDeadheadCost"]),
                         ["default", "topKDeadheadCost"])

        options = parse_arguments([
            "prueba", "--selection-strategies", "topKDeadheadCost",
        ])
        self.assertEqual(options.selection_strategies, ["topKDeadheadCost"])

    def test_solver_commands_for_all_configurations(self):
        completed = mock.Mock(returncode=0, stdout="", stderr="")
        with mock.patch("selection_strategy_experiments.subprocess.run",
                        return_value=completed) as run:
            for selection_strategy, expected_tail in (
                    ("random", ["random"]),
                    ("maxDeadheadCost", ["maxDeadheadCost"]),
                    ("topKDeadheadCost", ["topKDeadheadCost"])):
                run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"),
                           selection_strategy, Path("run"), "fixAndOptimize")
                self.assertEqual(
                    run.call_args.args[0],
                    ["solver", "graph.dat", "turns.dat", "fixAndOptimize",
                     *expected_tail],
                )

            run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"),
                       "default", Path("run"), "mip")
            self.assertEqual(run.call_args.args[0],
                             ["solver", "graph.dat", "turns.dat", "mip"])

    def test_csv_round_trip_and_resume_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            csv_path = Path(directory) / "resultados.csv"
            for strategy in ("default", "random"):
                row = dict.fromkeys(FIELDS, "")
                row.update({"seed": 0, "size": 100,
                            "selection_strategy": strategy, "repetition": 1})
                append_row(csv_path, row)
            rows = read_rows(csv_path)

        self.assertEqual(completed_keys(rows),
                         {(0, 100, "default", 1),
                          (0, 100, "random", 1)})

    def test_validation_accepts_adaptive_reachability(self):
        options = parse_arguments(["prueba"])
        options.plot_only = True
        validate(options)


if __name__ == "__main__":
    unittest.main()
