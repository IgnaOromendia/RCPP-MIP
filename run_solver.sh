#!/bin/bash

set -e

python3 tools/generate_graph.py "$1"
make -s all
./solverExec "input/graph_$1.dat" "input/graph_$1.turns.dat"
