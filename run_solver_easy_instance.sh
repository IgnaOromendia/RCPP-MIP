#!/bin/bash

set -e

cluster_percentage="${CLUSTER_PERCENTAGE:-10}"
output_directory="data/$1"
graph="$output_directory/graph_$1.dat"
tuned_graph="$output_directory/graph_$1.tuned.dat"
turns="$output_directory/graph_$1.turns.dat"
clusters="$output_directory/graph_clusters_$1.dat"
solution="$output_directory/min_dist_$1.dat"
segments="$output_directory/route_segments_$1.csv"
coordinates="$output_directory/graph_$1.coords.csv"
video="$output_directory/route_$1.gif"

make -s all
python3 tools/generate_graph.py "$1" --free --vehicles 1 --svg --demand-type integer --demand-min 1 --demand-max 1
python3 clusterGeneration/generate_clusters.py --graph-only \
    --graph "$graph" --nodes "$1" --percentage "$cluster_percentage" \
    --output "$clusters"
python3 tune_cluster_crossings.py "$graph" "$clusters" "$tuned_graph"
./solverExec "$tuned_graph" "$turns" "${@:2}"
./pathSortExec "$tuned_graph" "$turns" "$solution"
python3 tools/generate_route_video.py --segments "$segments" --coords "$coordinates" --output "$video"
