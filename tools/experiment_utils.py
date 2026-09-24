"""Shared helpers for solver experiment runners."""

import argparse
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SIZES = [100, 140, 180, 220, 260, 300]
DEFAULT_SEED = 0
DEFAULT_SERIES = "default"
SELECTION_STRATEGIES = ("random", "maxDeadheadCost", "topKDeadheadCost")
WORST_OBJECTIVE_DIFFERENCE = 0.15
RESULT_PATTERN = re.compile(
    r"^RCPP_RESULT elapsed_ms=(?P<elapsed_ms>\S+) "
    r"has_solution=(?P<has_solution>true|false) "
    r"optimal=(?P<optimal>true|false) "
    r"objective=(?P<objective>\S+)$",
    re.MULTILINE,
)


def unique(values):
    return list(dict.fromkeys(values))


def parse_experiment_name(name):
    if not name or name in {".", ".."} or "/" in name or "\\" in name:
        raise argparse.ArgumentTypeError(
            "el nombre del experimento debe ser un nombre de carpeta, no una ruta")
    return name


def used_sizes(rows):
    return sorted({int(row["size"]) for row in rows})


def normalize_objectives_by_size(objectives):
    if not objectives:
        return []
    normalized = [[None for _ in row] for row in objectives]
    for column_index in range(len(objectives[0])):
        values = [row[column_index] for row in objectives
                  if row[column_index] is not None]
        if not values:
            continue
        best = min(values)
        tolerance = 1e-9 * max(1.0, *(abs(value) for value in values))
        for row_index, row in enumerate(objectives):
            value = row[column_index]
            if value is None:
                continue
            difference = value - best
            if difference <= tolerance:
                score = 0.0
            elif abs(best) <= tolerance:
                score = 1.0
            else:
                score = min((difference / abs(best)) /
                            WORST_OBJECTIVE_DIFFERENCE, 1.0)
            normalized[row_index][column_index] = score
    return normalized


def parse_solver_result(stdout):
    matches = list(RESULT_PATTERN.finditer(stdout))
    if not matches:
        return None
    values = matches[-1].groupdict()
    values["elapsed_ms"] = float(values["elapsed_ms"])
    values["has_solution"] = values["has_solution"] == "true"
    values["optimal"] = values["optimal"] == "true"
    values["objective"] = (None if values["objective"] == "NA"
                           else float(values["objective"]))
    return values


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


def write_log(path, command_description, stdout, stderr):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        f"{command_description}\n\n--- stdout ---\n{stdout}"
        f"\n--- stderr ---\n{stderr}",
        encoding="utf-8",
    )


def format_run_result(row, wall_ms):
    objective = row["objective"] or "NA"
    return (f"resultado={objective}, tiempo={wall_ms / 1000:.3f} s, "
            f"optimo={row['optimal']}")
