#!/bin/bash

set -e

make -s all
./pathSortExec "input/graph_$1.dat" "input/graph_$1.turns.dat" "output/dist/out_$1.dat"
python3 tools/generate_route_video.py --segments "output/order/route_segments_$1.csv" --coords "data/graph_$1.coords.csv" --output output/videos/route.gif
