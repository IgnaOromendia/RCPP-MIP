"""Run CPLEX integration tests in isolated directories, preserving user output."""

from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures"
SOLVER = ROOT / "solverExec"
TEST = ROOT / "build" / "solver_result_test"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(command, directory, expected):
    result = subprocess.run(command, cwd=directory, capture_output=True, text=True, timeout=30)
    check(result.returncode == expected,
          f"{command} returned {result.returncode}, expected {expected}\n"
          f"{result.stdout}\n{result.stderr}")
    return result


def main():
    for scenario in ("optimal", "infeasible", "limited", "aborted"):
        with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
            run([TEST, FIXTURES, scenario], directory, 0)
            print(f"PASS API: {scenario}")

    for scenario in ("feasible", "infeasible", "infeasible_existing", "cplex_error", "export_error"):
        with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
            output = Path(directory) / "out.dat"
            infeasible = scenario.startswith("infeasible")
            if scenario == "infeasible_existing":
                output.write_text("previous solution\n")
            if scenario == "export_error":
                output.mkdir()
            command = [SOLVER, FIXTURES / ("infeasible.dat" if infeasible else "feasible.dat"),
                       FIXTURES / "turns.dat"]
            if scenario == "cplex_error":
                command.append("-1")
            expected = 2 if infeasible else (1 if scenario.endswith("error") else 0)
            result = run(command, directory, expected)
            if infeasible:
                check("Infeasible" in result.stderr, "Missing infeasibility diagnostic")
                check("Funcion objetivo:" not in result.stdout, "Printed an unavailable objective")
                if scenario == "infeasible_existing":
                    check(output.read_text() == "previous solution\n", "Overwrote previous output")
                else:
                    check(not output.exists(), "Created output without a solution")
            elif scenario == "feasible":
                contents = output.read_text()
                check(contents.startswith("OBJ: 7\n"), "Incorrect exported objective")
                for section in ("X", "Y", "YDK & YKD", "F", "FDK"):
                    check(f"---- {section} ----" in contents, f"Missing output section {section}")
            elif scenario == "cplex_error":
                check("Error de CPLEX:" in result.stderr, "Missing CPLEX diagnostic")
                check(not output.exists(), "Created output after a CPLEX error")
            else:
                check("Error:" in result.stderr, "Missing export error diagnostic")
            print(f"PASS CLI: {scenario}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
