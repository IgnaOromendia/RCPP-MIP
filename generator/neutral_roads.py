"""Shared classification of neutral OSM roads used by cluster visualizers."""

from pathlib import Path


def read_neutral_nodes(path: Path) -> dict[int, tuple[str, ...]]:
    try:
        rows = [row.strip() for row in Path(path).read_text(
            encoding="utf-8").splitlines() if row.strip()]
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if not rows or rows[0] != "node_id reasons":
        raise ValueError(f"{path}: cabecera de nodos neutros invalida")
    result = {}
    for line_number, row in enumerate(rows[1:], start=2):
        fields = row.split()
        if len(fields) != 2:
            raise ValueError(f"{path}: linea {line_number}: nodo neutro invalido")
        try:
            node = int(fields[0])
        except ValueError as error:
            raise ValueError(
                f"{path}: linea {line_number}: nodo neutro invalido"
            ) from error
        reasons = tuple(sorted(set(fields[1].split(","))))
        if node <= 0 or node in result or any(not reason for reason in reasons):
            raise ValueError(f"{path}: linea {line_number}: nodo neutro invalido")
        result[node] = reasons
    return result


def divider_path_ids(reasons: set[str]) -> set[str]:
    """Return stable path identifiers stored alongside the divider marker."""
    return {
        reason for reason in reasons
        if reason.startswith("divider_") and reason[8:].isdigit()
    }


def is_divider_edge(source_reasons: set[str], target_reasons: set[str]) -> bool:
    """Recognize one selected path without joining unrelated divider nodes."""
    source_paths = divider_path_ids(source_reasons)
    target_paths = divider_path_ids(target_reasons)
    if source_paths or target_paths:
        return bool(source_paths & target_paths)
    return "divider" in source_reasons and "divider" in target_reasons


def original_boundary_reasons(reasons: set[str]) -> set[str]:
    """Discard generated divider metadata from original road classifications."""
    return {
        reason for reason in reasons
        if reason != "divider" and not reason.startswith("divider_")
    }


def is_original_boundary_edge(
    source_reasons: set[str], target_reasons: set[str]
) -> bool:
    return bool(
        original_boundary_reasons(source_reasons)
        and original_boundary_reasons(target_reasons)
    )


def neutral_edge_kind(
    source: int, target: int, neutral_nodes: dict[int, tuple[str, ...]]
) -> str | None:
    """Return ``divider``, ``boundary`` or ``None`` for a graph edge."""
    source_reasons = set(neutral_nodes.get(source, ()))
    target_reasons = set(neutral_nodes.get(target, ()))
    if is_divider_edge(source_reasons, target_reasons):
        return "divider"
    if is_original_boundary_edge(source_reasons, target_reasons):
        return "boundary"
    return None
