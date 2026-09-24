#!/usr/bin/env python3
"""Compare the default MIP with every Fix-and-Optimize selection strategy.

The reachability is supplied by the caller. Top-K uses the k=15 value defined
inside FixAndOptimize::solve(). The runner generates one reproducible graph per
size, persists every result immediately, and resumes completed configurations.
"""

import argparse
import csv
import os
from pathlib import Path
import statistics
import subprocess
import sys
import tempfile
import time

from reachability_experiments import (DEFAULT_SIZES, DEFAULT_SEED,
                                      DEFAULT_SERIES, ROOT,
                                      SELECTION_STRATEGIES,
                                      generate_instance,
                                      format_run_result,
                                      normalize_objectives_by_size,
                                      parse_experiment_name,
                                      parse_solver_result, unique, used_sizes,
                                      write_log)


FIELDS = ["seed", "size", "reachability", "selection_strategy", "top_k",
          "repetition", "elapsed_ms", "wall_ms", "has_solution", "optimal",
          "status", "objective", "returncode", "outcome"]


def experiment_output_directory(name):
    parse_experiment_name(name)
    return ROOT / "experiments" / name


def configurations_for(repetitions, selection_strategies=SELECTION_STRATEGIES):
    """Return the MIP baseline followed by every F&O selection strategy."""
    return [(DEFAULT_SERIES, 1, "mip"), *(
        (selection_strategy, repetition, "fixAndOptimize")
        for selection_strategy in selection_strategies
        for repetition in range(1, repetitions + 1)
    )]


def plot_series(selection_strategies=SELECTION_STRATEGIES):
    return [DEFAULT_SERIES, *selection_strategies]


def read_rows(csv_path):
    if not csv_path.exists():
        return []
    with csv_path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        if reader.fieldnames != FIELDS:
            raise ValueError(f"Columnas incompatibles en {csv_path}")
        return list(reader)


def append_row(csv_path, row):
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    new_file = not csv_path.exists() or csv_path.stat().st_size == 0
    if not new_file:
        read_rows(csv_path)
    with csv_path.open("a", newline="", encoding="utf-8") as target:
        writer = csv.DictWriter(target, fieldnames=FIELDS)
        if new_file:
            writer.writeheader()
        writer.writerow(row)
        target.flush()


def completed_keys(rows):
    return {(int(row["seed"]), int(row["size"]), row["reachability"],
             row["selection_strategy"], int(row["repetition"])) for row in rows}


def run_solver(solver, graph, turns, reachability, selection_strategy,
               run_directory, strategy):
    command = [str(solver), str(graph), str(turns), strategy]
    if strategy == "mip":
        if selection_strategy != DEFAULT_SERIES:
            raise ValueError("mip no recibe selection_strategy")
    elif strategy == "fixAndOptimize":
        if selection_strategy not in SELECTION_STRATEGIES:
            raise ValueError(f"Selection strategy desconocida: {selection_strategy}")
        command.extend((str(reachability), selection_strategy))
    else:
        raise ValueError(f"Estrategia desconocida: {strategy}")

    started = time.perf_counter()
    result = subprocess.run(command, cwd=run_directory, capture_output=True, text=True)
    wall_ms = (time.perf_counter() - started) * 1000
    parsed = parse_solver_result(result.stdout)
    outcome = "completed" if result.returncode in (0, 2) and parsed else "error"
    return result.stdout, result.stderr, result.returncode, wall_ms, parsed, outcome


def make_row(seed, size, reachability, selection_strategy, repetition, wall_ms,
             parsed, returncode, outcome):
    is_default = selection_strategy == DEFAULT_SERIES
    return {
        "seed": seed,
        "size": size,
        "reachability": "" if is_default else reachability,
        "selection_strategy": selection_strategy,
        "top_k": 15 if selection_strategy == "topKDeadheadCost" else "",
        "repetition": repetition,
        "elapsed_ms": "" if parsed is None else f'{parsed["elapsed_ms"]:.6f}',
        "wall_ms": f"{wall_ms:.6f}",
        "has_solution": "false" if parsed is None else str(parsed["has_solution"]).lower(),
        "optimal": "false" if parsed is None else str(parsed["optimal"]).lower(),
        "status": outcome.upper(),
        "objective": "" if parsed is None or parsed["objective"] is None
                     else f'{parsed["objective"]:.17g}',
        "returncode": returncode,
        "outcome": outcome,
    }


def series_label(series_name):
    if series_name == DEFAULT_SERIES:
        return "default (MIP)"
    if series_name == "topKDeadheadCost":
        return f"{series_name} (k=15 interno)"
    return series_name


def plot_results(rows, sizes, seed, reachability, output_directory):
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

    series = plot_series()
    selected = [row for row in rows
                if int(row["seed"]) == seed
                and int(row["size"]) in sizes
                and row["selection_strategy"] in series
                and (row["selection_strategy"] == DEFAULT_SERIES
                     or int(row["reachability"]) == reachability)]
    if not selected:
        raise RuntimeError("No hay resultados para graficar con esta configuracion.")
    plotted_sizes = used_sizes(selected)

    fig, ax = plt.subplots(figsize=(10, 6))
    for series_name in series:
        x_values, y_values = [], []
        for size in plotted_sizes:
            measurements = [float(row["elapsed_ms"]) / 1000
                            for row in selected
                            if row["selection_strategy"] == series_name
                            and int(row["size"]) == size
                            and row["outcome"] == "completed"
                            and row["elapsed_ms"]]
            if measurements:
                x_values.append(size)
                y_values.append(statistics.median(measurements))
        if x_values:
            style = {"color": "black", "linestyle": "--"} if (
                series_name == DEFAULT_SERIES) else {}
            ax.plot(x_values, y_values, marker="o", linewidth=1.8,
                    label=series_label(series_name), **style)
    ax.set_xlabel("Cantidad de nodos")
    ax.set_ylabel("Tiempo total del solver (s, mediana)")
    ax.set_title(f"Tiempo por estrategia (reachability={reachability})")
    ax.set_xscale("log")
    ax.set_xticks(plotted_sizes, [f"{size:,}" for size in plotted_sizes],
                  rotation=45, ha="right")
    ax.xaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    time_plot = output_directory / "tiempo_por_estrategia.png"
    fig.savefig(time_plot, dpi=180)
    plt.close(fig)

    objectives = [[None for _ in plotted_sizes] for _ in series]
    labels = [["" for _ in plotted_sizes] for _ in series]
    for row_index, series_name in enumerate(series):
        for column_index, size in enumerate(plotted_sizes):
            attempts = [row for row in selected
                        if row["selection_strategy"] == series_name
                        and int(row["size"]) == size]
            values = [float(row["objective"]) for row in attempts
                      if row.get("has_solution") == "true" and row.get("objective")]
            if values:
                objective = statistics.median(values)
                objectives[row_index][column_index] = objective
                labels[row_index][column_index] = f"{objective:,.2f}"

    matrix = np.array([
        [np.nan if value is None else value for value in row]
        for row in normalize_objectives_by_size(objectives)
    ])
    fig, ax = plt.subplots(figsize=(max(10, 0.9 * len(plotted_sizes)),
                                    1.2 + 0.65 * len(series)))
    color_map = plt.get_cmap("RdYlGn_r").copy()
    color_map.set_bad("#d1d5db")
    image = ax.imshow(np.ma.masked_invalid(matrix), vmin=0, vmax=1,
                      cmap=color_map, aspect="auto")
    ax.set_xticks(range(len(plotted_sizes)),
                  [f"{size:,}" for size in plotted_sizes], rotation=45, ha="right")
    ax.set_yticks(range(len(series)), [series_label(value) for value in series])
    ax.set_xlabel("Cantidad de nodos")
    ax.set_ylabel("Estrategia")
    ax.set_title(f"Diferencia del objetivo (reachability={reachability})")
    for row_index in range(len(series)):
        for column_index in range(len(plotted_sizes)):
            ax.text(column_index, row_index,
                    labels[row_index][column_index] or "Sin dato",
                    ha="center", va="center", fontsize=8)
    colorbar = fig.colorbar(image, ax=ax, fraction=0.03, pad=0.03)
    colorbar.set_ticks([0, 1], labels=["Mejor (0%)", "Peor (≥ 15%)"])
    colorbar.set_label("Diferencia respecto del menor objetivo de cada n")
    fig.tight_layout()
    objective_plot = output_directory / "optimalidad_por_estrategia.png"
    fig.savefig(objective_plot, dpi=180)
    plt.close(fig)
    return time_plot, objective_plot


def parse_arguments(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("experiment_name", type=parse_experiment_name,
                        help="nombre; guarda resultados en experiments/<nombre>")
    parser.add_argument("--reachability", type=int, required=True,
                        help="radio BFS usado por todas las estrategias F&O")
    parser.add_argument("--solver", type=Path, default=ROOT / "solverExec")
    parser.add_argument("--generator", type=Path,
                        default=ROOT / "tools" / "generate_graph.py")
    parser.add_argument("--sizes", nargs="+", type=int, default=DEFAULT_SIZES)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--demand-type", choices=("fixed", "integer", "real"),
                        default="real")
    parser.add_argument("--repetitions", type=int, default=1)
    parser.add_argument("--regenerate", action="store_true")
    parser.add_argument("--rerun", action="store_true")
    parser.add_argument("--keep-solutions", action="store_true")
    parser.add_argument("--plot-only", action="store_true")
    parser.add_argument("--no-plots", action="store_true")
    return parser.parse_args(arguments)


def validate(options):
    options.sizes = unique(options.sizes)
    if not options.sizes or any(size < 3 for size in options.sizes):
        raise ValueError("Todos los tamanos deben ser enteros >= 3.")
    if options.reachability < 0:
        raise ValueError("reachability debe ser un entero no negativo.")
    if options.repetitions < 1:
        raise ValueError("repetitions debe ser >= 1.")
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
        total = len(options.sizes) * (1 + len(SELECTION_STRATEGIES)
                                      * options.repetitions)
        current = 0
        for size in options.sizes:
            graph, turns = generate_instance(options.generator.resolve(), instance_root,
                                             size, options.seed, options.demand_type,
                                             options.regenerate)
            for selection_strategy, repetition, strategy in configurations_for(
                    options.repetitions):
                current += 1
                is_default = selection_strategy == DEFAULT_SERIES
                key = (options.seed, size,
                       "" if is_default else str(options.reachability),
                       selection_strategy, repetition)
                description = ("default (MIP)" if is_default else
                               f"selection={series_label(selection_strategy)}")
                if key in existing:
                    print(f"[{current}/{total}] omitido n={size}, {description}, "
                          f"rep={repetition}", flush=True)
                    continue
                print(f"[{current}/{total}] n={size}, {description}, rep={repetition}",
                      flush=True)
                if options.keep_solutions:
                    run_directory = (output_directory / "runs" /
                                     f"seed_{options.seed}" / f"n_{size}" /
                                     ("default" if is_default else
                                      f"reachability_{options.reachability}_"
                                      f"selection_{selection_strategy}") /
                                     f"rep_{repetition}")
                    run_directory.mkdir(parents=True, exist_ok=True)
                    temporary = None
                else:
                    temporary = tempfile.TemporaryDirectory(
                        prefix="rcpp-selection-", dir=output_directory)
                    run_directory = Path(temporary.name)
                try:
                    result = run_solver(options.solver.resolve(), graph, turns,
                                        options.reachability, selection_strategy,
                                        run_directory, strategy)
                finally:
                    if temporary is not None:
                        temporary.cleanup()
                stdout, stderr, returncode, wall_ms, parsed, outcome = result
                log_name = (f"default_rep_{repetition}.log" if is_default else
                            f"reachability_{options.reachability}_"
                            f"selection_{selection_strategy}_rep_{repetition}.log")
                write_log(logs / f"n_{size}" / log_name,
                          f"n={size} reachability={options.reachability} "
                          f"selection_strategy={selection_strategy} "
                          f"repetition={repetition}", stdout, stderr)
                row = make_row(options.seed, size, options.reachability,
                               selection_strategy, repetition, wall_ms, parsed,
                               returncode, outcome)
                append_row(csv_path, row)
                print(f"  -> {format_run_result(row, wall_ms)}", flush=True)

    if not options.no_plots:
        plots = plot_results(read_rows(csv_path), options.sizes, options.seed,
                             options.reachability, output_directory)
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
