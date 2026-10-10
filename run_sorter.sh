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
segments="${output_directory}/route_segments_${nodes}.csv"
coordinates="${output_directory}/graph_${nodes}.coords.csv"
video="${output_directory}/route_${nodes}.gif"

make -s all
./pathSortExec "$graph" "$turns" "$solution"
python3 tools/generate_route_video.py \
    --segments "$segments" \
    --coords "$coordinates" \
    --output "$video"
