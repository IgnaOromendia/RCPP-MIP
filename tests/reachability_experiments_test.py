"""Tests for the reachability experiment runner CLI and output paths."""

import argparse
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from reachability_experiments import (DEFAULT_SIZES, FIELDS, LEGACY_FIELDS,
                                      SELECTION_STRATEGIES, append_row, completed_keys,
                                      configurations_for,
                                      experiment_output_directory, generate_instance,
                                      normalize_objectives_by_size, parse_arguments,
                                      plot_series, read_rows, run_solver, used_sizes)


class ReachabilityExperimentsTest(unittest.TestCase):
    def test_default_sizes_start_at_one_hundred(self):
        options = parse_arguments(["prueba_reachability"])

        self.assertEqual(options.sizes, DEFAULT_SIZES)
        self.assertEqual(options.sizes[0], 100)
        self.assertEqual(options.sizes[-1], 340)

    def test_experiment_name_is_a_positional_argument(self):
        options = parse_arguments(["prueba_reachability", "--sizes", "1000"])

        self.assertEqual(options.experiment_name, "prueba_reachability")
        self.assertEqual(options.sizes, [1000])
        self.assertEqual(options.demand_type, "real")
        self.assertEqual(SELECTION_STRATEGIES, ("random", "deadheadCost"))
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
                       Path("run"), "mip")

        self.assertEqual(run.call_args.args[0],
                         ["solver", "graph.dat", "turns.dat", "mip"])
        self.assertNotIn("timeout", run.call_args.kwargs)

    def test_fix_and_optimize_run_passes_strategy(self):
        completed = mock.Mock(returncode=0, stdout="", stderr="")
        with mock.patch("reachability_experiments.subprocess.run", return_value=completed) as run:
            run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"), 20,
                       Path("run"), "fixAndOptimize")

        self.assertEqual(run.call_args.args[0],
                         ["solver", "graph.dat", "turns.dat", "fixAndOptimize", "20",
                          "deadheadCost"])

    def test_fix_and_optimize_run_passes_random_selection_strategy(self):
        completed = mock.Mock(returncode=0, stdout="", stderr="")
        with mock.patch("reachability_experiments.subprocess.run", return_value=completed) as run:
            run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"), 20,
                       Path("run"), "fixAndOptimize", "random")

        self.assertEqual(run.call_args.args[0],
                         ["solver", "graph.dat", "turns.dat", "fixAndOptimize", "20",
                          "random"])

    def test_default_and_reachability_have_distinct_resume_keys(self):
        rows = [
            {"seed": "0", "size": "100", "reachability_percentage": "default",
             "repetition": "1", "selection_strategy": ""},
            {"seed": "0", "size": "100", "reachability_percentage": "5",
             "repetition": "1", "selection_strategy": "random"},
        ]

        self.assertEqual(completed_keys(rows),
                         {(0, 100, "default", 1, ""),
                          (0, 100, "5", 1, "random")})

    def test_used_sizes_only_includes_sizes_present_in_selected_rows(self):
        rows = [{"size": "300"}, {"size": "100"}, {"size": "300"}]

        self.assertEqual(used_sizes(rows), [100, 300])

    def test_appending_migrates_legacy_csv_to_deadhead_cost(self):
        with tempfile.TemporaryDirectory() as directory:
            csv_path = Path(directory) / "resultados.csv"
            csv_path.write_text(
                ",".join(LEGACY_FIELDS) + "\n" +
                "0,100,5,5,1,1.0,1.1,true,true,Optimal,7,0,completed\n",
                encoding="utf-8",
            )
            new_row = dict.fromkeys(FIELDS, "")
            new_row.update({
                "seed": 0, "size": 100, "reachability_percentage": 5,
                "reachability": 5, "selection_strategy": "random", "repetition": 1,
            })

            append_row(csv_path, new_row)
            rows = read_rows(csv_path)

        self.assertEqual(rows[0]["selection_strategy"], "deadheadCost")
        self.assertEqual(rows[1]["selection_strategy"], "random")

    def test_default_runs_once_before_every_reachability_and_selection_strategy(self):
        self.assertEqual(
            configurations_for(100, [5, 10], 2),
            [
                ("default", None, 1, "mip", ""),
                ("5", 5, 1, "fixAndOptimize", "random"),
                ("5", 5, 2, "fixAndOptimize", "random"),
                ("5", 5, 1, "fixAndOptimize", "deadheadCost"),
                ("5", 5, 2, "fixAndOptimize", "deadheadCost"),
                ("10", 10, 1, "fixAndOptimize", "random"),
                ("10", 10, 2, "fixAndOptimize", "random"),
                ("10", 10, 1, "fixAndOptimize", "deadheadCost"),
                ("10", 10, 2, "fixAndOptimize", "deadheadCost"),
            ],
        )
        self.assertEqual(
            configurations_for(100, [5], 1, ("deadheadCost",))[-1],
            ("5", 5, 1, "fixAndOptimize", "deadheadCost"),
        )
        self.assertEqual(
            plot_series([5, 10]),
            [("default", ""),
             ("5", "random"), ("5", "deadheadCost"),
             ("10", "random"), ("10", "deadheadCost")],
        )

    def test_objectives_are_normalized_independently_for_each_size(self):
        self.assertEqual(
            normalize_objectives_by_size([
                [100.0, 200.0, None],
                [115.0, 230.0, 5.0],
                [107.5, 215.0, 5.0],
            ]),
            [
                [0.0, 0.0, None],
                [1.0, 1.0, 0.0],
                [0.5, 0.5, 0.0],
            ],
        )


if __name__ == "__main__":
    unittest.main()
