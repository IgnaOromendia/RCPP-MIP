"""Tests for the Top-K deadhead-cost experiment runner."""

from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from top_k_deadhead_experiments import (DEFAULT_K_VALUES, DEFAULT_REACHABILITY,
                                        DEFAULT_SIZES, FIELDS, append_row,
                                        completed_keys, configurations_for,
                                        generate_instance, parse_arguments,
                                        plot_series, read_rows, run_solver,
                                        validate)


class TopKDeadheadExperimentsTest(unittest.TestCase):
    def test_defaults_match_requested_experiment(self):
        options = parse_arguments(["prueba_top_k"])

        self.assertEqual(options.sizes, DEFAULT_SIZES)
        self.assertEqual(options.sizes, [100, 140, 180, 220, 260, 300, 340])
        self.assertEqual(options.k_values, [5, 10, 15])
        self.assertEqual(options.reachability, 5)

    def test_configurations_have_one_mip_and_each_k(self):
        self.assertEqual(
            configurations_for([5, 10], 2),
            [("default", 1, "mip"),
             ("5", 1, "fixAndOptimize"),
             ("5", 2, "fixAndOptimize"),
             ("10", 1, "fixAndOptimize"),
             ("10", 2, "fixAndOptimize")],
        )
        self.assertEqual(plot_series([5, 10, 15]),
                         ["default", "5", "10", "15"])

    def test_generator_is_forced_to_real_demand(self):
        with tempfile.TemporaryDirectory() as directory, \
                mock.patch("top_k_deadhead_experiments.subprocess.run") as run:
            run.return_value = mock.Mock(returncode=0, stdout="", stderr="")
            generate_instance(Path("generator.py"), Path(directory), 100, 7, True)

        self.assertEqual(run.call_args.args[0],
                         [sys.executable, "generator.py", "100", "--seed", "7",
                          "--demand-type", "real"])

    def test_top_k_solver_command_has_fixed_reachability(self):
        completed = mock.Mock(returncode=0, stdout="", stderr="")
        with mock.patch("top_k_deadhead_experiments.subprocess.run",
                        return_value=completed) as run:
            run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"),
                       5, 15, Path("run"), "fixAndOptimize")

        self.assertEqual(
            run.call_args.args[0],
            ["solver", "graph.dat", "turns.dat", "fixAndOptimize", "5",
             "topKDeadheadCost", "15"],
        )

    def test_mip_command_has_no_heuristic_arguments(self):
        completed = mock.Mock(returncode=0, stdout="", stderr="")
        with mock.patch("top_k_deadhead_experiments.subprocess.run",
                        return_value=completed) as run:
            run_solver(Path("solver"), Path("graph.dat"), Path("turns.dat"),
                       DEFAULT_REACHABILITY, None, Path("run"), "mip")

        self.assertEqual(run.call_args.args[0],
                         ["solver", "graph.dat", "turns.dat", "mip"])

    def test_csv_round_trip_and_resume_keys(self):
        with tempfile.TemporaryDirectory() as directory:
            csv_path = Path(directory) / "resultados.csv"
            rows = []
            for top_k, reachability in (("default", ""), (5, 5)):
                row = dict.fromkeys(FIELDS, "")
                row.update({"seed": 0, "size": 100, "top_k": top_k,
                            "reachability": reachability, "repetition": 1})
                append_row(csv_path, row)
            rows = read_rows(csv_path)

        self.assertEqual(completed_keys(rows),
                         {(0, 100, "", "default", 1),
                          (0, 100, "5", "5", 1)})

    def test_validation_rejects_non_positive_k(self):
        options = parse_arguments(["prueba", "--k-values", "0"])
        options.plot_only = True
        with self.assertRaisesRegex(ValueError, "valores de k"):
            validate(options)


if __name__ == "__main__":
    unittest.main()
