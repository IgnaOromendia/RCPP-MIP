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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--solver', type=Path, default=ROOT / 'bestClusterPathExec')
    parser.add_argument('--sorter', type=Path, default=ROOT / 'pathSortClusterExec')
    options = parser.parse_args()
    # The same triangle exercises undirected, mixed and directed virtual IDs.
    for undirected in (0, 2, 3):
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
            neutral = directory / 'neutral.dat'
            neutral.write_text('node_id reasons\n1 boundary\n2 boundary\n3 boundary\n')
            contracted = directory / 'solution.dat'
            virtual = directory / 'solution.supergraph.dat'
            run([options.solver.resolve(), graph, turns, '--clusters', clusters,
                 '--neutral-nodes', neutral, '--output', contracted,
                 '--sorter-output', virtual], directory)
            if not contracted.read_text().startswith('OBJ: 9\n\n---- X ----\nX_1 = 1\n'):
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
            if {row['cluster'] for row in rows} != {'1', '2', '3'}:
                raise RuntimeError(f'The projection lost cluster assignments: {rows}')


if __name__ == '__main__':
    main()
