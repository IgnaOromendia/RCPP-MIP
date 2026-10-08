#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Uso: $0 <cantidad_nodos> [mip|fixAndOptimize [maxDeadheadCost|random|topKDeadheadCost]]" >&2
    exit 1
fi

nodes=$1
shift

make -s mip
python3 tools/generate_graph.py "$nodes" --free --vehicles 1 --svg
./solverExec "input/graph_${nodes}.dat" "input/graph_${nodes}.turns.dat" "$@"
python3 clusterGeneration/generate_clusters.py "output/dist/out_${nodes}.dat" \
    --graph "input/graph_${nodes}.dat" \
    --coords "data/coords/graph_${nodes}.coords.csv"
