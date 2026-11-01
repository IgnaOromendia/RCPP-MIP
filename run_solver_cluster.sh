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

output_directory="data/${nodes}"
graph="${output_directory}/graph_${nodes}.dat"
turns="${output_directory}/graph_${nodes}.turns.dat"
solution="${output_directory}/min_dist_${nodes}.dat"
clusters="${output_directory}/clusters_${nodes}.dat"
graph_clusters="${output_directory}/graph_clusters_${nodes}.dat"
neutral_nodes="${output_directory}/graph_${nodes}.cluster-neutral-nodes.dat"
segments="${output_directory}/route_segments_${nodes}.csv"
hierholzer_segments="${output_directory}/route_segments_${nodes}_h.csv"
coordinates="${output_directory}/graph_${nodes}.coords.csv"
video="${output_directory}/route_${nodes}.gif"
hierholzer_video="${output_directory}/route_${nodes}_h.gif"

make -s mip path-clusters
python3 tools/generate_graph.py "$nodes" --free --vehicles 1 --svg
python3 clusterGeneration/generate_clusters.py --graph-only \
    --graph "$graph" --nodes "$nodes" --percentage "$percentage" \
    --coords "$coordinates" \
    --neutral-nodes-output "$neutral_nodes" \
    --output "$graph_clusters"
if [[ -s "$solution" ]]; then
    echo "La solucion RCPP ya existe en $solution; se omite el solver."
else
    ./solverExec "$graph" "$turns" "$@" --expanded-graph \
        --clusters "$graph_clusters" --neutral-nodes "$neutral_nodes"
fi
python3 clusterGeneration/generate_clusters.py "$solution" \
    --graph-clusters "$graph_clusters" \
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
