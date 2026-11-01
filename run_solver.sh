#!/bin/bash

set -e

cluster_percentage="${CLUSTER_PERCENTAGE:-10}"
output_directory="data/$1"
graph="$output_directory/graph_$1.dat"
turns="$output_directory/graph_$1.turns.dat"
clusters="$output_directory/graph_clusters_$1.dat"
neutral_nodes="$output_directory/graph_$1.cluster-neutral-nodes.dat"
solution="$output_directory/min_dist_$1.dat"
segments="$output_directory/route_segments_$1.csv"
coordinates="$output_directory/graph_$1.coords.csv"
video="$output_directory/route_$1.gif"

make -s all
python3 tools/generate_graph.py "$1" --free --vehicles 1 --svg
python3 clusterGeneration/generate_clusters.py --graph-only \
    --graph "$graph" --nodes "$1" --percentage "$cluster_percentage" \
    --coords "$coordinates" \
    --neutral-nodes-output "$neutral_nodes" \
    --output "$clusters"
./solverExec "$graph" "$turns" "${@:2}" --expanded-graph \
    --clusters "$clusters" --neutral-nodes "$neutral_nodes"
./pathSortExec "$graph" "$turns" "$solution"
python3 tools/generate_route_video.py --segments "$segments" --coords "$coordinates" --output "$video"
