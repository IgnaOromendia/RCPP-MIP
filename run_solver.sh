#!/bin/bash

set -e

cluster_percentage="${CLUSTER_PERCENTAGE:-10}"
output_directory="data/$1"
graph="$output_directory/graph_$1.dat"
tuned_graph="$output_directory/graph_$1.tuned.dat"
mapping="$output_directory/graph_$1.tuned.mapping.dat"
turns="$output_directory/graph_$1.turns.dat"
clusters="$output_directory/graph_clusters_$1.dat"
solution="$output_directory/min_dist_$1.dat"
segments="$output_directory/route_segments_$1.csv"
coordinates="$output_directory/graph_$1.coords.csv"
video="$output_directory/route_$1.gif"

make -s all
python3 tools/generate_graph.py "$1" --free --vehicles 1 --svg
python3 clusterGeneration/generate_clusters.py --graph-only \
    --graph "$graph" --nodes "$1" --percentage "$cluster_percentage" \
    --output "$clusters"
python3 tune_cluster_crossing.py "$graph" "$clusters" "$tuned_graph" "$mapping"
expanded_nodes=$(awk 'NR == 1 { print $2 }' "$tuned_graph")
expanded_solution="data/${expanded_nodes}/min_dist_${expanded_nodes}.dat"
./solverExec "$tuned_graph" "$turns" "${@:2}"
python3 re-map-expanded-graph.py "$expanded_solution" "$mapping" "$solution" \
    --graph "$graph" --turns "$turns"
./pathSortExec "$graph" "$turns" "$solution"
python3 tools/generate_route_video.py --segments "$segments" --coords "$coordinates" --output "$video"
