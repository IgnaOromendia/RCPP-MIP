"""Generator invariants, exports and optional native reader/solver integration."""

import argparse
from contextlib import contextmanager
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
from generate_graph import generate_graph, write_graph

READER = SOLVER = None


@contextmanager
def working_directory(directory):
    previous = Path.cwd()
    os.chdir(directory)
    try:
        yield
    finally:
        os.chdir(previous)


def reachable(adjacency, start):
    seen, todo = {start}, [start]
    while todo:
        for v in adjacency[todo.pop()]:
            if v not in seen:
                seen.add(v)
                todo.append(v)
    return seen


class GeneratorTest(unittest.TestCase):
    def test_sizes_connectivity_degree_and_contour(self):
        for n in [*range(3, 130), 257, 1000]:
            for seed in (0, 19):
                with self.subTest(n=n, seed=seed):
                    graph = generate_graph(n, seed)
                    self.assertEqual(len(graph.points), n)
                    self.assertEqual(len(graph.edges), len(set(graph.edges)))
                    adj = [set() for _ in range(n)]
                    for u, v in graph.edges:
                        self.assertTrue(0 <= u < v < n)
                        self.assertGreater(math.dist(graph.points[u], graph.points[v]), 0)
                        adj[u].add(v)
                        adj[v].add(u)
                    self.assertEqual(len(reachable(adj, 0)), n)
                    self.assertGreaterEqual(min(map(len, adj)), 2)
                    self.assertEqual(len(graph.contour), len(set(graph.contour)))
                    boundary = graph.contour_edges
                    self.assertTrue(boundary <= set(graph.edges))
                    perimeter_adj = {u: set() for u in graph.contour}
                    for u, v in boundary:
                        perimeter_adj[u].add(v)
                        perimeter_adj[v].add(u)
                    self.assertTrue(all(len(v) == 2 for v in perimeter_adj.values()))
                    self.assertEqual(len(reachable(perimeter_adj, graph.contour[0])),
                                     len(graph.contour))
                    if n >= 14:
                        self.assertEqual(graph.average_degree, 4)
                    else:
                        self.assertLess(graph.average_degree, 4)
                        # A triangulated disk with h boundary vertices has 3n-3-h edges.
                        self.assertEqual(len(graph.edges), 3 * n - 3 - len(graph.contour))

    def test_geometric_boundary_and_no_crossings(self):
        def cross(a, b, c):
            return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])

        for n in range(3, 45):
            graph = generate_graph(n, seed=42)
            perimeter = [graph.points[u] for u in graph.contour]
            for a, b in zip(perimeter, perimeter[1:] + perimeter[:1]):
                self.assertTrue(all(cross(a, b, p) >= -1e-9 for p in graph.points))
            for i, (u, v) in enumerate(graph.edges):
                a, b = graph.points[u], graph.points[v]
                for w, z in graph.edges[i + 1:]:
                    if len({u, v, w, z}) < 4:
                        continue
                    c, d = graph.points[w], graph.points[z]
                    intersects = (cross(a, b, c) * cross(a, b, d) < -1e-9
                                  and cross(c, d, a) * cross(c, d, b) < -1e-9)
                    self.assertFalse(intersects, (n, u, v, w, z))

    def test_reproducibility(self):
        self.assertEqual(generate_graph(101, 7), generate_graph(101, 7))
        self.assertNotEqual(generate_graph(101, 7).edges, generate_graph(101, 8).edges)
        self.assertNotEqual(generate_graph(101, 7).illegal_turns,
                            generate_graph(101, 8).illegal_turns)

    def test_contour_is_optional_and_interior_is_split_between_two_zones(self):
        for n in [3, 4, 17, 100, 101]:
            for seed in (0, 7, 42):
                with self.subTest(n=n, seed=seed):
                    graph = generate_graph(n, seed)
                    boundary = graph.contour_edges
                    interior = set(graph.edges) - boundary
                    self.assertTrue(all(graph.zone_for(*connection) == 0
                                        for connection in boundary))
                    zone_1 = {connection for connection in interior
                              if graph.zone_for(*connection) == 1}
                    zone_2 = {connection for connection in interior
                              if graph.zone_for(*connection) == 2}
                    self.assertEqual(zone_1 | zone_2, interior)
                    self.assertFalse(zone_1 & zone_2)
                    self.assertLessEqual(abs(len(zone_1) - len(zone_2)), 1)

    def test_turn_counts_and_valid_triples(self):
        for n, counts in ((3, (0, 0)), (4, (1, 0)), (17, (6, 3)), (100, (40, 20))):
            for seed in (0, 7, 42):
                with self.subTest(n=n, seed=seed):
                    graph = generate_graph(n, seed)
                    self.assertEqual((len(graph.turns), len(graph.illegal_turns)), counts)
                    triples = graph.turns + graph.illegal_turns
                    self.assertEqual(len(set(triples)), len(triples))
                    edges = set(graph.edges)
                    for u, v, w in triples:
                        self.assertEqual(len({u, v, w}), 3)
                        self.assertIn(tuple(sorted((u, v))), edges)
                        self.assertIn(tuple(sorted((v, w))), edges)

    def test_invalid_arguments(self):
        for n in (-1, 0, 1, 2, 3.5, True, 2**30):
            with self.assertRaises(ValueError):
                generate_graph(n)
        for vehicles in (0, -1, 1, 1.5, 2**31):
            with self.assertRaises(ValueError):
                generate_graph(16, vehicles=vehicles)
        for demand in (0, -1, float('nan'), float('inf'), True, '2.5', None):
            with self.assertRaises(ValueError):
                generate_graph(16, demand=demand)

    def test_integer_and_real_demand(self):
        for demand, exported in ((3, '3'), (2.5, '2.5')):
            with self.subTest(demand=demand):
                graph = generate_graph(17, demand=demand)
                self.assertEqual(graph.demand, demand)
                with tempfile.TemporaryDirectory() as directory, working_directory(directory):
                    output, _ = write_graph(graph)
                    interior_demands = [line.split()[4] for line in output.read_text().splitlines()[2:]
                                        if line.split()[2] in ('1', '2')]
                    self.assertTrue(interior_demands)
                    self.assertEqual(set(interior_demands), {exported})

    def test_random_integer_and_real_demands(self):
        fixed = generate_graph(101, seed=7)
        integer = generate_graph(101, seed=7, demand_type='integer',
                                 demand_min=2, demand_max=8)
        real = generate_graph(101, seed=7, demand_type='real',
                              demand_min=0.25, demand_max=2.75)
        self.assertEqual(integer.edges, fixed.edges)
        self.assertEqual(integer.turns, fixed.turns)
        self.assertEqual(integer.illegal_turns, fixed.illegal_turns)
        self.assertEqual(real.edges, fixed.edges)
        self.assertEqual(real.turns, fixed.turns)
        self.assertEqual(real.illegal_turns, fixed.illegal_turns)
        self.assertEqual(integer, generate_graph(101, seed=7, demand_type='integer',
                                                 demand_min=2, demand_max=8))
        self.assertEqual(real, generate_graph(101, seed=7, demand_type='real',
                                              demand_min=0.25, demand_max=2.75))
        self.assertTrue(integer.edge_demands)
        self.assertTrue(real.edge_demands)
        self.assertTrue(all(isinstance(value, int) and 2 <= value <= 8
                            for value in integer.edge_demands.values()))
        self.assertTrue(all(isinstance(value, float) and 0.25 <= value <= 2.75
                            for value in real.edge_demands.values()))
        self.assertGreater(len(set(integer.edge_demands.values())), 1)
        self.assertGreater(len(set(real.edge_demands.values())), 1)
        self.assertEqual(set(integer.edge_demands),
                         set(integer.edges) - integer.contour_edges)
        self.assertEqual(set(real.edge_demands), set(real.edges) - real.contour_edges)

        for graph in (integer, real):
            with tempfile.TemporaryDirectory() as directory, working_directory(directory):
                output, _ = write_graph(graph)
                for line, connection in zip(output.read_text().splitlines()[2:], graph.edges):
                    exported = float(line.split()[4])
                    self.assertEqual(exported, graph.demand_for(*connection))

    def test_invalid_random_demand_arguments(self):
        invalid = (
            {'demand_type': 'unknown'},
            {'demand_type': 'integer', 'demand_min': 1.5, 'demand_max': 5},
            {'demand_type': 'integer', 'demand_min': 5, 'demand_max': 2},
            {'demand_type': 'real', 'demand_min': 0, 'demand_max': 2},
            {'demand_type': 'real', 'demand_min': 1, 'demand_max': float('inf')},
        )
        for arguments in invalid:
            with self.subTest(arguments=arguments), self.assertRaises(ValueError):
                generate_graph(16, **arguments)

    def test_export_and_native_reader(self):
        with tempfile.TemporaryDirectory() as directory, working_directory(directory):
            graph = generate_graph(17, vehicles=2, demand=0.25)
            output, turns = write_graph(graph)
            self.assertEqual(output.parent, Path.cwd() / 'input')
            self.assertEqual({p.name for p in output.parent.iterdir()},
                             {'graph_17.dat', 'graph_17.turns.dat'})
            lines = output.read_text().splitlines()
            self.assertEqual(lines[0], '2 17 1 34 0')
            self.assertEqual(int(lines[1]), graph.contour[0] + 1)
            self.assertEqual(len(lines), 36)
            for line, (u, v) in zip(lines[2:], graph.edges):
                source, target, zone, cost, demand = line.split()
                self.assertEqual((int(source), int(target)), (u + 1, v + 1))
                self.assertEqual(int(zone), graph.zone_for(u, v))
                self.assertEqual(float(demand), graph.demand_for(u, v))
                self.assertAlmostEqual(float(cost), math.dist(graph.points[u], graph.points[v]))
            turn_lines = turns.read_text().splitlines()
            self.assertEqual(turn_lines[0], '6 3')
            self.assertEqual(len(turn_lines), 10)
            triples = [tuple(int(node) - 1 for node in line.split()) for line in turn_lines[1:]]
            self.assertEqual(triples[:6], graph.turns)
            self.assertEqual(triples[6:], graph.illegal_turns)
            if READER:
                result = subprocess.run([READER, output, turns], capture_output=True,
                                        text=True, timeout=30, check=True)
                required = len(set(graph.edges) - graph.contour_edges)
                self.assertEqual(result.stdout.strip(), f'17 34 {required} 6 3')

    def test_cli(self):
        with tempfile.TemporaryDirectory() as directory:
            for size in (['19'], ['--nodes', '19']):
                result = subprocess.run([sys.executable, ROOT / 'tools/generate_graph.py',
                                         *size, '--seed', '9'],
                                        cwd=directory, capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('Nodos: 19; aristas: 38; grado promedio: 4', result.stdout)
                self.assertEqual({p.name for p in (Path(directory) / 'input').iterdir()},
                                 {'graph_19.dat', 'graph_19.turns.dat'})
                self.assertNotIn('.svg', result.stdout)
            self.assertEqual({p.name for p in Path(directory).iterdir()}, {'input'})
        for demand, exported in (('3', '3'), ('2.5', '2.5')):
            with self.subTest(demand=demand), tempfile.TemporaryDirectory() as directory:
                result = subprocess.run([sys.executable, ROOT / 'tools/generate_graph.py',
                                         '19', '--demand', demand], cwd=directory,
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(f'Demanda por arista requerida: {exported}', result.stdout)
                lines = (Path(directory) / 'input/graph_19.dat').read_text().splitlines()[2:]
                interior_demands = [line.split()[4] for line in lines
                                    if line.split()[2] in ('1', '2')]
                self.assertEqual(set(interior_demands), {exported})
        for demand_type, minimum, maximum in (('integer', '2', '8'),
                                               ('real', '0.25', '2.75')):
            with self.subTest(demand_type=demand_type), \
                    tempfile.TemporaryDirectory() as directory:
                result = subprocess.run([
                    sys.executable, ROOT / 'tools/generate_graph.py', '19', '--seed', '7',
                    '--demand-type', demand_type, '--demand-min', minimum,
                    '--demand-max', maximum], cwd=directory, capture_output=True,
                    text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn(f'Demanda aleatoria {demand_type}', result.stdout)
                lines = (Path(directory) / 'input/graph_19.dat').read_text().splitlines()[2:]
                values = [line.split()[4] for line in lines
                          if line.split()[2] in ('1', '2')]
                self.assertGreater(len(set(values)), 1)
                if demand_type == 'integer':
                    self.assertTrue(all(value.isdigit() for value in values))
                else:
                    self.assertTrue(all('.' in value for value in values))
        for arguments in (['2'], ['19', '--output', 'sample.dat']):
            with tempfile.TemporaryDirectory() as directory:
                result = subprocess.run([sys.executable, ROOT / 'tools/generate_graph.py',
                                         *arguments], cwd=directory, capture_output=True,
                                        text=True, timeout=30)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(list(Path(directory).iterdir()), [])

    def test_svg_flag(self):
        with tempfile.TemporaryDirectory() as directory:
            command = [sys.executable, ROOT / 'tools/generate_graph.py', '19']
            result = subprocess.run([*command, '--svg'], cwd=directory, capture_output=True,
                                    text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            output = Path(directory) / 'input'
            self.assertEqual({p.name for p in output.iterdir()},
                             {'graph_19.dat', 'graph_19.turns.dat', 'graph_19.svg'})
            self.assertEqual(ET.parse(output / 'graph_19.svg').getroot().tag,
                             '{http://www.w3.org/2000/svg}svg')
            self.assertIn('graph_19.svg', result.stdout)
            # Omitting --svg must neither overwrite nor remove an existing preview.
            (output / 'graph_19.svg').write_text('previous preview')
            result = subprocess.run(command, cwd=directory, capture_output=True,
                                    text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((output / 'graph_19.svg').read_text(), 'previous preview')
            self.assertNotIn('.svg', result.stdout)

    def test_different_sizes_keep_separate_files(self):
        with tempfile.TemporaryDirectory() as directory, working_directory(directory):
            paths = write_graph(generate_graph(17), svg=True)
            previous = {p: p.read_bytes() for p in paths}
            write_graph(generate_graph(19), svg=True)
            self.assertEqual({p.name for p in Path('input').iterdir()},
                             {f'graph_{n}{suffix}' for n in (17, 19)
                              for suffix in ('.dat', '.turns.dat', '.svg')})
            for path, data in previous.items():
                self.assertEqual(path.read_bytes(), data)

    def test_solver(self):
        if SOLVER is None:
            self.skipTest('CPLEX integration not requested')
        for n in (4, 9):
            with tempfile.TemporaryDirectory() as directory, working_directory(directory):
                graph = generate_graph(n)
                output, turns = write_graph(graph)
                result = subprocess.run([SOLVER, output, turns, 'mip'], cwd=directory,
                                        capture_output=True, text=True, timeout=30)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertTrue((Path(directory) / 'out.dat').read_text().startswith('OBJ: '))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--reader', type=Path)
    parser.add_argument('--solver', type=Path)
    options, rest = parser.parse_known_args()
    READER = options.reader.resolve() if options.reader else None
    SOLVER = options.solver.resolve() if options.solver else None
    unittest.main(argv=[sys.argv[0], *rest])
