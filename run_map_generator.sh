#!/bin/bash

set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "Uso: ./run_map_generator.sh <mapa> [opciones del generador]" >&2
    exit 1
fi

map_name="$1"
shift

make -s all
python3 generator/mapToOsmGraph.py "$map_name" --sin-grilla "$@"
node_count=$(awk 'NR == 1 { print $2 }' "data/generator/input/$map_name.dat")
./solverExec "data/generator/input/$map_name.dat" "data/generator/input/$map_name.turns.dat" fixAndOptimize topKDeadheadCost
./pathSortExec "data/generator/input/$map_name.dat" "data/generator/input/$map_name.turns.dat" "output/dist/out_$node_count.dat"
python3 tools/generate_route_video.py --segments "output/order/route_segments_$node_count.csv" --coords "data/coords/graph_$node_count.coords.csv" --output output/videos/route_$node_count.gif
