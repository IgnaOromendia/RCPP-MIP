#!/usr/bin/env bash

set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "Uso: $0 <cantidad_de_nodos>" >&2
    exit 1
fi

nodes="$1"
output_directory="data/${nodes}"
graph="${output_directory}/graph_${nodes}.tuned.dat"
turns="${output_directory}/graph_${nodes}.turns.dat"
solution="${output_directory}/min_dist_${nodes}.dat"
clusters="${output_directory}/clusters_${nodes}.dat"
segments="${output_directory}/route_segments_${nodes}.csv"
hierholzer_segments="${output_directory}/route_segments_${nodes}_h.csv"
coordinates="${output_directory}/graph_${nodes}.coords.csv"
video="${output_directory}/route_${nodes}.gif"
hierholzer_video="${output_directory}/route_${nodes}_h.gif"

make -s path-clusters
./pathSortClusterExec "$graph" "$turns" "$solution" "$clusters"
python3 tools/generate_route_video.py \
    --segments "$segments" \
    --coords "$coordinates" \
    --output "$video" \
    --cluster
python3 tools/generate_route_video.py \
    --segments "$hierholzer_segments" \
    --coords "$coordinates" \
    --output "$hierholzer_video" \
    --cluster
