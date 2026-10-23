#!/usr/bin/env python3
"""Project an RCPP solution from an expanded graph onto its original graph."""

from __future__ import annotations

import argparse
from collections import defaultdict
import heapq
from pathlib import Path
import re
import sys

from tune_cluster_crossing import GraphData, read_graph


MAPPING_HEADER = "expanded_node original_node"
SECTION_HEADERS = {
    "---- X ----": "service",
    "---- Y ----": "traversals",
    "---- YDK & YKD ----": "deposit",
    "---- F ----": "ignored",
    "---- FDK ----": "ignored",
}
VARIABLE = re.compile(
    r"^([XY])_([^_]+)_([^_]+)_([1-9][0-9]*)\s*=\s*([0-9]+)$"
)


class TurnFlowError(ValueError):
    def __init__(self, arrivals: dict[int, int], departures: dict[int, int]):
        super().__init__(
            "las pasadas fisicas no admiten una conexion que respete los giros originales"
        )
        self.arrivals = arrivals
        self.departures = departures


class OriginalTopology:
    def __init__(self, graph: GraphData, illegal_turns: set[tuple[int, int, int]]):
        self.physical_arcs: dict[tuple[int, int], tuple[int, int]] = {}
        self.arrivals: dict[int, list[int]] = defaultdict(list)
        self.departures: dict[int, list[int]] = defaultdict(list)
        self.allowed_turns: set[tuple[int, int]] = set()
        self.deposit_arcs: set[tuple[int, int]] = set()
        self.reverse_physical_arcs: dict[tuple[int, int], tuple[int, int]] = {}
        self.physical_arc_costs: dict[tuple[int, int], float] = {}
        previous_by_arrival: dict[int, int] = {}
        following_by_departure: dict[int, int] = {}

        for index, record in enumerate(graph.records):
            if index < graph.edge_count:
                first = 4 * index + 1
                oriented = (
                    (first, first + 1, record.source, record.target),
                    (first + 2, first + 3, record.target, record.source),
                )
            else:
                first = 4 * graph.edge_count + 2 * (index - graph.edge_count) + 1
                oriented = ((first, first + 1, record.source, record.target),)
            if index < graph.edge_count:
                forward = oriented[0][:2]
                reverse = oriented[1][:2]
                self.reverse_physical_arcs[forward] = reverse
                self.reverse_physical_arcs[reverse] = forward
            for source_virtual, target_virtual, source, target in oriented:
                self.physical_arcs[source_virtual, target_virtual] = (source, target)
                self.physical_arc_costs[source_virtual, target_virtual] = float(record.cost)
                self.departures[source].append(source_virtual)
                self.arrivals[target].append(target_virtual)
                following_by_departure[source_virtual] = target
                previous_by_arrival[target_virtual] = source
                if source in graph.deposit_nodes:
                    self.deposit_arcs.add((-1, source_virtual))
                if target in graph.deposit_nodes:
                    self.deposit_arcs.add((target_virtual, -1))

        for node, arrivals in self.arrivals.items():
            departures = self.departures.get(node, [])
            for arrival in arrivals:
                previous = previous_by_arrival[arrival]
                for departure in departures:
                    following = following_by_departure[departure]
                    if previous == following:
                        continue
                    if (previous, node, following) in illegal_turns:
                        continue
                    self.allowed_turns.add((arrival, departure))


def read_illegal_turns(path: Path, node_count: int) -> set[tuple[int, int, int]]:
    try:
        tokens = path.read_text(encoding="utf-8").split()
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if len(tokens) < 2:
        raise ValueError(f"{path}: cabecera incompleta")
    try:
        turn_count, illegal_count = map(int, tokens[:2])
    except ValueError as error:
        raise ValueError(f"{path}: cabecera invalida") from error
    if turn_count < 0 or illegal_count < 0 or len(tokens) != 2 + 3 * (turn_count + illegal_count):
        raise ValueError(f"{path}: cantidad de campos invalida")
    triples = []
    for offset in range(2, len(tokens), 3):
        try:
            triple = tuple(map(int, tokens[offset:offset + 3]))
        except ValueError as error:
            raise ValueError(f"{path}: giro invalido") from error
        if any(node < 1 or node > node_count for node in triple):
            raise ValueError(f"{path}: giro con nodo fuera de rango")
        triples.append(triple)
    return set(triples[turn_count:])


def read_mapping(path: Path) -> dict[int, int | None]:
    try:
        rows = [line.strip() for line in path.read_text(encoding="utf-8").splitlines()
                if line.strip() and not line.lstrip().startswith("#")]
    except OSError as error:
        raise ValueError(f"no se pudo leer {path}: {error}") from error
    if not rows or rows[0] != MAPPING_HEADER:
        raise ValueError(f"{path}: cabecera invalida")

    mapping: dict[int, int | None] = {}
    for line_number, row in enumerate(rows[1:], start=2):
        fields = row.split()
        if len(fields) != 2:
            raise ValueError(f"{path}: fila {line_number} invalida")
        try:
            expanded = int(fields[0])
            original = None if fields[1] == "-" else int(fields[1])
        except ValueError as error:
            raise ValueError(f"{path}: fila {line_number} invalida") from error
        if expanded <= 0 or (original is not None and original <= 0):
            raise ValueError(f"{path}: fila {line_number}: nodo invalido")
        if expanded in mapping:
            raise ValueError(f"{path}: nodo expandido duplicado {expanded}")
        mapping[expanded] = original
    if not mapping or set(mapping) != set(range(1, max(mapping) + 1)):
        raise ValueError(f"{path}: el mapeo de nodos expandidos no es contiguo")
    return mapping


def _parse_node(token: str, mapping: dict[int, int | None], context: str
                ) -> tuple[str, int]:
    if token == "D":
        return "D", -1
    try:
        expanded = int(token)
    except ValueError as error:
        raise ValueError(f"{context}: nodo invalido") from error
    if expanded not in mapping:
        raise ValueError(f"{context}: nodo {expanded} ausente del mapeo")
    original = mapping[expanded]
    # -1 is reserved for the deposit. Negative values below it preserve the
    # identity of synthetic nodes while they are being contracted.
    return token, original if original is not None else -expanded - 1


def _contract_synthetic_nodes(
    arcs: dict[tuple[int, int, int], int],
) -> dict[tuple[int, int, int], int]:
    """Contract positive integer walks through nodes mapped as synthetic."""
    output: dict[tuple[int, int, int], int] = defaultdict(int)
    by_vehicle: dict[int, dict[tuple[int, int], int]] = defaultdict(
        lambda: defaultdict(int)
    )
    for (source, target, vehicle), count in arcs.items():
        if count:
            by_vehicle[vehicle][source, target] += count

    for vehicle, counts in by_vehicle.items():
        adjacency: dict[int, list[int]] = defaultdict(list)
        for source, target in counts:
            adjacency[source].append(target)
        for targets in adjacency.values():
            targets.sort()

        direct = [(source, target, count) for (source, target), count in counts.items()
                  if source >= -1 and target >= -1]
        for source, target, count in direct:
            output[source, target, vehicle] += count
            counts[source, target] = 0

        remaining_edges = sum(counts.values())

        starts = sorted(
            ((source, target) for (source, target), count in counts.items()
             if count and source >= -1 and target < -1),
            key=lambda edge: edge[0],
        )
        for source, first_synthetic in starts:
            while counts[source, first_synthetic] > 0:
                counts[source, first_synthetic] -= 1
                remaining_edges -= 1
                current = first_synthetic
                remaining = remaining_edges + 1
                while current < -1:
                    next_node = next(
                        (target for target in adjacency[current]
                         if counts[current, target] > 0),
                        None,
                    )
                    if next_node is None:
                        raise ValueError(
                            "la solucion expandida no conserva el flujo en un nodo sintetico"
                        )
                    counts[current, next_node] -= 1
                    remaining_edges -= 1
                    current = next_node
                    remaining -= 1
                    if remaining < 0:
                        raise ValueError("la solucion contiene un ciclo sintetico no contraible")
                output[source, current, vehicle] += 1

        if any(count for count in counts.values()):
            raise ValueError("la solucion contiene recorridos sinteticos aislados")
    return dict(output)


def _maximum_turn_flow(
    arrivals: dict[int, int],
    departures: dict[int, int],
    allowed_turns: set[tuple[int, int]],
    preferred: dict[tuple[int, int], int] | None = None,
) -> dict[tuple[int, int], int]:
    """Match legal turns while retaining as many preferred connectors as possible."""
    preferred = preferred or {}
    arrival_nodes = sorted(arrivals)
    departure_nodes = sorted(departures)
    source = 0
    arrival_index = {node: index + 1 for index, node in enumerate(arrival_nodes)}
    departure_offset = 1 + len(arrival_nodes)
    departure_index = {
        node: departure_offset + index for index, node in enumerate(departure_nodes)
    }
    sink = departure_offset + len(departure_nodes)
    adjacency: list[list[list[int]]] = [[] for _ in range(sink + 1)]
    turn_edges: dict[tuple[int, int], list[list[int]]] = defaultdict(list)

    def add_edge(left: int, right: int, capacity: int, cost: int) -> list[int]:
        forward = [right, len(adjacency[right]), capacity, cost, capacity]
        reverse = [left, len(adjacency[left]), 0, -cost, 0]
        adjacency[left].append(forward)
        adjacency[right].append(reverse)
        return forward

    total = sum(arrivals.values())
    for node, count in arrivals.items():
        add_edge(source, arrival_index[node], count, 0)
    for node, count in departures.items():
        add_edge(departure_index[node], sink, count, 0)
    for arrival, departure in sorted(allowed_turns):
        if arrival in arrivals and departure in departures:
            retained = min(preferred.get((arrival, departure), 0),
                           arrivals[arrival], departures[departure])
            if retained:
                turn_edges[arrival, departure].append(add_edge(
                    arrival_index[arrival], departure_index[departure], retained, 0
                ))
            turn_edges[arrival, departure].append(add_edge(
                arrival_index[arrival], departure_index[departure], total, 1
            ))

    flow = 0
    while flow < total:
        infinity = 10 ** 18
        distance = [infinity] * len(adjacency)
        parent: list[tuple[int, int] | None] = [None] * len(adjacency)
        queued = [False] * len(adjacency)
        distance[source] = 0
        pending = [source]
        queued[source] = True
        cursor = 0
        while cursor < len(pending):
            current = pending[cursor]
            cursor += 1
            queued[current] = False
            for edge_index, edge in enumerate(adjacency[current]):
                neighbour, _reverse, capacity, cost, _initial = edge
                if capacity and distance[neighbour] > distance[current] + cost:
                    distance[neighbour] = distance[current] + cost
                    parent[neighbour] = (current, edge_index)
                    if not queued[neighbour]:
                        queued[neighbour] = True
                        pending.append(neighbour)
        if parent[sink] is None:
            break
        amount = total - flow
        current = sink
        while current != source:
            previous, edge_index = parent[current]
            amount = min(amount, adjacency[previous][edge_index][2])
            current = previous
        current = sink
        while current != source:
            previous, edge_index = parent[current]
            edge = adjacency[previous][edge_index]
            edge[2] -= amount
            adjacency[current][edge[1]][2] += amount
            current = previous
        flow += amount

    result = {}
    for (arrival, departure), edges in turn_edges.items():
        amount = sum(edge[4] - edge[2] for edge in edges)
        if amount:
            result[arrival, departure] = amount
    if flow != total:
        matched_arrivals: dict[int, int] = defaultdict(int)
        matched_departures: dict[int, int] = defaultdict(int)
        for (arrival, departure), amount in result.items():
            matched_arrivals[arrival] += amount
            matched_departures[departure] += amount
        raise TurnFlowError(
            {node: count - matched_arrivals[node] for node, count in arrivals.items()
             if count > matched_arrivals[node]},
            {node: count - matched_departures[node] for node, count in departures.items()
             if count > matched_departures[node]},
        )
    return result


def _remove_deadhead_subtours(
    service: dict[tuple[int, int, int], int],
    traversals: dict[tuple[int, int, int], int],
) -> dict[tuple[int, int, int], int]:
    """Drop disconnected traversal-only components after turn reconstruction."""
    vehicles = {vehicle for _source, _target, vehicle in service}
    vehicles.update(vehicle for _source, _target, vehicle in traversals)
    removable: set[tuple[int, int, int]] = set()

    for vehicle in vehicles:
        adjacency: dict[int, set[int]] = defaultdict(set)
        service_nodes: set[int] = set()
        for (source, target, current_vehicle), count in service.items():
            if current_vehicle != vehicle or not count:
                continue
            adjacency[source].add(target)
            adjacency[target].add(source)
            service_nodes.update((source, target))
        for (source, target, current_vehicle), count in traversals.items():
            if current_vehicle != vehicle or not count:
                continue
            adjacency[source].add(target)
            adjacency[target].add(source)

        connected_to_deposit = {-1}
        pending = [-1]
        for node in pending:
            for neighbour in adjacency.get(node, ()):
                if neighbour not in connected_to_deposit:
                    connected_to_deposit.add(neighbour)
                    pending.append(neighbour)

        disconnected = set(adjacency) - connected_to_deposit
        if disconnected & service_nodes:
            raise ValueError(
                "las pasadas de servicio no forman un circuito conectado al deposito"
            )
        removable.update(
            key for key, count in traversals.items()
            if count and key[2] == vehicle and key[0] in disconnected
        )

    return {
        key: count for key, count in traversals.items()
        if count and key not in removable
    }


def _remove_redundant_backtracks(
    service: dict[tuple[int, int, int], int],
    traversals: dict[tuple[int, int, int], int],
    topology: OriginalTopology,
) -> dict[tuple[int, int, int], int]:
    """Cancel opposite deadheads without removing an edge from the support."""
    cleaned = defaultdict(int, traversals)
    seen: set[tuple[int, int]] = set()
    for forward, reverse in topology.reverse_physical_arcs.items():
        if forward in seen:
            continue
        seen.update((forward, reverse))
        vehicles = {
            vehicle
            for source, target, vehicle in set(service) | set(traversals)
            if (source, target) in (forward, reverse)
        }
        for vehicle in vehicles:
            forward_key = (*forward, vehicle)
            reverse_key = (*reverse, vehicle)
            removable = min(cleaned[forward_key], cleaned[reverse_key])
            remaining_support = (
                service.get(forward_key, 0) + service.get(reverse_key, 0) +
                cleaned[forward_key] + cleaned[reverse_key] - 2 * removable
            )
            if not remaining_support:
                continue
            cleaned[forward_key] -= removable
            cleaned[reverse_key] -= removable
    return {key: count for key, count in cleaned.items() if count}


def _shortest_legal_detour(
    node: int,
    arrivals: dict[int, int],
    departures: dict[int, int],
    topology: OriginalTopology,
) -> list[tuple[int, int]]:
    """Find a physical circuit that separates one incompatible turn."""
    queue: list[tuple[float, tuple[int, int]]] = []
    distance: dict[tuple[int, int], float] = {}
    parent: dict[tuple[int, int], tuple[int, int] | None] = {}
    bad_arrivals = sorted(arrivals)
    bad_departures = set(departures)
    for departure in topology.departures.get(node, []):
        arc = next(
            (candidate for candidate in topology.physical_arcs
             if candidate[0] == departure), None
        )
        if arc is None or not any(
            (arrival, departure) in topology.allowed_turns
            for arrival in bad_arrivals
        ):
            continue
        cost = topology.physical_arc_costs[arc]
        if cost < distance.get(arc, float("inf")):
            distance[arc] = cost
            parent[arc] = None
            heapq.heappush(queue, (cost, arc))

    destination: tuple[int, int] | None = None
    while queue:
        cost, arc = heapq.heappop(queue)
        if cost != distance[arc]:
            continue
        _source_node, target_node = topology.physical_arcs[arc]
        if target_node == node and any(
            (arc[1], departure) in topology.allowed_turns
            for departure in bad_departures
        ):
            destination = arc
            break
        for departure in topology.departures.get(target_node, []):
            if (arc[1], departure) not in topology.allowed_turns:
                continue
            following = next(
                (candidate for candidate in topology.physical_arcs
                 if candidate[0] == departure), None
            )
            if following is None:
                continue
            next_cost = cost + topology.physical_arc_costs[following]
            if next_cost < distance.get(following, float("inf")):
                distance[following] = next_cost
                parent[following] = arc
                heapq.heappush(queue, (next_cost, following))
    if destination is None:
        raise ValueError(
            f"el nodo original {node} no admite un desvio que repare sus giros"
        )
    path = []
    current: tuple[int, int] | None = destination
    while current is not None:
        path.append(current)
        current = parent[current]
    return list(reversed(path))


def _rebuild_original_traversals(
    service: dict[tuple[int, int, int], int],
    traversals: dict[tuple[int, int, int], int],
    topology: OriginalTopology,
) -> dict[tuple[int, int, int], int]:
    """Keep physical/deposit counts and rebuild every legal turn connector."""
    physical: dict[tuple[int, int, int], int] = defaultdict(int)
    deposit: dict[tuple[int, int, int], int] = defaultdict(int)
    vehicles: set[int] = set()
    for (source, target, vehicle), count in traversals.items():
        if not count or source < -1 or target < -1:
            continue
        vehicles.add(vehicle)
        if (source, target) in topology.physical_arcs:
            physical[source, target, vehicle] += count
        elif source == -1 or target == -1:
            if (source, target) not in topology.deposit_arcs:
                raise ValueError("la solucion contiene un conector de deposito inexistente")
            deposit[source, target, vehicle] += count

    for _source, _target, vehicle in service:
        vehicles.add(vehicle)

    physical = _remove_redundant_backtracks(service, physical, topology)

    contracted = _contract_synthetic_nodes(traversals)
    preferred_by_vehicle: dict[int, dict[tuple[int, int], int]] = defaultdict(
        lambda: defaultdict(int)
    )
    for (source, target, vehicle), count in contracted.items():
        if ((source, target) not in topology.physical_arcs and
                (source, target) not in topology.deposit_arcs):
            preferred_by_vehicle[vehicle][source, target] += count

    repairs = 0
    while True:
        result: dict[tuple[int, int, int], int] = defaultdict(int)
        result.update(physical)
        result.update(deposit)
        repaired = False
        for vehicle in sorted(vehicles):
            passes: dict[tuple[int, int], int] = defaultdict(int)
            for (source, target, current_vehicle), count in service.items():
                if current_vehicle == vehicle:
                    passes[source, target] += count
            for (source, target, current_vehicle), count in physical.items():
                if current_vehicle == vehicle:
                    passes[source, target] += count

            arrivals_by_node: dict[int, dict[int, int]] = defaultdict(
                lambda: defaultdict(int)
            )
            departures_by_node: dict[int, dict[int, int]] = defaultdict(
                lambda: defaultdict(int)
            )
            for physical_arc, count in passes.items():
                if physical_arc not in topology.physical_arcs:
                    raise ValueError("la solucion contiene una pasada fisica inexistente")
                source_node, target_node = topology.physical_arcs[physical_arc]
                source_virtual, target_virtual = physical_arc
                departures_by_node[source_node][source_virtual] += count
                arrivals_by_node[target_node][target_virtual] += count

            for (source, target, current_vehicle), count in deposit.items():
                if current_vehicle != vehicle:
                    continue
                if source == -1:
                    physical_arc = next(
                        (arc for arc in passes if arc[0] == target), None
                    )
                    if physical_arc is None:
                        raise ValueError(
                            "un conector de deposito no alimenta una pasada fisica"
                        )
                    node = topology.physical_arcs[physical_arc][0]
                    departures_by_node[node][target] -= count
                else:
                    physical_arc = next(
                        (arc for arc in passes if arc[1] == source), None
                    )
                    if physical_arc is None:
                        raise ValueError(
                            "un conector de deposito no recibe una pasada fisica"
                        )
                    node = topology.physical_arcs[physical_arc][1]
                    arrivals_by_node[node][source] -= count

            for node in set(arrivals_by_node) | set(departures_by_node):
                arrivals = {
                    virtual: count
                    for virtual, count in arrivals_by_node[node].items()
                    if count > 0
                }
                departures = {
                    virtual: count
                    for virtual, count in departures_by_node[node].items()
                    if count > 0
                }
                if any(count < 0 for count in arrivals_by_node[node].values()) or any(
                    count < 0 for count in departures_by_node[node].values()
                ):
                    raise ValueError("los conectores de deposito exceden las pasadas fisicas")
                if sum(arrivals.values()) != sum(departures.values()):
                    raise ValueError("la solucion no conserva las pasadas en un nodo original")
                allowed = {
                    (arrival, departure)
                    for arrival in arrivals
                    for departure in departures
                    if (arrival, departure) in topology.allowed_turns
                }
                preferred = {
                    (arrival, departure): preferred_by_vehicle[vehicle][arrival, departure]
                    for arrival, departure in allowed
                    if preferred_by_vehicle[vehicle][arrival, departure]
                }
                try:
                    turns = _maximum_turn_flow(arrivals, departures, allowed, preferred)
                except TurnFlowError as error:
                    for arc in _shortest_legal_detour(
                        node, error.arrivals, error.departures, topology
                    ):
                        key = (*arc, vehicle)
                        physical[key] = physical.get(key, 0) + 1
                    repairs += 1
                    if repairs > len(topology.physical_arcs):
                        raise ValueError("no se pudieron reparar los giros originales")
                    repaired = True
                    break
                for (arrival, departure), count in turns.items():
                    result[arrival, departure, vehicle] += count
            if repaired:
                break
        if not repaired:
            break
    return _remove_deadhead_subtours(service, result)


def remap_solution(contents: str, mapping: dict[int, int | None],
                   topology: OriginalTopology | None = None) -> str:
    objective: str | None = None
    section: str | None = None
    seen: set[tuple[str, str, str, str, int]] = set()
    service: dict[tuple[int, int, int], int] = defaultdict(int)
    traversals: dict[tuple[int, int, int], int] = defaultdict(int)

    for line_number, raw_line in enumerate(contents.splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("OBJ:"):
            if objective is not None:
                raise ValueError("la solucion contiene mas de un objetivo")
            objective = line
            continue
        if line in SECTION_HEADERS:
            section = SECTION_HEADERS[line]
            continue
        if section == "ignored":
            continue
        if section not in {"service", "traversals", "deposit"}:
            raise ValueError(f"linea {line_number}: contenido fuera de una seccion conocida")
        match = VARIABLE.fullmatch(line)
        if match is None:
            raise ValueError(f"linea {line_number}: variable invalida")
        kind, raw_source, raw_target, raw_vehicle, raw_count = match.groups()
        expected = "X" if section == "service" else "Y"
        if kind != expected:
            raise ValueError(f"linea {line_number}: variable en una seccion incorrecta")
        vehicle, count = int(raw_vehicle), int(raw_count)
        variable_key = (section, kind, raw_source, raw_target, vehicle)
        if variable_key in seen:
            raise ValueError(f"linea {line_number}: variable duplicada")
        seen.add(variable_key)
        _, source = _parse_node(raw_source, mapping, f"linea {line_number}")
        _, target = _parse_node(raw_target, mapping, f"linea {line_number}")
        if section == "deposit" and source != -1 and target != -1:
            raise ValueError(f"linea {line_number}: arco de deposito sin D")
        if section != "deposit" and (source == -1 or target == -1):
            raise ValueError(f"linea {line_number}: deposito en una seccion incorrecta")
        if not count:
            continue
        if section == "service":
            if source < -1 or target < -1:
                raise ValueError("una variable de servicio usa una transicion sintetica")
            service[source, target, vehicle] += count
        else:
            traversals[source, target, vehicle] += count

    if objective is None:
        raise ValueError("la solucion no contiene objetivo")
    remapped_traversals = (
        _contract_synthetic_nodes(traversals)
        if topology is None
        else _rebuild_original_traversals(service, traversals, topology)
    )

    def node_name(node: int) -> str:
        return "D" if node == -1 else str(node)

    service_lines = [
        f"X_{node_name(source)}_{node_name(target)}_{vehicle} = {count}"
        for (source, target, vehicle), count in sorted(service.items())
    ]
    traversal_lines = []
    deposit_lines = []
    for (source, target, vehicle), count in sorted(remapped_traversals.items()):
        line = f"Y_{node_name(source)}_{node_name(target)}_{vehicle} = {count}"
        (deposit_lines if source == -1 or target == -1 else traversal_lines).append(line)

    sections = [
        objective,
        "",
        "---- X ----",
        *service_lines,
        "",
        "---- Y ----",
        *traversal_lines,
        "",
        "---- YDK & YKD ----",
        *deposit_lines,
        "",
        "---- F ----",
        "",
        "---- FDK ----",
    ]
    return "\n".join(sections) + "\n"


def parse_arguments(arguments: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=("Contrae los nodos in/out sinteticos de una solucion RCPP "
                     "producida sobre un grafo expandido."),
    )
    parser.add_argument("solution", type=Path, help="solucion del grafo expandido")
    parser.add_argument("mapping", type=Path, help="mapeo emitido por tune_cluster_crossing.py")
    parser.add_argument("output", type=Path, help="solucion remapeada")
    parser.add_argument("--graph", type=Path, help="grafo original para reconstruir giros")
    parser.add_argument("--turns", type=Path, help="giros originales")
    return parser.parse_args(arguments)


def main(arguments: list[str] | None = None) -> int:
    options = parse_arguments(arguments)
    try:
        mapping = read_mapping(options.mapping)
        try:
            contents = options.solution.read_text(encoding="utf-8")
        except OSError as error:
            raise ValueError(f"no se pudo leer {options.solution}: {error}") from error
        if (options.graph is None) != (options.turns is None):
            raise ValueError("--graph y --turns deben usarse juntos")
        topology = None
        if options.graph is not None:
            graph = read_graph(options.graph)
            illegal_turns = read_illegal_turns(options.turns, graph.nodes)
            topology = OriginalTopology(graph, illegal_turns)
        remapped = remap_solution(contents, mapping, topology)
        options.output.parent.mkdir(parents=True, exist_ok=True)
        try:
            options.output.write_text(remapped, encoding="utf-8")
        except OSError as error:
            raise ValueError(f"no se pudo escribir {options.output}: {error}") from error
        print(f"Solucion remapeada guardada en {options.output}")
        return 0
    except ValueError as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
