"""Tests for the fixed-reachability selection-strategy experiment runner."""

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
    def test_reachability_is_required_and_top_k_is_fixed(self):
        with self.assertRaises(SystemExit):
            parse_arguments(["prueba"])

        options = parse_arguments(["prueba", "--reachability", "7"])
        self.assertEqual(options.reachability, 7)

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

    def test_solver_commands_for_all_configurations(self):
        completed = mock.Mock(returncode=0, stdout="", stderr="")
        with mock.patch("selection_strategy_experiments.subprocess.run",
                        return_value=completed) as run:
            for selection_strategy, expected_tail in (
                    ("random", ["7", "random"]),
                    ("maxDeadheadCost", ["7", "maxDeadheadCost"]),
                    ("topKDeadheadCost", ["7", "topKDeadheadCost"])):
                run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"),
                           7, selection_strategy, Path("run"), "fixAndOptimize")
                self.assertEqual(
                    run.call_args.args[0],
                    ["solver", "graph.dat", "turns.dat", "fixAndOptimize",
                     *expected_tail],
                )

            run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"),
                       7, "default", Path("run"), "mip")
            self.assertEqual(run.call_args.args[0],
                             ["solver", "graph.dat", "turns.dat", "mip"])

    def test_csv_round_trip_and_resume_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            csv_path = Path(directory) / "resultados.csv"
            for strategy, reachability in (("default", ""), ("random", 7)):
                row = dict.fromkeys(FIELDS, "")
                row.update({"seed": 0, "size": 100,
                            "reachability": reachability,
                            "selection_strategy": strategy, "repetition": 1})
                append_row(csv_path, row)
            rows = read_rows(csv_path)

        self.assertEqual(completed_keys(rows),
                         {(0, 100, "", "default", 1),
                          (0, 100, "7", "random", 1)})

    def test_validation_rejects_negative_reachability(self):
        options = parse_arguments(["prueba", "--reachability", "-1"])
        options.plot_only = True
        with self.assertRaisesRegex(ValueError, "reachability"):
            validate(options)


if __name__ == "__main__":
    unittest.main()
