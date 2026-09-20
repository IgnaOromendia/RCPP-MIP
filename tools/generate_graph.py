#!/usr/bin/env python3
"""Generate a connected planar RCPP instance with exactly n original nodes.

Uses only the Python standard library. Zones belong to edges, not vertices.
The outer cycle is required (-1); every interior edge is optional (0).
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
    edge_demands: dict = field(default_factory=dict)

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
    optional_edges = sorted(set(graph.edges) - graph.contour_edges)
    if demand_type == 'integer':
        graph.edge_demands = {
            connection: rng.randint(int(minimum), int(maximum))
            for connection in optional_edges
        }
    else:
        graph.edge_demands = {
            connection: rng.uniform(float(minimum), float(maximum))
            for connection in optional_edges
        }
    return graph


def generate_graph(n, seed=0, vehicles=1, demand=1, demand_type='fixed',
                   demand_min=1, demand_max=10):
    """Return a mesh; IDs are zero-based until export, without a deposit node.

    For n >= 14 the mean degree is exactly 4. Smaller meshes use all available
    noncrossing edges. At least three nodes are needed for a simple contour.
    """
    if not isinstance(n, int) or isinstance(n, bool) or n < 3:
        raise ValueError("n debe ser un entero mayor o igual a 3")
    if not isinstance(vehicles, int) or isinstance(vehicles, bool) or vehicles < 1:
        raise ValueError("vehicles debe ser un entero positivo")
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
        graph = with_turns(GeneratedGraph([(0., 0.), (1., 0.), (0.5, 1.)],
                           [(0, 1), (0, 2), (1, 2)], [0, 1, 2],
                           vehicles, demand, seed), rng)
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
    graph = with_turns(GeneratedGraph(points, sorted(selected), contour,
                                     vehicles, demand, seed), rng)
    return (graph if demand_type == 'fixed' else
            with_random_demands(graph, demand_type, demand_min, demand_max))


def write_graph(graph, svg=False):
    """Write solver files in ./input, with an SVG preview only when requested."""
    output = Path.cwd() / 'input' / f'graph_{len(graph.points)}.dat'
    turns = output.with_name(output.stem + '.turns.dat')
    preview = output.with_name(output.stem + '.svg')
    paths = (output, turns, preview) if svg else (output, turns)
    output.parent.mkdir(parents=True, exist_ok=True)
    boundary = graph.contour_edges
    lines = [f'{graph.vehicles} {len(graph.points)} 1 {len(graph.edges)} 0',
             str(graph.contour[0] + 1)]
    for u, v in graph.edges:
        required = (u, v) in boundary
        cost = math.dist(graph.points[u], graph.points[v])
        demand = 0 if required else graph.edge_demands.get((u, v), graph.demand)
        lines.append(f'{u + 1} {v + 1} {-1 if required else 0} '
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
    boundary = graph.contour_edges
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 840">',
           '<rect width="800" height="840" fill="white"/>',
           '<text x="40" y="805" font-family="sans-serif" font-size="16">'
           'Rojo: contorno (zona -1). Gris: interior (zona 0).</text>',
           '<text x="40" y="828" font-family="sans-serif" font-size="16">'
           'Azul: nodo adyacente al deposito.</text>']
    for u, v in graph.edges:
        x1, y1 = points[u]
        x2, y2 = points[v]
        color, width = ('#dc2626', 3) if (u, v) in boundary else ('#9ca3af', 1)
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
    parser.add_argument('--vehicles', type=int, default=1)
    parser.add_argument('--demand', type=float,
                        help='demanda entera o real positiva por arista de zona 0 (default: 1)')
    parser.add_argument('--demand-type', choices=('fixed', 'integer', 'real'), default='fixed',
                        help='demanda fija o aleatoria entera/real (default: fixed)')
    parser.add_argument('--demand-min', type=float, default=1,
                        help='minimo para demanda aleatoria (default: 1)')
    parser.add_argument('--demand-max', type=float, default=10,
                        help='maximo para demanda aleatoria (default: 10)')
    parser.add_argument('--svg', action='store_true', help='generar también input/graph_n.svg')
    args = parser.parse_args()
    try:
        if args.demand is not None and args.demand_type != 'fixed':
            raise ValueError("--demand solo se puede usar con --demand-type fixed")
        graph = generate_graph(args.n if args.n is not None else args.nodes,
                               args.seed, args.vehicles,
                               1 if args.demand is None else args.demand,
                               args.demand_type, args.demand_min, args.demand_max)
        paths = write_graph(graph, svg=args.svg)
    except (ValueError, OSError) as error:
        parser.error(str(error))
    print(f'Nodos: {len(graph.points)}; aristas: {len(graph.edges)}; '
          f'grado promedio: {graph.average_degree:.6g}; contorno: {len(graph.contour)}')
    print(f'Giros: {len(graph.turns)}; giros ilegales: {len(graph.illegal_turns)} '
          '(20% y 10% de las aristas, redondeados hacia abajo)')
    if args.demand_type != 'fixed' and graph.edge_demands:
        values = list(graph.edge_demands.values())
        print(f'Demanda aleatoria {args.demand_type} por arista de zona 0: '
              f'min={min(values):.17g}; max={max(values):.17g}')
    elif args.demand_type != 'fixed':
        print(f'Demanda aleatoria {args.demand_type}: no hay aristas de zona 0')
    else:
        print(f'Demanda por arista de zona 0: {graph.demand:.17g}')
    if graph.average_degree < 4:
        print('Para este tamaño, la malla plana alcanza el grado indicado; '
              'el promedio es exactamente 4 a partir de n=14.')
    for path in paths:
        print(path)


if __name__ == '__main__':
    main()
