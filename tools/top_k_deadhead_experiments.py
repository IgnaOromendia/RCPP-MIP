#!/usr/bin/env python3
"""Benchmark Top-K deadhead-cost selection with a fixed reachability.

The runner generates one reproducible two-zone graph per size, with real costs
and real demands, runs the default MIP once and Fix-and-Optimize for every k,
persists each result immediately, and resumes without repeating completed runs.
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

from reachability_experiments import (DEFAULT_SIZES, DEFAULT_SERIES, ROOT,
                                      normalize_objectives_by_size,
                                      parse_experiment_name, parse_solver_result,
                                      unique, used_sizes, write_log)


DEFAULT_K_VALUES = [5, 10, 15]
DEFAULT_REACHABILITY = 5
DEFAULT_SEED = 0
FIELDS = ["seed", "size", "reachability", "top_k", "repetition",
          "elapsed_ms", "wall_ms", "has_solution", "optimal", "status",
          "objective", "returncode", "outcome"]


def experiment_output_directory(name):
    """Return the output directory for a validated experiment name."""
    parse_experiment_name(name)
    return ROOT / "experiments" / name


def configurations_for(k_values, repetitions):
    """Return the MIP baseline followed by all Top-K configurations."""
    return [(DEFAULT_SERIES, 1, "mip"), *(
        (str(top_k), repetition, "fixAndOptimize")
        for top_k in k_values
        for repetition in range(1, repetitions + 1)
    )]


def plot_series(k_values):
    return [DEFAULT_SERIES, *(str(top_k) for top_k in k_values)]


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
             row["top_k"], int(row["repetition"])) for row in rows}


def generate_instance(generator, instance_root, size, seed, regenerate):
    """Generate a graph with real traversal costs/demands and zones 1 and 2."""
    graph = instance_root / "input" / f"graph_{size}.dat"
    turns = instance_root / "input" / f"graph_{size}.turns.dat"
    if regenerate or not (graph.exists() and turns.exists()):
        command = [sys.executable, str(generator), str(size), "--seed", str(seed),
                   "--demand-type", "real"]
        result = subprocess.run(command, cwd=instance_root, capture_output=True,
                                text=True)
        if result.returncode != 0:
            raise RuntimeError(
                f"No se pudo generar n={size} (exit {result.returncode}):\n"
                f"{result.stdout}{result.stderr}")
    return graph.resolve(), turns.resolve()


def run_solver(solver, graph, turns, reachability, top_k, run_directory,
               strategy):
    command = [str(solver), str(graph), str(turns), strategy]
    if strategy == "mip":
        if top_k is not None:
            raise ValueError("mip no recibe top_k")
    elif strategy == "fixAndOptimize":
        if top_k is None or top_k <= 0:
            raise ValueError("fixAndOptimize requiere top_k positivo")
        command.extend((str(reachability), "topKDeadheadCost", str(top_k)))
    else:
        raise ValueError(f"Estrategia desconocida: {strategy}")
    started = time.perf_counter()
    result = subprocess.run(command, cwd=run_directory, capture_output=True, text=True)
    wall_ms = (time.perf_counter() - started) * 1000
    parsed = parse_solver_result(result.stdout)
    outcome = "completed" if result.returncode in (0, 2) and parsed else "error"
    return result.stdout, result.stderr, result.returncode, wall_ms, parsed, outcome


def make_row(seed, size, reachability, top_k, repetition, wall_ms, parsed,
             returncode, outcome):
    return {
        "seed": seed,
        "size": size,
        "reachability": "" if top_k is None else reachability,
        "top_k": DEFAULT_SERIES if top_k is None else top_k,
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


def plot_results(rows, sizes, k_values, seed, reachability, output_directory):
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

    series = plot_series(k_values)
    selected = [row for row in rows
                if int(row["seed"]) == seed
                and int(row["size"]) in sizes
                and row["top_k"] in series
                and (row["top_k"] == DEFAULT_SERIES
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
                            if row["top_k"] == series_name
                            and int(row["size"]) == size
                            and row["outcome"] == "completed"
                            and row["elapsed_ms"]]
            if measurements:
                x_values.append(size)
                y_values.append(statistics.median(measurements))
        if x_values:
            label = ("default (MIP)" if series_name == DEFAULT_SERIES else
                     f"topKDeadheadCost, k={series_name}")
            style = {"color": "black", "linestyle": "--"} if (
                series_name == DEFAULT_SERIES) else {}
            ax.plot(x_values, y_values, marker="o", linewidth=1.8,
                    label=label, **style)
    ax.set_xlabel("Cantidad de nodos")
    ax.set_ylabel("Tiempo total del solver (s, mediana)")
    ax.set_title(f"Tiempo por k (reachability={reachability})")
    ax.set_xscale("log")
    ax.set_xticks(plotted_sizes, [f"{size:,}" for size in plotted_sizes],
                  rotation=45, ha="right")
    ax.xaxis.set_minor_formatter(matplotlib.ticker.NullFormatter())
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()
    time_plot = output_directory / "tiempo_por_top_k.png"
    fig.savefig(time_plot, dpi=180)
    plt.close(fig)

    objectives = [[None for _ in plotted_sizes] for _ in series]
    labels = [["" for _ in plotted_sizes] for _ in series]
    for row_index, series_name in enumerate(series):
        for column_index, size in enumerate(plotted_sizes):
            attempts = [row for row in selected
                        if row["top_k"] == series_name
                        and int(row["size"]) == size]
            values = [float(row["objective"]) for row in attempts
                      if row.get("has_solution") == "true" and row.get("objective")]
            if values:
                objective = statistics.median(values)
                ratio = sum(row["optimal"] == "true" for row in attempts) / len(attempts)
                objectives[row_index][column_index] = objective
                optimality = ("Optimal" if ratio == 1 else "No optimal" if ratio == 0
                              else f"{ratio:.0%} optimal")
                labels[row_index][column_index] = f"{objective:,.2f}\n{optimality}"

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
    ax.set_yticks(range(len(series)),
                  ["default (MIP)" if value == DEFAULT_SERIES else f"k={value}"
                   for value in series])
    ax.set_xlabel("Cantidad de nodos")
    ax.set_ylabel("Top-k deadhead cost")
    ax.set_title(f"Diferencia del objetivo por k (reachability={reachability})")
    for row_index in range(len(series)):
        for column_index in range(len(plotted_sizes)):
            ax.text(column_index, row_index,
                    labels[row_index][column_index] or "Sin dato",
                    ha="center", va="center", fontsize=8)
    colorbar = fig.colorbar(image, ax=ax, fraction=0.03, pad=0.03)
    colorbar.set_ticks([0, 1], labels=["Mejor (0%)", "Peor (≥ 15%)"])
    colorbar.set_label("Diferencia respecto del menor objetivo de cada n")
    fig.tight_layout()
    optimality_plot = output_directory / "optimalidad_por_top_k.png"
    fig.savefig(optimality_plot, dpi=180)
    plt.close(fig)
    return time_plot, optimality_plot


def parse_arguments(arguments=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("experiment_name", type=parse_experiment_name,
                        help="nombre; guarda resultados en experiments/<nombre>")
    parser.add_argument("--solver", type=Path, default=ROOT / "solverExec")
    parser.add_argument("--generator", type=Path,
                        default=ROOT / "tools" / "generate_graph.py")
    parser.add_argument("--sizes", nargs="+", type=int, default=DEFAULT_SIZES)
    parser.add_argument("--k-values", nargs="+", type=int, default=DEFAULT_K_VALUES)
    parser.add_argument("--reachability", type=int, default=DEFAULT_REACHABILITY)
    parser.add_argument("--seed", type=int, default=DEFAULT_SEED)
    parser.add_argument("--repetitions", type=int, default=1)
    parser.add_argument("--regenerate", action="store_true")
    parser.add_argument("--rerun", action="store_true")
    parser.add_argument("--keep-solutions", action="store_true")
    parser.add_argument("--plot-only", action="store_true")
    parser.add_argument("--no-plots", action="store_true")
    return parser.parse_args(arguments)


def validate(options):
    options.sizes = unique(options.sizes)
    options.k_values = unique(options.k_values)
    if not options.sizes or any(size < 3 for size in options.sizes):
        raise ValueError("Todos los tamanos deben ser enteros >= 3.")
    if not options.k_values or any(value <= 0 for value in options.k_values):
        raise ValueError("Todos los valores de k deben ser enteros positivos.")
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
        instance_root = (output_directory / "instances" / "real_cost_real_demand" /
                         f"seed_{options.seed}")
        instance_root.mkdir(parents=True, exist_ok=True)
        logs = output_directory / "logs" / f"seed_{options.seed}"
        existing = completed_keys(read_rows(csv_path)) if not options.rerun else set()
        total = len(options.sizes) * (1 + len(options.k_values) * options.repetitions)
        current = 0
        for size in options.sizes:
            graph, turns = generate_instance(options.generator.resolve(), instance_root,
                                             size, options.seed, options.regenerate)
            for series_name, repetition, strategy in configurations_for(
                    options.k_values, options.repetitions):
                current += 1
                top_k = None if series_name == DEFAULT_SERIES else int(series_name)
                key = (options.seed, size,
                       "" if top_k is None else str(options.reachability),
                       series_name, repetition)
                description = ("default (MIP)" if top_k is None else
                               f"reachability={options.reachability}, k={top_k}")
                if key in existing:
                    print(f"[{current}/{total}] omitido n={size}, {description}, "
                          f"rep={repetition}", flush=True)
                    continue
                print(f"[{current}/{total}] n={size}, {description}, rep={repetition}",
                      flush=True)
                if options.keep_solutions:
                    run_directory = (output_directory / "runs" / f"seed_{options.seed}" /
                                     f"n_{size}" /
                                     ("default" if top_k is None else
                                      f"reachability_{options.reachability}_k_{top_k}") /
                                     f"rep_{repetition}")
                    run_directory.mkdir(parents=True, exist_ok=True)
                    temporary = None
                else:
                    temporary = tempfile.TemporaryDirectory(
                        prefix="rcpp-top-k-", dir=output_directory)
                    run_directory = Path(temporary.name)
                try:
                    result = run_solver(options.solver.resolve(), graph, turns,
                                        options.reachability, top_k, run_directory,
                                        strategy)
                finally:
                    if temporary is not None:
                        temporary.cleanup()
                stdout, stderr, returncode, wall_ms, parsed, outcome = result
                log_name = (f"default_rep_{repetition}.log" if top_k is None else
                            f"reachability_{options.reachability}_k_{top_k}_"
                            f"rep_{repetition}.log")
                write_log(logs / f"n_{size}" / log_name,
                          f"n={size} reachability={options.reachability} "
                          f"top_k={top_k} repetition={repetition}", stdout, stderr)
                row = make_row(options.seed, size, options.reachability, top_k,
                               repetition, wall_ms, parsed, returncode, outcome)
                append_row(csv_path, row)
                print(f"  -> {outcome}, {wall_ms / 1000:.3f} s, "
                      f"optimal={row['optimal']}", flush=True)

    if not options.no_plots:
        plots = plot_results(read_rows(csv_path), options.sizes, options.k_values,
                             options.seed, options.reachability, output_directory)
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
