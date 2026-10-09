#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Uso: $0 <cantidad_nodos> <porcentaje_aristas> [mip|fixAndOptimize [maxDeadheadCost|random|topKDeadheadCost]]" >&2
    exit 1
fi

nodes="$1"
percentage="$2"
shift
shift

graph="input/graph_${nodes}.dat"
turns="input/graph_${nodes}.turns.dat"
solution="output/dist/out_${nodes}.dat"
clusters="data/clusters/clusters_${nodes}.dat"
segments="output/order/route_segments_${nodes}.csv"
hierholzer_segments="output/order/route_segments_${nodes}_h.csv"
coordinates="data/coords/graph_${nodes}.coords.csv"
video="output/videos/route_${nodes}.gif"
hierholzer_video="output/videos/route_${nodes}_h.gif"

make -s mip path-clusters
python3 tools/generate_graph.py "$nodes" --free --vehicles 1 --svg
if [[ -s "$solution" ]]; then
    echo "La solucion RCPP ya existe en $solution; se omite el solver."
else
    ./solverExec "$graph" "$turns" "$@"
fi
python3 clusterGeneration/generate_clusters.py "$solution" \
    --percentage "$percentage" \
    --graph "$graph" \
    --coords "$coordinates"
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
