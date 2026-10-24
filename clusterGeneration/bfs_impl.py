"""Deterministic BFS partitions of required edges, bounded by percentage."""

from collections import defaultdict, deque
import math

try:
    from .divider_impl import _attach_optional, _required_edges
except ImportError:
    from divider_impl import _attach_optional, _required_edges


def bfs_clusters(geometry, percentage):
    if not math.isfinite(percentage) or not 0 < percentage <= 100:
        raise ValueError("el porcentaje debe estar entre 0 y 100")
    edges = _required_edges(geometry)
    limit = math.floor(len(edges) * percentage / 100)
    if limit < 1:
        raise ValueError("el porcentaje es demasiado pequeno para un cluster de una arista")
    incident = defaultdict(list)
    for position, edge in enumerate(edges):
        incident[edge.source].append(position)
        incident[edge.target].append(position)
    unseen = set(range(len(edges)))
    assignments = [0] * len(edges)
    cluster, maximum = 0, 0
    while unseen:
        cluster += 1
        pending = deque([min(unseen)])
        queued = set(pending)
        size = 0
        while pending and size < limit:
            position = pending.popleft()
            unseen.remove(position)
            assignments[position] = cluster
            size += 1
            edge = edges[position]
            neighbours = sorted(set(incident[edge.source] + incident[edge.target]))
            for neighbour in neighbours:
                if neighbour in unseen and neighbour not in queued:
                    queued.add(neighbour)
                    pending.append(neighbour)
        maximum = max(maximum, size)
    all_edges, all_assignments = _attach_optional(geometry, edges, assignments)
    return all_edges, all_assignments, maximum, {}, 0
