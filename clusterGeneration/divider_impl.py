"""Algorithm described by algoritmo-cluster.md."""

from __future__ import annotations

from collections import defaultdict, deque
import heapq
import math

try:
    from .model import CoordinateGeometry, Edge, edge_key, node_sort_key
except ImportError:
    from model import CoordinateGeometry, Edge, edge_key, node_sort_key


def _graph_edges(geometry: CoordinateGeometry) -> list[Edge]:
    return [
        Edge(index, source, target, 1, 0, 0)
        for index, (source, target) in enumerate(geometry.original_edges, start=1)
    ]


def _required_edges(geometry: CoordinateGeometry) -> list[Edge]:
    edges = _graph_edges(geometry)
    if geometry.zones is None:
        return edges
    if len(geometry.zones) != len(edges):
        raise ValueError("las zonas no coinciden con las aristas del grafo")
    required = [edge for edge, zone in zip(edges, geometry.zones) if zone != 0]
    if not required:
        raise ValueError("el grafo no contiene aristas en la zona obligatoria")
    return required


def _local_components(
    edges: list[Edge], neutral: set[str],
    positions: set[int] | None = None,
    boundary_positions: set[int] | None = None,
) -> list[set[int]]:
    """Step 2: components cannot cross neutral nodes or neutral streets."""
    selected = set(range(len(edges))) if positions is None else set(positions)
    boundaries = boundary_positions or {
        position for position in selected
        if edges[position].source in neutral and edges[position].target in neutral
    }
    active = selected - boundaries
    incident: dict[str, list[int]] = defaultdict(list)
    for position in active:
        edge = edges[position]
        if edge.source not in neutral:
            incident[edge.source].append(position)
        if edge.target not in neutral:
            incident[edge.target].append(position)
    adjacency = {position: set() for position in active}
    for positions_at_node in incident.values():
        for offset, left in enumerate(positions_at_node):
            for right in positions_at_node[offset + 1:]:
                adjacency[left].add(right)
                adjacency[right].add(left)
    result = []
    unseen = set(active)
    while unseen:
        start = min(unseen)
        component = {start}
        pending = deque([start])
        unseen.remove(start)
        while pending:
            for neighbour in adjacency[pending.popleft()]:
                if neighbour in unseen:
                    unseen.remove(neighbour)
                    component.add(neighbour)
                    pending.append(neighbour)
        result.append(component)
    return result


def _distance(coordinates: dict[str, tuple[float, float]], source: str,
              target: str) -> float:
    x1, y1 = coordinates[source]
    x2, y2 = coordinates[target]
    latitude = math.radians((x1 + x2) / 2)
    return math.hypot(x2 - x1, (y2 - y1) * math.cos(latitude))


def _boundary_name(
    edge: Edge, street_names: dict[tuple[str, str], str],
    reasons: dict[int, set[str]],
) -> str:
    name = street_names.get(edge_key(edge.source, edge.target))
    if name:
        return f"street:{name}"
    shared = reasons.get(int(edge.source), set()) & reasons.get(int(edge.target), set())
    dividers = sorted(reason for reason in shared if reason.startswith("divider_"))
    return dividers[0] if dividers else f"edge:{edge.index}"


def _bordering_boundaries(
    edges: list[Edge], component: set[int], boundary_positions: set[int],
    street_names: dict[tuple[str, str], str], reasons: dict[int, set[str]],
    coordinates: dict[str, tuple[float, float]],
) -> list[tuple[float, int, str, set[str]]]:
    component_nodes = {
        node for position in component
        for node in (edges[position].source, edges[position].target)
    }
    grouped: dict[str, list[int]] = defaultdict(list)
    for position in boundary_positions:
        edge = edges[position]
        if edge.source in component_nodes or edge.target in component_nodes:
            grouped[_boundary_name(edge, street_names, reasons)].append(position)
    result = []
    for label, positions in grouped.items():
        nodes = {
            node for position in positions
            for node in (edges[position].source, edges[position].target)
        }
        length = sum(
            _distance(coordinates, edges[position].source, edges[position].target)
            for position in positions
        )
        result.append((-length, min(positions), label, nodes))
    return sorted(result)


def _named_components(
    edges: list[Edge], component: set[int],
    street_names: dict[tuple[str, str], str],
) -> dict[str, list[set[int]]]:
    by_name: dict[str, set[int]] = defaultdict(set)
    for position in component:
        name = street_names.get(edge_key(edges[position].source, edges[position].target))
        if name:
            by_name[name].add(position)
    result: dict[str, list[set[int]]] = {}
    for name, positions in by_name.items():
        incident: dict[str, set[int]] = defaultdict(set)
        for position in positions:
            incident[edges[position].source].add(position)
            incident[edges[position].target].add(position)
        unseen = set(positions)
        groups = []
        while unseen:
            start = min(unseen)
            group = {start}
            pending = deque([start])
            unseen.remove(start)
            while pending:
                edge = edges[pending.popleft()]
                for node in (edge.source, edge.target):
                    for neighbour in incident[node] & unseen:
                        unseen.remove(neighbour)
                        group.add(neighbour)
                        pending.append(neighbour)
            groups.append(group)
        result[name] = groups
    return result


def _node_path(edges: list[Edge], positions: set[int], source: str,
               target: str) -> list[str] | None:
    adjacency: dict[str, list[str]] = defaultdict(list)
    for position in positions:
        edge = edges[position]
        adjacency[edge.source].append(edge.target)
        adjacency[edge.target].append(edge.source)
    previous = {source: None}
    pending = deque([source])
    while pending:
        current = pending.popleft()
        if current == target:
            break
        for following in sorted(adjacency[current], key=node_sort_key):
            if following not in previous:
                previous[following] = current
                pending.append(following)
    if target not in previous:
        return None
    path = []
    current: str | None = target
    while current is not None:
        path.append(current)
        current = previous[current]
    return list(reversed(path))


def _path_through(
    edges: list[Edge], positions: set[int], source: str, middle: str, target: str,
) -> list[str] | None:
    left = _node_path(edges, positions, source, middle)
    right = _node_path(edges, positions, middle, target)
    if left is None or right is None:
        return None
    path = left + right[1:]
    return path if len(path) == len(set(path)) else None


def _path_positions(
    edges: list[Edge], positions: set[int], path: list[str],
) -> set[int]:
    traversed = {
        edge_key(source, target) for source, target in zip(path, path[1:])
    }
    return {
        position for position in positions
        if edge_key(edges[position].source, edges[position].target) in traversed
    }


def _choose_divider(
    edges: list[Edge], component: set[int], neutral: set[str],
    boundary_positions: set[int], coordinates: dict[str, tuple[float, float]],
    street_names: dict[tuple[str, str], str], reasons: dict[int, set[str]],
) -> tuple[list[str], set[int], list[set[int]]] | None:
    boundaries = _bordering_boundaries(
        edges, component, boundary_positions, street_names, reasons, coordinates
    )
    if not boundaries:
        return None
    _length, _order, selected_label, selected_nodes = boundaries[0]
    label_by_node: dict[str, set[str]] = defaultdict(set)
    for _length, _order, label, nodes in boundaries:
        for node in nodes:
            label_by_node[node].add(label)

    component_nodes = {
        node for position in component
        for node in (edges[position].source, edges[position].target)
    }
    center = (
        sum(coordinates[node][0] for node in component_nodes) / len(component_nodes),
        sum(coordinates[node][1] for node in component_nodes) / len(component_nodes),
    )
    named = _named_components(edges, component, street_names)
    groups_by_node: dict[str, list[tuple[str, set[int], set[str], set[str]]]] = (
        defaultdict(list)
    )
    for name, groups in named.items():
        for group in groups:
            nodes = {
                node for position in group
                for node in (edges[position].source, edges[position].target)
            }
            starts = sorted(nodes & selected_nodes, key=node_sort_key)
            internal = nodes - neutral
            if not starts or not internal:
                continue
            degree: dict[str, int] = defaultdict(int)
            for position in group:
                degree[edges[position].source] += 1
                degree[edges[position].target] += 1
            street_ends = {node for node, value in degree.items() if value == 1}
            ends = street_ends | {
                node for node in nodes & neutral
                if label_by_node[node] - {selected_label}
            }
            for node in internal:
                groups_by_node[node].append(
                    (name, group, set(starts), ends)
                )
    if not groups_by_node:
        return None

    # Steps 5 and 7-9: longitude is the horizontal coordinate. Start at the
    # candidate nearest the centroid, then alternate right and left.
    horizontal = sorted(
        groups_by_node,
        key=lambda node: (coordinates[node][1], coordinates[node][0], node_sort_key(node)),
    )
    middle = min(
        horizontal,
        key=lambda node: (math.dist(coordinates[node], center), node_sort_key(node)),
    )
    middle_index = horizontal.index(middle)
    candidate_order = [middle]
    offset = 1
    while middle_index + offset < len(horizontal) or middle_index - offset >= 0:
        if middle_index + offset < len(horizontal):
            candidate_order.append(horizontal[middle_index + offset])
        if middle_index - offset >= 0:
            candidate_order.append(horizontal[middle_index - offset])
        offset += 1

    for candidate in candidate_order:
        groups = sorted(
            groups_by_node[candidate], key=lambda item: (min(item[1]), item[0])
        )
        for _name, group, starts, ends in groups:
            for source in sorted(starts, key=node_sort_key):
                for target in sorted(ends, key=node_sort_key):
                    if target == source:
                        continue
                    path = _path_through(edges, group, source, candidate, target)
                    if path is None or any(node in neutral for node in path[1:-1]):
                        continue
                    divider_positions = _path_positions(edges, group, path)
                    new_neutral = neutral | set(path)
                    new_boundaries = boundary_positions | divider_positions
                    parts = _local_components(
                        edges, new_neutral, component, new_boundaries
                    )
                    if len(parts) >= 2:
                        return path, divider_positions, parts
    return None


def _attach_optional(
    geometry: CoordinateGeometry, required: list[Edge], assignments: list[int],
) -> tuple[list[Edge], list[int]]:
    edges = _graph_edges(geometry)
    incident: dict[str, list[int]] = defaultdict(list)
    for position, edge in enumerate(edges):
        incident[edge.source].append(position)
        incident[edge.target].append(position)
    best: list[tuple[int, int] | None] = [None] * len(edges)
    pending = []
    for edge, cluster in zip(required, assignments):
        best[edge.index - 1] = (0, cluster)
        heapq.heappush(pending, (0, cluster, edge.index - 1))
    while pending:
        distance, cluster, position = heapq.heappop(pending)
        if best[position] != (distance, cluster):
            continue
        edge = edges[position]
        neighbours = {
            neighbour for node in (edge.source, edge.target)
            for neighbour in incident[node]
        }
        for neighbour in neighbours:
            candidate = (distance + 1, cluster)
            if best[neighbour] is None or candidate < best[neighbour]:
                best[neighbour] = candidate
                heapq.heappush(pending, (*candidate, neighbour))
    if any(value is None for value in best):
        raise ValueError("hay una arista opcional desconectada de la zona obligatoria")

    return edges, [value[1] for value in best if value is not None]


def straight_divider_clusters(
    geometry: CoordinateGeometry, percentage: float,
    neutral_node_data: dict[int, tuple[str, ...]],
    street_names: dict[tuple[str, str], str] | None = None,
) -> tuple[list[Edge], list[int], int, dict[int, tuple[str, ...]], int]:
    if not math.isfinite(percentage) or not 0 < percentage <= 100:
        raise ValueError("k debe ser un porcentaje mayor que 0 y menor o igual que 100")
    if not geometry.coordinates:
        raise ValueError("el algoritmo requiere --coords")
    edges = _required_edges(geometry)
    required_nodes = {int(node) for edge in edges for node in (edge.source, edge.target)}
    reasons = {
        node: set(values) for node, values in neutral_node_data.items()
        if node in required_nodes and (
            {"avenida", "primary"} & set(values)
            or any(value.startswith("divider_") for value in values)
        )
    }
    neutral = {str(node) for node in reasons}
    boundary_positions = {
        position for position, edge in enumerate(edges)
        if edge.source in neutral and edge.target in neutral
    }
    components = _local_components(edges, neutral, boundary_positions=boundary_positions)
    required_edge_count = len(edges)
    maximum_cluster_edges = math.floor(required_edge_count * percentage / 100)
    if maximum_cluster_edges < 1:
        raise ValueError(
            "el porcentaje es demasiado pequeno para formar un cluster de una arista"
        )
    divider_count = 0
    names = street_names or {}
    while components:
        largest = min(components, key=lambda item: (-len(item), min(item)))
        if len(largest) <= maximum_cluster_edges:
            break
        divider = _choose_divider(
            edges, largest, neutral, boundary_positions,
            geometry.coordinates, names, reasons,
        )
        if divider is None:
            raise ValueError(
                "no existe una calle recta que divida el cluster mas grande "
                f"sin superar el limite de {maximum_cluster_edges} aristas"
            )
        path, path_positions, parts = divider
        divider_count += 1
        label = f"divider_{divider_count}"
        for node in path:
            reasons.setdefault(int(node), set()).update(("divider", label))
        neutral.update(path)
        boundary_positions.update(path_positions)
        components.remove(largest)
        components.extend(parts)

    components.sort(key=min)
    assignments = [0] * len(edges)
    assignment_sizes: dict[int, int] = defaultdict(int)
    for cluster, component in enumerate(components, start=1):
        for position in component:
            assignments[position] = cluster
            assignment_sizes[cluster] += 1
    incident: dict[str, set[int]] = defaultdict(set)
    for position, edge in enumerate(edges):
        if assignments[position]:
            incident[edge.source].add(assignments[position])
            incident[edge.target].add(assignments[position])
    for position, edge in enumerate(edges):
        if not assignments[position]:
            candidates = sorted(
                cluster for cluster in incident[edge.source] | incident[edge.target]
                if assignment_sizes[cluster] < maximum_cluster_edges
            )
            cluster = candidates[0] if candidates else max(assignment_sizes, default=0) + 1
            assignments[position] = cluster
            assignment_sizes[cluster] += 1
            incident[edge.source].add(cluster)
            incident[edge.target].add(cluster)
    maximum = max(assignment_sizes.values(), default=0)
    all_edges, all_assignments = _attach_optional(geometry, edges, assignments)
    normalized = {node: tuple(sorted(values)) for node, values in reasons.items()}
    return all_edges, all_assignments, maximum, normalized, divider_count
