#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Uso: $0 <cantidad_de_nodos>" >&2
    exit 1
fi

nodes="$1"
graph="input/graph_${nodes}.dat"
turns="input/graph_${nodes}.turns.dat"
solution="output/dist/out_${nodes}.dat"
clusters="data/clusters/clusters_${nodes}.dat"
segments="output/order/route_segments_${nodes}.csv"
coordinates="data/coords/graph_${nodes}.coords.csv"
video="output/videos/route_${nodes}.gif"

make -s path-clusters
./pathSortClusterExec "$graph" "$turns" "$solution" "$clusters"
python3 tools/generate_route_video.py \
    --segments "$segments" \
    --coords "$coordinates" \
    --output "$video"
