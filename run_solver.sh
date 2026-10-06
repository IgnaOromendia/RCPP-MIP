#!/bin/bash

set -e

make -s all
python3 tools/generate_graph.py "$1" --free --vehicles 1 --svg
./solverExec "input/graph_$1.dat" "input/graph_$1.turns.dat" "${@:2}"
./pathSortExec "input/graph_$1.dat" "input/graph_$1.turns.dat" "output/dist/out_$1.dat"
python3 tools/generate_route_video.py --segments "output/order/route_segments_$1.csv" --coords "data/coords/graph_$1.coords.csv" --output output/videos/route.gif
