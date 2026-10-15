#!/usr/bin/env python3
"""Regression tests for contracting an expanded RCPP solution."""

from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
REMAPPER = ROOT / "re-map-expanded-graph.py"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    with tempfile.TemporaryDirectory(prefix="remap-expanded-") as raw_directory:
        directory = Path(raw_directory)
        mapping = directory / "mapping.dat"
        mapping.write_text(
            "expanded_node original_node\n"
            "1 1\n2 2\n3 -\n4 -\n5 3\n6 4\n",
            encoding="utf-8",
        )
        expanded = directory / "expanded.dat"
        expanded.write_text(
            "OBJ: 19\n\n"
            "---- X ----\nX_1_2_1 = 1\n"
            "\n---- Y ----\n"
            "Y_2_3_1 = 2\nY_3_4_1 = 2\nY_4_5_1 = 2\n"
            "Y_2_5_1 = 1\nY_5_6_1 = 1\n"
            "\n---- YDK & YKD ----\nY_D_1_1 = 1\nY_6_D_1 = 1\n"
            "\n---- F ----\nF_2_3_1 = 0.5\n"
            "\n---- FDK ----\nF_D_1_1 = 0.5\n",
            encoding="utf-8",
        )
        output = directory / "original.dat"
        result = subprocess.run(
            [sys.executable, REMAPPER, expanded, mapping, output],
            capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        contents = output.read_text(encoding="utf-8")
        check("X_1_2_1 = 1" in contents, "service traversal was not retained")
        check("Y_2_3_1 = 3" in contents,
              "contracted and direct traversals were not aggregated")
        check("Y_3_4_1 = 1" in contents, "physical traversal was not retained")
        check("Y_D_1_1 = 1" in contents and "Y_4_D_1 = 1" in contents,
              "deposit traversals were not remapped")
        check("_5_" not in contents and "_6_" not in contents,
              "expanded node identifiers leaked into the remapped solution")

        malformed = directory / "malformed.dat"
        malformed.write_text(
            "OBJ: 1\n\n---- X ----\n\n---- Y ----\nY_2_3_1 = 1\n"
            "\n---- YDK & YKD ----\n",
            encoding="utf-8",
        )
        rejected = subprocess.run(
            [sys.executable, REMAPPER, malformed, mapping, output],
            capture_output=True, text=True, timeout=10,
        )
        check(rejected.returncode == 1 and "no conserva el flujo" in rejected.stderr,
              "an unbalanced synthetic path was accepted")

        graph = directory / "graph.dat"
        graph.write_text(
            "1 4 3 3 0\n1 3 4\n"
            "1 2 0 1 0\n2 3 0 1 0\n2 4 0 1 0\n",
            encoding="utf-8",
        )
        turns = directory / "turns.dat"
        turns.write_text("0 1\n1 2 3\n", encoding="utf-8")
        identity = directory / "identity.dat"
        identity.write_text(
            "expanded_node original_node\n" +
            "".join(f"{node} {node}\n" for node in range(1, 13)),
            encoding="utf-8",
        )
        physical = directory / "physical.dat"
        physical.write_text(
            "OBJ: 6\n\n---- X ----\n\n---- Y ----\n"
            "Y_1_2_1 = 1\nY_3_4_1 = 1\nY_5_6_1 = 1\n"
            "Y_7_8_1 = 1\nY_9_10_1 = 1\nY_11_12_1 = 1\n"
            "Y_2_5_1 = 1\n"
            "\n---- YDK & YKD ----\n"
            "Y_D_1_1 = 1\nY_4_D_1 = 1\n"
            "Y_6_D_1 = 1\nY_D_7_1 = 1\n"
            "Y_10_D_1 = 1\nY_D_11_1 = 1\n"
            "\n---- F ----\n\n---- FDK ----\n",
            encoding="utf-8",
        )
        legal_output = directory / "legal.dat"
        legal = subprocess.run(
            [sys.executable, REMAPPER, physical, identity, legal_output,
             "--graph", graph, "--turns", turns],
            capture_output=True, text=True, timeout=10,
        )
        check(legal.returncode == 0, legal.stdout + legal.stderr)
        legal_contents = legal_output.read_text(encoding="utf-8")
        check("Y_2_5_1" not in legal_contents,
              "the forbidden turn survived the topology-aware remap")
        check("Y_2_9_1 = 1" in legal_contents and "Y_12_5_1 = 1" in legal_contents,
              "physical passes were not reconnected through legal turns")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
