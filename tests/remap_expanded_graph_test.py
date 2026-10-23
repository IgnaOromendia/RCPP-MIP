#!/usr/bin/env python3
"""Regression tests for contracting an expanded RCPP solution."""

from pathlib import Path
import runpy
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
REMAPPER = ROOT / "re-map-expanded-graph.py"
sys.path.insert(0, str(ROOT))


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    remapper = runpy.run_path(str(REMAPPER), run_name="remapper_test")
    remove_subtours = remapper["_remove_deadhead_subtours"]
    cleaned = remove_subtours(
        {(1, 2, 1): 1},
        {(-1, 1, 1): 1, (2, -1, 1): 1,
         (3, 4, 1): 1, (4, 3, 1): 1},
    )
    check((3, 4, 1) not in cleaned and (4, 3, 1) not in cleaned,
          "a disconnected deadhead-only subtour was retained")
    try:
        remove_subtours(
            {(3, 4, 1): 1},
            {(4, 3, 1): 1},
        )
    except ValueError as error:
        check("servicio" in str(error) and "deposito" in str(error),
              "disconnected service reported an unclear error")
    else:
        raise RuntimeError("a disconnected service component was accepted")

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

        backtrack_graph = directory / "backtrack-graph.dat"
        backtrack_graph.write_text(
            "1 2 2 1 0\n1 2\n"
            "1 2 1 1 0\n",
            encoding="utf-8",
        )
        backtrack_turns = directory / "backtrack-turns.dat"
        backtrack_turns.write_text("0 0\n", encoding="utf-8")
        backtrack_mapping = directory / "backtrack-mapping.dat"
        backtrack_mapping.write_text(
            "expanded_node original_node\n1 1\n2 2\n3 3\n4 4\n",
            encoding="utf-8",
        )
        backtrack_solution = directory / "backtrack-solution.dat"
        backtrack_solution.write_text(
            "OBJ: 0\n\n---- X ----\nX_1_2_1 = 1\n"
            "\n---- Y ----\nY_1_2_1 = 2\nY_3_4_1 = 2\n"
            "\n---- YDK & YKD ----\nY_D_1_1 = 1\nY_2_D_1 = 1\n"
            "\n---- F ----\n\n---- FDK ----\n",
            encoding="utf-8",
        )
        backtrack_output = directory / "backtrack-output.dat"
        backtrack = subprocess.run(
            [sys.executable, REMAPPER, backtrack_solution, backtrack_mapping,
             backtrack_output, "--graph", backtrack_graph,
             "--turns", backtrack_turns],
            capture_output=True, text=True, timeout=10,
        )
        check(backtrack.returncode == 0, backtrack.stdout + backtrack.stderr)
        backtrack_contents = backtrack_output.read_text(encoding="utf-8")
        check("Y_1_2_1" not in backtrack_contents and
              "Y_3_4_1" not in backtrack_contents,
              "opposite deadheads on a serviced edge were not cancelled")

        detour_graph = directory / "detour-graph.dat"
        detour_graph.write_text(
            "1 4 2 4 0\n2 3\n"
            "1 2 1 1 0\n1 3 0 1 0\n3 4 0 1 0\n4 1 0 1 0\n",
            encoding="utf-8",
        )
        detour_turns = directory / "detour-turns.dat"
        detour_turns.write_text("0 0\n", encoding="utf-8")
        detour_mapping = directory / "detour-mapping.dat"
        detour_mapping.write_text(
            "expanded_node original_node\n" +
            "".join(f"{node} {node}\n" for node in range(1, 17)),
            encoding="utf-8",
        )
        detour_solution = directory / "detour-solution.dat"
        detour_solution.write_text(
            "OBJ: 0\n\n---- X ----\nX_3_4_1 = 1\n"
            "\n---- Y ----\nY_1_2_1 = 2\nY_7_8_1 = 1\n"
            "\n---- YDK & YKD ----\n"
            "Y_D_3_1 = 1\nY_D_7_1 = 1\nY_2_D_1 = 2\n"
            "\n---- F ----\n\n---- FDK ----\n",
            encoding="utf-8",
        )
        detour_output = directory / "detour-output.dat"
        detour = subprocess.run(
            [sys.executable, REMAPPER, detour_solution, detour_mapping,
             detour_output, "--graph", detour_graph, "--turns", detour_turns],
            capture_output=True, text=True, timeout=10,
        )
        check(detour.returncode == 0, detour.stdout + detour.stderr)
        detour_contents = detour_output.read_text(encoding="utf-8")
        forward_detour = all(turn in detour_contents for turn in (
            "Y_5_6_1 = 1", "Y_9_10_1 = 1", "Y_13_14_1 = 1"
        ))
        reverse_detour = all(turn in detour_contents for turn in (
            "Y_15_16_1 = 1", "Y_11_12_1 = 1", "Y_7_8_1 = 2"
        ))
        check(forward_detour or reverse_detour,
              "a legal physical detour was not added for a hidden U-turn")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
