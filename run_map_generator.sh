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
./solverExec "data/generator/input/$map_name.dat" "data/generator/input/$map_name.turns.dat" fixAndOptimizwe topKDeadheadCost
./pathSortExec "data/generator/input/$map_name.dat" "data/generator/input/$map_name.turns.dat" "output/dist/out_$node_count.dat"
