"""Tests for the reachability experiment runner CLI and output paths."""

import argparse
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from reachability_experiments import (experiment_output_directory, generate_instance,
                                      parse_arguments)


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


if __name__ == "__main__":
    unittest.main()
