#!/bin/bash

set -e

make -s all
python3 tools/generate_graph.py "$1" --free --vehicles 1
./solverExec "input/graph_$1.dat" "input/graph_$1.turns.dat" "${@:2}"
./pathSortExec "input/graph_$1.dat" "input/graph_$1.turns.dat" out.dat orden.dat
