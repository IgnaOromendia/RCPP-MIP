#!/bin/bash

set -e

make -s all
./pathSortExec "input/graph_$1.dat" "input/graph_$1.turns.dat" "output/dist/out_$1.dat"
python3 tools/generate_route_video.py --segments route_segments.csv --coords "data/graph_$1.coords.csv" --output route.gif
