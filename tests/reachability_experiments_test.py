"""Tests for the reachability experiment runner CLI and output paths."""

import argparse
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

from reachability_experiments import experiment_output_directory, parse_arguments


class ReachabilityExperimentsTest(unittest.TestCase):
    def test_experiment_name_is_a_positional_argument(self):
        options = parse_arguments(["prueba_reachability", "--sizes", "1000"])

        self.assertEqual(options.experiment_name, "prueba_reachability")
        self.assertEqual(options.sizes, [1000])
        self.assertEqual(
            experiment_output_directory(options.experiment_name),
            ROOT / "experiments" / "prueba_reachability",
        )

    def test_experiment_name_cannot_be_a_path(self):
        for name in ("", ".", "..", "grupo/prueba", "grupo\\prueba"):
            with self.subTest(name=name), self.assertRaises(argparse.ArgumentTypeError):
                experiment_output_directory(name)


if __name__ == "__main__":
    unittest.main()
