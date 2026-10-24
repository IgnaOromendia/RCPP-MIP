"""Domain records and shared constants for cluster generation."""

from __future__ import annotations

from dataclasses import dataclass
import re

SECTION_HEADERS = {
    "---- X ----": "service",
    "---- Y ----": "traversals",
    "---- YDK & YKD ----": "deposit",
    "---- F ----": "ignored",
    "---- FDK ----": "ignored",
}

VARIABLE = re.compile(r"^([XY])_([^_]+)_([^_]+)_([1-9][0-9]*)\s*=\s*([0-9]+)$")

OUTPUT_NAME = re.compile(r"^min_dist_([1-9][0-9]*)\.dat$")

@dataclass(frozen=True)
class Edge:
    index: int
    source: str
    target: str
    vehicle: int
    service_count: int
    traversal_count: int

    @property
    def passages(self) -> int:
        return self.service_count + self.traversal_count

@dataclass(frozen=True)
class CoordinateGeometry:
    node_count: int
    coordinates: dict[str, tuple[float, float]]
    original_edges: list[tuple[str, str]]
    virtual_to_original: dict[str, str]
    virtual_to_edge: dict[str, int]
    deposit_adjacent: str
    zones: list[int] | None = None

def node_sort_key(node: str) -> tuple[int, int]:
    return (0, 0) if node == "D" else (1, int(node))

def edge_key(source: str, target: str) -> tuple[str, str]:
    return ((source, target) if node_sort_key(source) <= node_sort_key(target)
            else (target, source))

