#!/usr/bin/env python3
"""Generate a connected planar RCPP instance with exactly n original nodes.

Uses only the Python standard library. Zones belong to edges, not vertices.
The outer cycle is optional (0); interior edges are split between zones 1 and 2.
"""

import argparse
from dataclasses import dataclass, field
import math
from numbers import Real
from pathlib import Path
import random


@dataclass
class GeneratedGraph:
    points: list
    edges: list
    contour: list
    vehicles: int
    demand: float
    seed: int
    turns: list = field(default_factory=list)
    illegal_turns: list = field(default_factory=list)
    edge_costs: dict = field(default_factory=dict)
    edge_demands: dict = field(default_factory=dict)
    edge_zones: dict = field(default_factory=dict)

    @property
    def average_degree(self):
        return 2 * len(self.edges) / len(self.points)

    @property
    def contour_edges(self):
        return {edge(u, v) for u, v in
                zip(self.contour, self.contour[1:] + self.contour[:1])}

    def demand_for(self, u, v):
        connection = edge(u, v)
        return 0 if connection in self.contour_edges else self.edge_demands.get(
            connection, self.demand)

    def cost_for(self, u, v):
        return self.edge_costs[edge(u, v)]

    def zone_for(self, u, v):
        return self.edge_zones.get(edge(u, v), 0)


def edge(u, v):
    return min(u, v), max(u, v)


def with_turns(graph, rng):
    """Sample disjoint directed triples, using edge count as the percentage base.

    Counts are rounded down: floor(20% * m) listed, floor(10% * m) illegal.
    Reverse triples are distinct turns, matching the solver's semantics.
    """
    adjacency = [[] for _ in graph.points]
    for u, v in graph.edges:
        adjacency[u].append(v)
        adjacency[v].append(u)
    candidates = [(u, v, w) for v, neighbours in enumerate(adjacency)
                  for u in sorted(neighbours) for w in sorted(neighbours) if u != w]
    listed_count = len(graph.edges) // 5
    illegal_count = len(graph.edges) // 10
    chosen = rng.sample(candidates, listed_count + illegal_count)
    graph.turns = sorted(chosen[:listed_count])
    graph.illegal_turns = sorted(chosen[listed_count:])
    return graph


def with_zones(graph):
    """Split interior edges evenly into two reproducible connected zones.

    Connectivity here is edge connectivity: consecutive edges in a zone may
    meet at a vertex.
    """
    required_edges = sorted(set(graph.edges) - graph.contour_edges)
    split = len(required_edges) // 2
    if split == 0:
        graph.edge_zones = {connection: 2 for connection in required_edges}
        return graph

    incident = {}
    for connection in required_edges:
        for vertex in connection:
            incident.setdefault(vertex, []).append(connection)
    adjacency = {connection: set() for connection in required_edges}
    for connections in incident.values():
        for connection in connections:
            adjacency[connection].update(connections)
            adjacency[connection].discard(connection)

    rng = random.Random(f'{graph.seed}:zones')
    priority = required_edges[:]
    rng.shuffle(priority)
    rank = {connection: index for index, connection in enumerate(priority)}
    all_edges = set(required_edges)

    # A straight geometric cut is fast and normally gives two connected
    # regions on the mesh. Try several seeded orientations before using the
    # more expensive constructive fallback below.
    offset = rng.random() * 2 * math.pi
    for step in range(32):
        angle = offset + step * math.pi / 16
        x_weight, y_weight = math.cos(angle), math.sin(angle)
        ordered = sorted(required_edges, key=lambda connection: (
            sum(graph.points[vertex][0] * x_weight
                + graph.points[vertex][1] * y_weight
                for vertex in connection),
            connection,
        ))
        zone_1 = set(ordered[:split])
        zone_2 = all_edges - zone_1
        if (connected_edges(adjacency, zone_1)
                and connected_edges(adjacency, zone_2)):
            graph.edge_zones = {
                connection: 1 if connection in zone_1 else 2
                for connection in required_edges
            }
            return graph

    # Trying different removable starting edges avoids committing to a branch
    # that cannot reach the requested size without separating the remainder.
    starts = all_edges - articulation_points(adjacency, all_edges)
    for start in sorted(starts, key=rank.get):
        zone_1 = {start}
        zone_2 = all_edges - zone_1
        while len(zone_1) < split:
            boundary = set()
            for connection in zone_1:
                boundary.update(adjacency[connection] & zone_2)
            removable = boundary - articulation_points(adjacency, zone_2)
            if not removable:
                break
            chosen = min(removable, key=rank.get)
            zone_1.add(chosen)
            zone_2.remove(chosen)
        if len(zone_1) == split:
            graph.edge_zones = {
                connection: 1 if connection in zone_1 else 2
                for connection in required_edges
            }
            return graph

    raise RuntimeError("no se pudieron particionar las zonas de forma conexa")


def connected_edges(adjacency, vertices):
    """Return whether an edge-adjacency subgraph is connected."""
    if not vertices:
        return True
    reached = {next(iter(vertices))}
    pending = list(reached)
    while pending:
        new_vertices = (adjacency[pending.pop()] & vertices) - reached
        reached.update(new_vertices)
        pending.extend(new_vertices)
    return len(reached) == len(vertices)


def articulation_points(adjacency, vertices):
    """Return articulation points of a connected induced subgraph."""
    if len(vertices) < 3:
        return set()

    discovery, low, parent, child_count = {}, {}, {}, {}
    result = set()
    clock = 0
    root = min(vertices)
    discovery[root] = low[root] = clock
    clock += 1
    child_count[root] = 0
    stack = [(root, iter(adjacency[root] & vertices))]

    while stack:
        current, neighbours = stack[-1]
        try:
            neighbour = next(neighbours)
        except StopIteration:
            stack.pop()
            if current not in parent:
                if child_count[current] > 1:
                    result.add(current)
                continue
            previous = parent[current]
            low[previous] = min(low[previous], low[current])
            if previous in parent and low[current] >= discovery[previous]:
                result.add(previous)
            continue

        if neighbour not in discovery:
            parent[neighbour] = current
            child_count[current] = child_count.get(current, 0) + 1
            child_count[neighbour] = 0
            discovery[neighbour] = low[neighbour] = clock
            clock += 1
            stack.append((neighbour, iter(adjacency[neighbour] & vertices)))
        elif parent.get(current) != neighbour:
            low[current] = min(low[current], discovery[neighbour])

    return result


def with_random_demands(graph, demand_type, minimum, maximum):
    """Assign reproducible per-edge demands without changing topology or turns."""
    if demand_type not in ('integer', 'real'):
        raise ValueError("demand_type debe ser 'integer' o 'real'")
    if (not isinstance(minimum, Real) or isinstance(minimum, bool)
            or not isinstance(maximum, Real) or isinstance(maximum, bool)
            or not math.isfinite(minimum) or not math.isfinite(maximum)
            or minimum <= 0 or maximum < minimum):
        raise ValueError("el rango de demanda debe ser positivo, finito y no decreciente")
    if demand_type == 'integer' and (int(minimum) != minimum or int(maximum) != maximum):
        raise ValueError("los limites de demanda integer deben ser enteros")

    # Keep demand sampling independent from the random draws used for topology
    # and turns, so changing only the demand mode does not change the graph.
    rng = random.Random(f'{graph.seed}:demand')
    required_edges = sorted(set(graph.edges) - graph.contour_edges)
    if demand_type == 'integer':
        graph.edge_demands = {
            connection: rng.randint(int(minimum), int(maximum))
            for connection in required_edges
        }
    else:
        graph.edge_demands = {
            connection: rng.uniform(float(minimum), float(maximum))
            for connection in required_edges
        }
    return graph


def with_random_costs(graph, minimum, maximum):
    """Assign reproducible real traversal costs independently of topology."""
    if (not isinstance(minimum, Real) or isinstance(minimum, bool)
            or not isinstance(maximum, Real) or isinstance(maximum, bool)
            or not math.isfinite(minimum) or not math.isfinite(maximum)
            or minimum <= 0 or maximum < minimum):
        raise ValueError("el rango de costo debe ser positivo, finito y no decreciente")

    rng = random.Random(f'{graph.seed}:cost')
    graph.edge_costs = {
        connection: rng.uniform(float(minimum), float(maximum))
        for connection in sorted(graph.edges)
    }
    return graph


def generate_graph(n, seed=0, vehicles=2, demand=1, demand_type='real',
                   demand_min=1, demand_max=10, cost_min=1, cost_max=10):
    """Return a mesh; IDs are zero-based until export, without a deposit node.

    For n >= 14 the mean degree is exactly 4. Smaller meshes use all available
    noncrossing edges. At least three nodes are needed for a simple contour.
    """
    if not isinstance(n, int) or isinstance(n, bool) or n < 3:
        raise ValueError("n debe ser un entero mayor o igual a 3")
    if not isinstance(vehicles, int) or isinstance(vehicles, bool) or vehicles < 2:
        raise ValueError("vehicles debe ser un entero mayor o igual a 2")
    if (not isinstance(demand, Real) or isinstance(demand, bool)
            or not math.isfinite(demand) or demand <= 0):
        raise ValueError("demand debe ser un numero entero o real, positivo y finito")
    if demand_type not in ('fixed', 'integer', 'real'):
        raise ValueError("demand_type debe ser 'fixed', 'integer' o 'real'")
    # Match the reader's signed 32-bit index limits, including virtual nodes.
    if 8 * n >= 2**31 - 1 or vehicles >= 2**31 - 1:
        raise ValueError("cantidad fuera del rango de indices del solver")
    rng = random.Random(seed)
    if n == 3:
        graph = with_random_costs(with_zones(with_turns(
            GeneratedGraph([(0., 0.), (1., 0.), (0.5, 1.)],
                           [(0, 1), (0, 2), (1, 2)], [0, 1, 2],
                           vehicles, demand, seed), rng)), cost_min, cost_max)
        return (graph if demand_type == 'fixed' else
                with_random_demands(graph, demand_type, demand_min, demand_max))

    height = math.isqrt(n)
    width, extra = divmod(n, height)
    points, rows = [], []
    for y in range(height):
        count = width + (y < extra)
        row = []
        for x in range(count):
            row.append(len(points))
            points.append((x * (width - 1) / (count - 1), float(y)))
        rows.append(row)

    contour = (rows[0] + [row[-1] for row in rows[1:]]
               + list(reversed(rows[-1][:-1]))
               + [row[0] for row in reversed(rows[1:-1])])
    selected = {edge(u, v) for row in rows for u, v in zip(row, row[1:])}
    candidates = set()
    for upper, lower in zip(rows, rows[1:]):
        # Triangulate each strip by merging its two ordered rows. There can be
        # no crossing edges. Integer comparisons avoid rounding aligned nodes.
        i = j = 0
        while True:
            connection = edge(upper[i], lower[j])
            candidates.add(connection)
            if i * (len(lower) - 1) == j * (len(upper) - 1):
                selected.add(connection)
            if i == len(upper) - 1 and j == len(lower) - 1:
                break
            if i == len(upper) - 1:
                j += 1
            elif j == len(lower) - 1:
                i += 1
            else:
                a = (i + 1) * (len(lower) - 1)
                b = (j + 1) * (len(upper) - 1)
                if a < b or (a == b and rng.randrange(2) == 0):
                    i += 1
                else:
                    j += 1

    # Horizontal paths plus the left and right sides already connect all nodes
    # and give each node degree >= 2. Extra edges never alter the contour.
    remaining = sorted(candidates - selected)
    rng.shuffle(remaining)
    target = min(2 * n, len(selected) + len(remaining))
    selected.update(remaining[:target - len(selected)])
    graph = with_random_costs(
        with_zones(with_turns(GeneratedGraph(points, sorted(selected), contour,
                                            vehicles, demand, seed), rng)),
        cost_min, cost_max)
    return (graph if demand_type == 'fixed' else
            with_random_demands(graph, demand_type, demand_min, demand_max))


def write_graph(graph, svg=False):
    """Write solver files in ./input, with an SVG preview only when requested."""
    output = Path.cwd() / 'input' / f'graph_{len(graph.points)}.dat'
    turns = output.with_name(output.stem + '.turns.dat')
    preview = output.with_name(output.stem + '.svg')
    paths = (output, turns, preview) if svg else (output, turns)
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = [f'{graph.vehicles} {len(graph.points)} 1 {len(graph.edges)} 0',
             str(graph.contour[0] + 1)]
    for u, v in graph.edges:
        cost = graph.cost_for(u, v)
        zone = graph.zone_for(u, v)
        demand = graph.demand_for(u, v)
        lines.append(f'{u + 1} {v + 1} {zone} '
                     f'{cost:.17g} {demand:.17g}')
    output.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    turn_lines = [f'{len(graph.turns)} {len(graph.illegal_turns)}']
    turn_lines.extend(f'{u + 1} {v + 1} {w + 1}'
                      for u, v, w in graph.turns + graph.illegal_turns)
    turns.write_text('\n'.join(turn_lines) + '\n', encoding='utf-8')
    if svg:
        write_svg(graph, preview)
    return paths


def write_svg(graph, output):
    scale = 720 / max(max(x for x, _ in graph.points), max(y for _, y in graph.points))
    points = [(40 + x * scale, 40 + y * scale) for x, y in graph.points]
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 840">',
           '<rect width="800" height="840" fill="white"/>',
           '<text x="40" y="805" font-family="sans-serif" font-size="16">'
           'Gris: contorno opcional (zona 0). Rojo: zona 1. Verde: zona 2.</text>',
           '<text x="40" y="828" font-family="sans-serif" font-size="16">'
           'Azul: nodo adyacente al deposito.</text>']
    for u, v in graph.edges:
        x1, y1 = points[u]
        x2, y2 = points[v]
        zone = graph.zone_for(u, v)
        color = {0: '#9ca3af', 1: '#dc2626', 2: '#16a34a'}[zone]
        width = 1 if zone == 0 else 3
        svg.append(f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" '
                   f'stroke="{color}" stroke-width="{width}"/>')
    for u, (x, y) in enumerate(points):
        color = '#2563eb' if u == graph.contour[0] else '#111827'
        svg.append(f'<circle cx="{x}" cy="{y}" r="3" fill="{color}">'
                   f'<title>Nodo {u + 1}</title></circle>')
    svg.append('</svg>')
    Path(output).write_text('\n'.join(svg) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    size = parser.add_mutually_exclusive_group(required=True)
    size.add_argument('n', nargs='?', type=int, help='cantidad exacta de nodos (>= 3)')
    size.add_argument('--nodes', '-n', type=int, help='alternativa al parametro posicional n')
    parser.add_argument('--seed', type=int, default=0)
    parser.add_argument('--vehicles', type=int, default=2,
                        help='cantidad de vehiculos, al menos 2 (default: 2)')
    parser.add_argument('--demand', type=float,
                        help='demanda fija positiva por arista requerida de zonas 1 y 2')
    parser.add_argument('--demand-type', choices=('fixed', 'integer', 'real'),
                        help='demanda fija o aleatoria entera/real (default: real)')
    parser.add_argument('--demand-min', type=float, default=1,
                        help='minimo para demanda aleatoria (default: 1)')
    parser.add_argument('--demand-max', type=float, default=10,
                        help='maximo para demanda aleatoria (default: 10)')
    parser.add_argument('--cost-min', type=float, default=1,
                        help='minimo para costo real aleatorio (default: 1)')
    parser.add_argument('--cost-max', type=float, default=10,
                        help='maximo para costo real aleatorio (default: 10)')
    parser.add_argument('--svg', action='store_true', help='generar también input/graph_n.svg')
    args = parser.parse_args()
    try:
        demand_type = args.demand_type or ('fixed' if args.demand is not None else 'real')
        if args.demand is not None and demand_type != 'fixed':
            raise ValueError("--demand solo se puede usar con --demand-type fixed")
        graph = generate_graph(args.n if args.n is not None else args.nodes,
                               args.seed, args.vehicles,
                               1 if args.demand is None else args.demand,
                               demand_type, args.demand_min, args.demand_max,
                               args.cost_min, args.cost_max)
        paths = write_graph(graph, svg=args.svg)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    print(f'Nodos: {len(graph.points)}; aristas: {len(graph.edges)}; '
          f'grado promedio: {graph.average_degree:.6g}; contorno: {len(graph.contour)}')
    print(f'Giros: {len(graph.turns)}; giros ilegales: {len(graph.illegal_turns)} '
          '(20% y 10% de las aristas, redondeados hacia abajo)')
    if demand_type != 'fixed' and graph.edge_demands:
        values = list(graph.edge_demands.values())
        print(f'Demanda aleatoria {demand_type} por arista requerida: '
              f'min={min(values):.17g}; max={max(values):.17g}')
    elif demand_type != 'fixed':
        print(f'Demanda aleatoria {demand_type}: no hay aristas requeridas')
    else:
        print(f'Demanda por arista requerida: {graph.demand:.17g}')
    costs = list(graph.edge_costs.values())
    print(f'Costo real aleatorio por arista: min={min(costs):.17g}; '
          f'max={max(costs):.17g}')
    if graph.average_degree < 4:
        print('Para este tamaño, la malla plana alcanza el grado indicado; '
              'el promedio es exactamente 4 a partir de n=14.')
    for path in paths:
        print(path)


if __name__ == '__main__':
    main()
