#!/bin/bash

set -e

make -s all
python3 tools/generate_graph.py "$1"
./solverExec "input/graph_$1.dat" "input/graph_$1.turns.dat" "${@:2}"
