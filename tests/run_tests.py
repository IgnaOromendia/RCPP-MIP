"""Run graph and CPLEX integration tests in isolated directories."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures"
SOLVER = ROOT / "solverExec"


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
    parser = argparse.ArgumentParser()
    parser.add_argument('--unit-only', action='store_true')
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--solver', type=Path, default=SOLVER)
    options = parser.parse_args()
    build = options.build_dir.resolve()
    solver = options.solver.resolve()
    generator_command = [sys.executable, ROOT / 'tests/generate_graph_test.py',
                         '--reader', build / 'instance_reader_test']
    if not options.unit_only:
        generator_command.extend(['--solver', solver])
    with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
        run(generator_command, directory, 0)
    print('PASS generator')
    with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
        run([sys.executable, ROOT / 'tests/reachability_experiments_test.py'], directory, 0)
    print('PASS reachability experiments')
    for domain in ('instance_reader_test', 'solution_writer_test', 'cli_options_test'):
        with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
            run([build / domain], directory, 0)
        print(f'PASS {domain}')
    for domain in ("graph_test", "super_graph_test"):
        for fixture, undirected_count in (("mixed_ids.dat", 2), ("directed_ids.dat", 0),
                                           ("undirected_ids.dat", 4)):
            with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
                run([build / domain, FIXTURES / fixture, str(undirected_count)], directory, 0)
                print(f"PASS {domain}: {fixture}")

    if options.unit_only:
        return
    with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
        run([build / 'constraint_setter_test'], directory, 0)
    print('PASS constraint_setter_test')
    with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
        run([build / 'solver_options_test', FIXTURES], directory, 0)
    print('PASS solver_options_test')
    for scenario in ("optimal", "infeasible", "limited", "aborted"):
        with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
            run([build / 'solver_result_test', FIXTURES, scenario], directory, 0)
            print(f"PASS API: {scenario}")

    for scenario in ("unbuilt", "repeated", "independent", "constructor_error", "use_error"):
        with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
            run([build / "solver_lifetime_test", FIXTURES, scenario], directory, 0)
            print(f"PASS lifetime: {scenario}")

    for arguments in ([], ["--help"], ["-h"]):
        with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
            result = run([solver, *arguments], directory, 1)
            check("Uso: solverExec <input.dat> <curvas.dat> "
                  "[mip|fixAndOptimize <reachability>]" in result.stderr,
                  "Missing usage error for absent input paths")
            check(not (Path(directory) / "out.dat").exists(),
                  "Created output without input paths")
    print("PASS CLI: missing inputs and removed help")

    for scenario in ("feasible", "infeasible", "infeasible_existing", "argument_error", "export_error"):
        with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
            output = Path(directory) / "out.dat"
            infeasible = scenario.startswith("infeasible")
            if scenario == "infeasible_existing":
                output.write_text("previous solution\n")
            if scenario == "export_error":
                output.mkdir()
            command = [solver, FIXTURES / ("infeasible.dat" if infeasible else "feasible.dat"),
                       FIXTURES / "turns.dat", "mip"]
            if scenario == "argument_error":
                command[-1:] = ["fixAndOptimize", "-1"]
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
                check("Optimo: true" in result.stdout, "Missing optimality flag")
                check("RCPP_RESULT " in result.stdout, "Missing machine-readable result")
                for section in ("X", "Y", "YDK & YKD", "F", "FDK"):
                    check(f"---- {section} ----" in contents, f"Missing output section {section}")
            elif scenario == "argument_error":
                check("reachability debe ser un entero no negativo" in result.stderr,
                      "Missing reachability diagnostic")
                check(not output.exists(), "Created output after an argument error")
            else:
                check("Error:" in result.stderr, "Missing export error diagnostic")
            print(f"PASS CLI: {scenario}")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
