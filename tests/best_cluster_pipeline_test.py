#!/usr/bin/env python3
"""Exercise the real cluster solver, projection and sorter across format boundaries."""

import argparse
import csv
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent


def run(command, directory):
    result = subprocess.run(command, cwd=directory, capture_output=True,
                            text=True, timeout=30)
    if result.returncode:
        raise RuntimeError(f"{command}\n{result.stdout}\n{result.stderr}")


def check_unassigned_service(solver):
    # C=0 is feasible only with the zero-demand required cycle disconnected:
    # the depot is in a separate, optional cluster. Repair must discard its
    # support, then reject the service because cluster 2 has no active visit.
    with tempfile.TemporaryDirectory(prefix='cluster-unassigned-') as raw:
        directory = Path(raw)
        graph = directory / 'graph.dat'
        graph.write_text('1 6 1 0 6\n1\n'
                         '1 2 0 1 0\n2 3 0 1 0\n3 1 0 1 0\n'
                         '4 5 1 1 0\n5 6 1 1 0\n6 4 1 1 0\n')
        turns = directory / 'turns.dat'
        turns.write_text('0 0\n')
        clusters = directory / 'clusters.dat'
        clusters.write_text('arista cluster origen destino vehiculo servicio recorridos pasadas\n'
                            '1 1 1 2 0 0 0 0\n2 1 2 3 0 0 0 0\n3 1 3 1 0 0 0 0\n'
                            '4 2 4 5 0 0 0 0\n5 2 5 6 0 0 0 0\n6 2 6 4 0 0 0 0\n')
        output, virtual = directory / 'solution.dat', directory / 'solution.supergraph.dat'
        command = [solver.resolve(), graph, turns, '--clusters', clusters,
                   '--output', output, '--sorter-output', virtual]
        result = subprocess.run(command, cwd=directory, capture_output=True,
                                text=True, timeout=30)
        if result.returncode != 1 or 'visita compatible en el cluster 2' not in result.stderr:
            raise RuntimeError(f'Unassigned service must fail with code 1: {result}')
        if output.exists() or virtual.exists():
            raise RuntimeError('Failed repair must not export a solution')
        # Existing outputs must also survive a failed run untouched.
        for path in (output, virtual):
            path.write_text('previous solution\n')
        result = subprocess.run(command, cwd=directory, capture_output=True,
                                text=True, timeout=30)
        if result.returncode != 1 or any(path.read_text() != 'previous solution\n'
                                         for path in (output, virtual)):
            raise RuntimeError('Failed repair overwrote existing outputs')


def check_shared_street(solver, sorter):
    # A directed circuit forces the shared chain to be traversed once.
    # The change from 1 to 2 can happen at either end or in the middle.
    with tempfile.TemporaryDirectory(prefix='shared-street-') as raw:
        directory = Path(raw)
        graph = directory / 'graph.dat'
        graph.write_text('1 4 1 0 4\n1\n'
                         '1 2 1 2 1\n2 3 1 3 1\n3 4 0 5 0\n4 1 1 7 1\n')
        turns = directory / 'turns.dat'
        turns.write_text('0 0\n')
        clusters = directory / 'graph-clusters.dat'
        clusters.write_text('# porcentaje_objetivo 100\n'
                            '# frontera 2 1 2\n# frontera 3 1 2\n'
                            'arista cluster origen destino vehiculo servicio recorridos pasadas\n'
                            '1 1 1 2 0 0 0 0\n2 -1 2 3 0 0 0 0\n'
                            '3 -1 3 4 0 0 0 0\n4 2 4 1 0 0 0 0\n')
        route = directory / 'route.csv'
        virtual = directory / 'solution.supergraph.dat'
        contracted = directory / 'solution.dat'
        run([solver, graph, turns, '--clusters', clusters,
             '--output', contracted, '--route-output', route, '--sorter-output', virtual],
            directory)
        rows = list(csv.DictReader(route.read_text().splitlines()))
        assert [int(row['arista']) for row in rows] == [1, 2, 3, 4]
        assert [int(row['cluster']) for row in rows] == [1, -1, -1, 2]
        assert [int(row['servicio']) for row in rows] == [1, 1, 0, 1]
        sides = [row['cluster_efectivo'] for row in rows]
        assert sum(a != b for a, b in zip(sides, sides[1:])) == 1
        assert contracted.read_text().startswith('OBJ: 17\n')

        projected = directory / 'clusters.dat'
        run([sys.executable, ROOT / 'clusterGeneration/generate_clusters.py',
             virtual, '--graph-clusters', clusters, '--graph', graph,
             '--nodes', '4', '--output', projected], directory)
        ordered = directory / 'ordered.csv'
        run([sorter, graph, turns, virtual, projected, directory / 'order.dat', ordered],
            directory)
        rows = list(csv.DictReader(ordered.read_text().splitlines()))
        assert [int(row['cluster']) for row in rows] == [1, -1, -1, 2]
        assert [int(row['arista']) for row in rows] == [1, 2, 3, 4]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--solver', type=Path, default=ROOT / 'bestClusterPathExec')
    parser.add_argument('--sorter', type=Path, default=ROOT / 'pathSortClusterExec')
    options = parser.parse_args()
    check_unassigned_service(options.solver)
    check_shared_street(options.solver.resolve(), options.sorter.resolve())
    # The same triangle exercises undirected, mixed and directed virtual IDs.
    for undirected, boundary_copies in ((0, False), (2, False), (3, False), (0, True)):
        with tempfile.TemporaryDirectory(prefix='cluster-pipeline-') as raw:
            directory = Path(raw)
            graph = directory / 'graph.dat'
            graph.write_text(f'1 3 1 {undirected} {3 - undirected}\n1\n'
                             '1 2 1 2 1\n2 3 1 3 1\n3 1 1 4 1\n')
            turns = directory / 'turns.dat'
            turns.write_text('0 0\n')
            clusters = directory / 'graph-clusters.dat'
            clusters.write_text(
                '# porcentaje_objetivo 34\n'
                'arista cluster origen destino vehiculo servicio recorridos pasadas\n'
                '1 1 1 2 0 0 0 0\n2 2 2 3 0 0 0 0\n3 3 3 1 0 0 0 0\n')
            if boundary_copies:
                # All three streets have copies in cluster 2. The optional
                # optional undirected triangle supplies their representations.
                graph.write_text('1 3 1 3 3\n1\n'
                                 '2 1 0 8 0\n3 2 0 8 0\n1 3 0 8 0\n'
                                 '1 2 1 2 1\n2 3 1 3 1\n3 1 1 4 1\n')
                clusters.write_text(
                    '# porcentaje_objetivo 50\n'
                    'arista cluster origen destino vehiculo servicio recorridos pasadas\n'
                    '1 2 2 1 0 0 0 0\n2 2 3 2 0 0 0 0\n3 2 1 3 0 0 0 0\n'
                    '4 1 1 2 0 0 0 0\n5 1 2 3 0 0 0 0\n6 1 3 1 0 0 0 0\n')
            neutral = directory / 'neutral.dat'
            neutral.write_text('node_id reasons\n1 boundary\n2 boundary\n3 boundary\n')
            contracted = directory / 'solution.dat'
            virtual = directory / 'solution.supergraph.dat'
            run([options.solver.resolve(), graph, turns, '--clusters', clusters,
                 '--neutral-nodes', neutral, '--output', contracted,
                 '--sorter-output', virtual], directory)
            first_service = 4 if boundary_copies else 1
            if not contracted.read_text().startswith(
                    f'OBJ: 9\n\n---- X ----\nX_{first_service} = 1\n'):
                raise RuntimeError('The contracted output changed or lost physical services')
            projected = directory / 'clusters.dat'
            run([sys.executable, ROOT / 'clusterGeneration/generate_clusters.py',
                 virtual, '--graph-clusters', clusters, '--graph', graph,
                 '--nodes', '3', '--output', projected], directory)
            segments = directory / 'segments.csv'
            run([options.sorter.resolve(), graph, turns, virtual, projected,
                 directory / 'order.dat', segments], directory)
            with segments.open() as source:
                rows = list(csv.DictReader(source))
            physical = [(int(row['nodo_origen']), int(row['nodo_destino'])) for row in rows]
            if len(physical) != 3 or {tuple(sorted(edge)) for edge in physical} != {
                    (1, 2), (2, 3), (1, 3)}:
                raise RuntimeError(f'The sorter lost or duplicated physical passages: {rows}')
            if any(edge[1] != physical[(index + 1) % 3][0]
                   for index, edge in enumerate(physical)):
                raise RuntimeError(f'The exported route is not continuous: {rows}')
            expected_clusters = {'1'} if boundary_copies else {'1', '2', '3'}
            if {row['cluster'] for row in rows} != expected_clusters:
                raise RuntimeError(f'The projection lost cluster assignments: {rows}')


if __name__ == '__main__':
    main()
