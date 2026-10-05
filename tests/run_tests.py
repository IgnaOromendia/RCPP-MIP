"""Run graph and CPLEX integration tests in isolated directories."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
FIXTURES = ROOT / "tests" / "fixtures"
SOLVER = ROOT / "solverExec"
PATH_SORTER = ROOT / "pathSortExec"


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
    parser.add_argument('--path-sorter', type=Path, default=PATH_SORTER)
    options = parser.parse_args()
    build = options.build_dir.resolve()
    solver = options.solver.resolve()
    path_sorter = options.path_sorter.resolve()
    generator_command = [sys.executable, ROOT / 'tests/generate_graph_test.py',
                         '--reader', build / 'instance_reader_test']
    if not options.unit_only:
        generator_command.extend(['--solver', solver])
    with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
        run(generator_command, directory, 0)
    print('PASS generator')
    with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
        run([sys.executable, ROOT / 'tests/selection_strategy_experiments_test.py'],
            directory, 0)
    print('PASS selection-strategy experiments')
    for domain in ('instance_reader_test', 'solution_writer_test', 'cli_options_test',
                   'fix_and_optimize_test'):
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
    with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
        run([build / 'path_sorter_test'], directory, 0)
    print('PASS path_sorter_test')
    with tempfile.TemporaryDirectory(prefix='rcpp-test-') as directory:
        run([build / 'path_sort_instance_reader_test'], directory, 0)
    print('PASS path_sort_instance_reader_test')
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
                  "[mip|fixAndOptimize "
                  "[maxDeadheadCost|random|topKDeadheadCost]]" in result.stderr,
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
                command[-1:] = ["fixAndOptimize", "unknown"]
            expected = 2 if infeasible else (1 if scenario.endswith("error") else 0)
            result = run(command, directory, expected)
            if infeasible:
                check("No se encontro solucion." in result.stderr,
                      "Missing no-solution diagnostic")
                check("Status:" not in result.stderr, "Printed solver status")
                check("Funcion objetivo:" not in result.stdout, "Printed an unavailable objective")
                if scenario == "infeasible_existing":
                    check(output.read_text() == "previous solution\n", "Overwrote previous output")
                else:
                    check(not output.exists(), "Created output without a solution")
            elif scenario == "feasible":
                contents = output.read_text()
                check(contents.startswith("OBJ: 7\n"), "Incorrect exported objective")
                check("Strategy: mip" in result.stdout, "Missing MIP strategy output")
                check("Selection strategy:" not in result.stdout,
                      "Printed a selection strategy for MIP")
                check("Optimo: true" in result.stdout, "Missing optimality flag")
                check("RCPP_RESULT " in result.stdout, "Missing machine-readable result")
                check("Funcion objetivo: 7" in result.stdout,
                      "Missing obtained objective")
                check("reachability=" not in result.stdout,
                      "Printed reachability in solver output")
                check("status=" not in result.stdout and "(Optimal)" not in result.stdout,
                      "Printed solver status")
                for section in ("X", "Y", "YDK & YKD", "F", "FDK"):
                    check(f"---- {section} ----" in contents, f"Missing output section {section}")
            elif scenario == "argument_error":
                check("selection strategy debe ser" in result.stderr,
                      "Missing selection-strategy diagnostic")
                check(not output.exists(), "Created output after an argument error")
            else:
                check("Error:" in result.stderr, "Missing export error diagnostic")
            print(f"PASS CLI: {scenario}")

    with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
        result = run([solver, FIXTURES / "feasible.dat", FIXTURES / "turns.dat",
                      "fixAndOptimize", "random"], directory, 0)
        check("Funcion objetivo: 7" in result.stdout,
              "Random selection strategy did not produce the expected solution")
        check("Strategy: fixAndOptimize" in result.stdout,
              "Missing Fix-and-Optimize strategy output")
        check("Selection strategy: random" in result.stdout,
              "Missing random selection strategy output")
    print("PASS CLI: random selection strategy")

    with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
        result = run([solver, FIXTURES / "feasible.dat", FIXTURES / "turns.dat",
                      "fixAndOptimize", "topKDeadheadCost"], directory, 0)
        check("Funcion objetivo: 7" in result.stdout,
              "Top-k selection strategy did not produce the expected solution")
        check("Selection strategy: topKDeadheadCost" in result.stdout,
              "Missing top-k selection strategy output")
    print("PASS CLI: top-k selection strategy")

    for arguments in ([], [FIXTURES / "feasible.dat"]):
        with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
            result = run([path_sorter, *arguments], directory, 1)
            check("Uso: pathSortExec" in result.stderr,
                  "Missing path-sort usage diagnostic")
            check(not (Path(directory) / "orden.dat").exists(),
                  "Created an order without all input paths")
    print("PASS path-sort CLI: missing inputs")

    with tempfile.TemporaryDirectory(prefix="rcpp-test-") as directory:
        rcpp_output = Path(directory) / "rcpp.dat"
        run([solver, FIXTURES / "feasible.dat", FIXTURES / "turns.dat", "mip"],
            directory, 0)
        (Path(directory) / "out.dat").replace(rcpp_output)
        result = run([path_sorter, FIXTURES / "feasible.dat", FIXTURES / "turns.dat",
                      rcpp_output], directory, 0)
        order = Path(directory) / "orden.dat"
        check("Orden guardado en orden.dat" in result.stdout,
              "Missing path-sort export diagnostic")
        check(order.exists(), "Path sorter did not create orden.dat")
        lines = order.read_text().splitlines()
        check(lines[0] ==
              "posicion vehiculo origen destino pasada super_arco arista_original",
              "Incorrect path-order header")
        rows = [line.split() for line in lines[1:]]
        check([int(row[0]) for row in rows] == list(range(1, len(rows) + 1)),
              "Path-order positions are not consecutive")
        check(rows[0][2] == "D" and rows[-1][3] == "D",
              "Path order does not start and finish at the deposit")
        check(all(rows[index][3] == rows[index + 1][2]
                  for index in range(len(rows) - 1)),
              "Path-order rows are not continuous")
    print("PASS path-sort CLI: feasible order")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
