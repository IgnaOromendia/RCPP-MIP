#!/usr/bin/env python3
"""Benchmark default MIP and Fix-and-Optimize reachabilities with comparison plots.

The script generates one reproducible graph per size, runs the default MIP once
and every configured reachability in an isolated directory, appends each result
to CSV immediately, and can resume an interrupted experiment without repeating
completed runs.
"""

import argparse
import csv
import math
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import tempfile
import time


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SIZES = [100, 140, 180, 220, 260, 300,
                 340, 380, 420, 460, 500, 540,
                 580, 620, 660, 700]
DEFAULT_REACHABILITY_PERCENTAGES = [5, 10, 15, 20, 25]
DEFAULT_SEED = 0
DEFAULT_SERIES = "default"
DEFAULT_SELECTION_STRATEGY = "deadheadCost"
FIELDS = ["seed", "size", "reachability_percentage", "reachability",
          "selection_strategy", "repetition", "elapsed_ms",
          "wall_ms", "has_solution", "optimal", "status", "objective",
          "returncode", "outcome"]
LEGACY_FIELDS = [field for field in FIELDS if field != "selection_strategy"]
RESULT_PATTERN = re.compile(
    r"^RCPP_RESULT elapsed_ms=(?P<elapsed_ms>\S+) "
    r"reachability=(?P<reachability>-?\d+) "
    r"has_solution=(?P<has_solution>true|false) "
    r"optimal=(?P<optimal>true|false) "
    r"status=(?P<status>\S+) objective=(?P<objective>\S+)$",
    re.MULTILINE,
)


def unique(values):
    return list(dict.fromkeys(values))


def parse_experiment_name(name):
    """Accept a single folder name and reject paths or traversal."""
    if not name or name in {".", ".."} or "/" in name or "\\" in name:
        raise argparse.ArgumentTypeError(
            "el nombre del experimento debe ser un nombre de carpeta, no una ruta")
    return name


def experiment_output_directory(name):
    """Return the output directory for a validated experiment name."""
    parse_experiment_name(name)
    return ROOT / "experiments" / name


def reachability_for(size, percentage):
    """Convert a percentage of n to a positive integer BFS radius."""
    return max(1, math.ceil(size * percentage / 100))


def configurations_for(size, percentages, repetitions,
                       selection_strategy=DEFAULT_SELECTION_STRATEGY):
    """Return default MIP first, followed by all Fix-and-Optimize runs."""
    configurations = [(DEFAULT_SERIES, None, 1, "mip", "")]
    configurations.extend(
        (str(percentage), reachability_for(size, percentage), repetition,
         "fixAndOptimize", selection_strategy)
        for percentage in percentages
        for repetition in range(1, repetitions + 1)
    )
    return configurations


def plot_series(percentages):
    """Return the CSV series in their display order."""
    return [DEFAULT_SERIES, *[str(value) for value in percentages]]


def parse_solver_result(stdout):
    """Return the fields from the last machine-readable solver output line."""
    matches = list(RESULT_PATTERN.finditer(stdout))
    if not matches:
        return None
    values = matches[-1].groupdict()
    values["elapsed_ms"] = float(values["elapsed_ms"])
    values["reachability"] = int(values["reachability"])
    values["has_solution"] = values["has_solution"] == "true"
    values["optimal"] = values["optimal"] == "true"
    values["objective"] = (None if values["objective"] == "NA"
                           else float(values["objective"]))
    return values


def text_from_timeout(value):
    if value is None:
        return ""
    return value.decode(errors="replace") if isinstance(value, bytes) else value


def read_rows(csv_path):
    if not csv_path.exists():
        return []
    with csv_path.open(newline="", encoding="utf-8") as source:
        return list(csv.DictReader(source))


def row_selection_strategy(row):
    """Infer the strategy used by rows created before this column existed."""
    if row["reachability_percentage"] == DEFAULT_SERIES:
        return ""
    return row.get("selection_strategy") or DEFAULT_SELECTION_STRATEGY


def migrate_legacy_csv(csv_path):
    """Add selection_strategy to an existing CSV without losing its rows."""
    if not csv_path.exists() or csv_path.stat().st_size == 0:
        return
    with csv_path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames == FIELDS:
            return
        if reader.fieldnames != LEGACY_FIELDS:
            raise ValueError(f"Columnas incompatibles en {csv_path}")
        rows = list(reader)

    temporary = csv_path.with_suffix(csv_path.suffix + ".tmp")
    with temporary.open("w", newline="", encoding="utf-8") as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS)
        writer.writeheader()
        for row in rows:
            row["selection_strategy"] = row_selection_strategy(row)
            writer.writerow(row)
    os.replace(temporary, csv_path)


def append_row(csv_path, row):
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    migrate_legacy_csv(csv_path)
    new_file = not csv_path.exists() or csv_path.stat().st_size == 0
    with csv_path.open("a", newline="", encoding="utf-8") as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS)
        if new_file:
            writer.writeheader()
        writer.writerow(row)
        target.flush()


def completed_keys(rows):
    return {(int(row["seed"]), int(row["size"]),
             row["reachability_percentage"],
             int(row["repetition"]), row_selection_strategy(row)) for row in rows}


def generate_instance(generator, instance_root, size, seed, demand_type, regenerate):
    graph = instance_root / "input" / f"graph_{size}.dat"
    turns = instance_root / "input" / f"graph_{size}.turns.dat"
    if regenerate or not (graph.exists() and turns.exists()):
        command = [sys.executable, str(generator), str(size), "--seed", str(seed),
                   "--demand-type", demand_type]
        result = subprocess.run(command, cwd=instance_root, capture_output=True,
                                text=True)
        if result.returncode != 0:
            raise RuntimeError(
                f"No se pudo generar n={size} (exit {result.returncode}):\n"
                f"{result.stdout}{result.stderr}")
    return graph.resolve(), turns.resolve()


def run_solver(solver, graph, turns, reachability, timeout_seconds, run_directory,
               strategy, selection_strategy=DEFAULT_SELECTION_STRATEGY):
    command = [str(solver), str(graph), str(turns), strategy]
    if strategy == "mip":
        if reachability is not None:
            raise ValueError("mip no recibe reachability")
    elif strategy == "fixAndOptimize":
        if reachability is None:
            raise ValueError("fixAndOptimize requiere reachability")
        if selection_strategy not in ("deadheadCost", "random"):
            raise ValueError(f"Selection strategy desconocida: {selection_strategy}")
        command.extend((str(reachability), selection_strategy))
    else:
        raise ValueError(f"Estrategia desconocida: {strategy}")
    started = time.perf_counter()
    try:
        result = subprocess.run(command, cwd=run_directory, capture_output=True,
                                text=True, timeout=timeout_seconds)
        wall_ms = (time.perf_counter() - started) * 1000
        parsed = parse_solver_result(result.stdout)
        outcome = "completed" if result.returncode in (0, 2) and parsed else "error"
        return result.stdout, result.stderr, result.returncode, wall_ms, parsed, outcome
    except subprocess.TimeoutExpired as error:
        wall_ms = (time.perf_counter() - started) * 1000
        return (text_from_timeout(error.stdout), text_from_timeout(error.stderr),
                -1, wall_ms, None, "timeout")


def write_log(path, command_description, stdout, stderr):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        f"{command_description}\n\n--- stdout ---\n{stdout}"
        f"\n--- stderr ---\n{stderr}",
        encoding="utf-8",
    )


def make_row(seed, size, percentage, reachability, selection_strategy, repetition,
             wall_ms, parsed, returncode, outcome):
    return {
        "seed": seed,
        "size": size,
        "reachability_percentage": percentage,
        "reachability": reachability,
        "selection_strategy": selection_strategy,
        "repetition": repetition,
        "elapsed_ms": "" if parsed is None else f'{parsed["elapsed_ms"]:.6f}',
        "wall_ms": f"{wall_ms:.6f}",
        "has_solution": "false" if parsed is None else str(parsed["has_solution"]).lower(),
        "optimal": "false" if parsed is None else str(parsed["optimal"]).lower(),
        "status": outcome.upper() if parsed is None else parsed["status"],
        "objective": "" if parsed is None or parsed["objective"] is None
                     else f'{parsed["objective"]:.17g}',
        "returncode": returncode,
        "outcome": outcome,
    }


def plot_results(rows, sizes, percentages, seed, output_directory,
                 selection_strategy=DEFAULT_SELECTION_STRATEGY):
    try:
        cache = output_directory / ".matplotlib"
        cache.mkdir(parents=True, exist_ok=True)
        os.environ.setdefault("MPLCONFIGDIR", str(cache))
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        import numpy as np
    except ImportError as error:
        raise RuntimeError(
            "Para generar los plots instale matplotlib (python3 -m pip install matplotlib)."
        ) from error

    series = plot_series(percentages)
    selected = [row for row in rows
                if int(row["seed"]) == seed
                and int(row["size"]) in sizes
                and row["reachability_percentage"] in series
                and (row["reachability_percentage"] == DEFAULT_SERIES
                     or row_selection_strategy(row) == selection_strategy)]
    if not selected:
        raise RuntimeError("No hay resultados para graficar con esta configuracion.")

    fig, ax = plt.subplots(figsize=(10, 6))
    for series_name in series:
        x_values, y_values = [], []
        for size in sizes:
            measurements = [float(row["elapsed_ms"]) / 1000
                            for row in selected
                            if row["reachability_percentage"] == series_name
                            and int(row["size"]) == size
                            and row["outcome"] == "completed"
                            and row["elapsed_ms"]]
            if measurements:
                x_values.append(size)
                y_values.append(statistics.median(measurements))
        if x_values:
            label = ("default (MIP)" if series_name == DEFAULT_SERIES else
                     f"reachability={series_name}% de n")
            style = {"color": "black", "linestyle": "--"} if (
                series_name == DEFAULT_SERIES) else {}
            ax.plot(x_values, y_values, marker="o", linewidth=1.8,
                    label=label, **style)
    ax.set_xlabel("Cantidad de nodos")
    ax.set_ylabel("Tiempo total del solver (s, mediana)")
    ax.set_title("Tiempo por tamano de grafo y estrategia")
    ax.set_xscale("log")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    time_plot = output_directory / "tiempo_por_reachability.png"
    fig.savefig(time_plot, dpi=180)
    plt.close(fig)

    matrix = np.full((len(series), len(sizes)), np.nan)
    labels = [["" for _ in sizes] for _ in series]
    for row_index, series_name in enumerate(series):
        for column_index, size in enumerate(sizes):
            attempts = [row for row in selected
                        if row["reachability_percentage"] == series_name
                        and int(row["size"]) == size]
            if attempts:
                ratio = sum(row["optimal"] == "true" for row in attempts) / len(attempts)
                matrix[row_index, column_index] = ratio
                labels[row_index][column_index] = ("Si" if ratio == 1 else
                                                   "No" if ratio == 0 else
                                                   f"{ratio:.0%}")

    fig_width = max(10, 0.9 * len(sizes))
    fig, ax = plt.subplots(figsize=(fig_width, 1.2 + 0.65 * len(series)))
    color_map = plt.get_cmap("RdYlGn").copy()
    color_map.set_bad("#d1d5db")
    image = ax.imshow(np.ma.masked_invalid(matrix), vmin=0, vmax=1,
                      cmap=color_map, aspect="auto")
    ax.set_xticks(range(len(sizes)), [f"{size:,}" for size in sizes], rotation=45,
                  ha="right")
    series_labels = ["default (MIP)" if value == DEFAULT_SERIES else f"{value}%"
                     for value in series]
    ax.set_yticks(range(len(series)), series_labels)
    ax.set_xlabel("Cantidad de nodos")
    ax.set_ylabel("Estrategia / reachability")
    ax.set_title("Proporcion de ejecuciones con estado Optimal")
    for row_index in range(len(series)):
        for column_index in range(len(sizes)):
            label = labels[row_index][column_index] or "Sin dato"
            ax.text(column_index, row_index, label, ha="center", va="center",
                    fontsize=8)
    colorbar = fig.colorbar(image, ax=ax, fraction=0.03, pad=0.03)
    colorbar.set_label("Fraccion optimal")
    fig.tight_layout()
    optimality_plot = output_directory / "optimalidad_por_reachability.png"
    fig.savefig(optimality_plot, dpi=180)
    plt.close(fig)
    return time_plot, optimality_plot


def parse_arguments(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "experiment_name",
        type=parse_experiment_name,
        help="nombre del experimento; guarda los resultados en experiments/<nombre>",
    )
    parser.add_argument("--solver", type=Path, default=ROOT / "solverExec")
    parser.add_argument("--generator", type=Path,
                        default=ROOT / "tools" / "generate_graph.py")
    parser.add_argument("--sizes", nargs="+", type=int, default=DEFAULT_SIZES)
    parser.add_argument("--reachability-percentages", nargs="+", type=int,
                        default=DEFAULT_REACHABILITY_PERCENTAGES,
                        help="porcentajes de n usados como reachability; default: 5 10 15 20 25")
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED,
                        help=f"semilla fija pasada al generador; default: {DEFAULT_SEED}")
    parser.add_argument("--demand-type", choices=("fixed", "integer", "real"),
                        default="fixed",
                        help="tipo de demanda pasado al generador; default: fixed")
    parser.add_argument("--repetitions", type=int, default=1)
    parser.add_argument("--selection-strategy", choices=("deadheadCost", "random"),
                        default=DEFAULT_SELECTION_STRATEGY,
                        help="seleccion de vecindad de Fix-and-Optimize; "
                             f"default: {DEFAULT_SELECTION_STRATEGY}")
    parser.add_argument("--timeout-seconds", type=float, default=300,
                        help="limite por ejecucion; default: 300 (5 minutos)")
    parser.add_argument("--regenerate", action="store_true",
                        help="volver a generar instancias que ya existen")
    parser.add_argument("--rerun", action="store_true",
                        help="repetir combinaciones que ya aparecen en el CSV")
    parser.add_argument("--keep-solutions", action="store_true",
                        help="conservar el out.dat de cada ejecucion")
    parser.add_argument("--plot-only", action="store_true",
                        help="no ejecutar; regenerar plots desde el CSV")
    parser.add_argument("--no-plots", action="store_true",
                        help="ejecutar y guardar CSV sin requerir matplotlib")
    return parser.parse_args(arguments)


def validate(options):
    options.sizes = unique(options.sizes)
    options.reachability_percentages = unique(options.reachability_percentages)
    if not options.sizes or any(size < 3 for size in options.sizes):
        raise ValueError("Todos los tamanos deben ser enteros >= 3.")
    if (not options.reachability_percentages
            or any(value <= 0 or value > 100
                   for value in options.reachability_percentages)):
        raise ValueError("Los porcentajes de reachability deben ser enteros entre 1 y 100.")
    if options.repetitions < 1:
        raise ValueError("repetitions debe ser >= 1.")
    if options.timeout_seconds <= 0:
        raise ValueError("timeout-seconds debe ser positivo.")
    if options.plot_only and options.no_plots:
        raise ValueError("--plot-only y --no-plots no se pueden combinar.")
    if not options.plot_only:
        if not options.solver.is_file():
            raise ValueError(f"No existe el solver: {options.solver}. Ejecute make primero.")
        if not options.generator.is_file():
            raise ValueError(f"No existe el generador: {options.generator}.")


def main():
    options = parse_arguments()
    validate(options)
    output_directory = experiment_output_directory(options.experiment_name).resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    csv_path = output_directory / "resultados.csv"

    if not options.plot_only:
        instance_root = (output_directory / "instances" /
                         f"demand_type_{options.demand_type}" /
                         f"seed_{options.seed}")
        instance_root.mkdir(parents=True, exist_ok=True)
        logs = output_directory / "logs" / f"seed_{options.seed}"
        existing = completed_keys(read_rows(csv_path)) if not options.rerun else set()
        total = len(options.sizes) * (1 + len(options.reachability_percentages)
                                      * options.repetitions)
        current = 0
        for size in options.sizes:
            graph, turns = generate_instance(options.generator.resolve(), instance_root,
                                             size, options.seed, options.demand_type,
                                             options.regenerate)
            configurations = configurations_for(
                size, options.reachability_percentages, options.repetitions,
                options.selection_strategy)
            for (series_name, reachability, repetition, strategy,
                 selection_strategy) in configurations:
                current += 1
                key = (options.seed, size, series_name, repetition,
                       selection_strategy)
                description = ("default (MIP)" if series_name == DEFAULT_SERIES else
                               f"reachability={series_name}% ({reachability}), "
                               f"selection={selection_strategy}")
                if key in existing:
                    print(f"[{current}/{total}] omitido n={size}, {description}, "
                          f"rep={repetition}", flush=True)
                    continue
                print(f"[{current}/{total}] n={size}, {description}, "
                      f"rep={repetition}", flush=True)
                if options.keep_solutions:
                    run_directory = (output_directory / "runs" /
                                     f"seed_{options.seed}" / f"n_{size}" /
                                     ("default" if series_name == DEFAULT_SERIES else
                                      f"p_{series_name}_r_{reachability}_"
                                      f"selection_{selection_strategy}") /
                                     f"rep_{repetition}")
                    run_directory.mkdir(parents=True, exist_ok=True)
                    temporary = None
                else:
                    temporary = tempfile.TemporaryDirectory(
                        prefix="rcpp-reachability-", dir=output_directory)
                    run_directory = Path(temporary.name)
                try:
                    stdout, stderr, returncode, wall_ms, parsed, outcome = run_solver(
                        options.solver.resolve(), graph, turns, reachability,
                        options.timeout_seconds, run_directory, strategy,
                        selection_strategy)
                finally:
                    if temporary is not None:
                        temporary.cleanup()
                log_name = (f"default_rep_{repetition}.log" if
                            series_name == DEFAULT_SERIES else
                            f"percentage_{series_name}_reachability_{reachability}_"
                            f"selection_{selection_strategy}_"
                            f"rep_{repetition}.log")
                log_path = logs / f"n_{size}" / log_name
                write_log(log_path,
                          f"n={size} series={series_name} reachability={reachability} "
                          f"selection_strategy={selection_strategy} "
                          f"repetition={repetition}",
                          stdout, stderr)
                row = make_row(options.seed, size, series_name, reachability,
                               selection_strategy, repetition, wall_ms, parsed,
                               returncode, outcome)
                append_row(csv_path, row)
                print(f"  -> {outcome}, {wall_ms / 1000:.3f} s, "
                      f"optimal={row['optimal']}", flush=True)

    if not options.no_plots:
        plots = plot_results(read_rows(csv_path), options.sizes,
                             options.reachability_percentages, options.seed,
                             output_directory, options.selection_strategy)
        print(f"CSV: {csv_path}")
        for plot in plots:
            print(f"Plot: {plot}")
    else:
        print(f"CSV: {csv_path}")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
