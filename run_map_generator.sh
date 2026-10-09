#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Uso: $0 <mapa> <porcentaje_aristas> [opciones del generador]" >&2
    exit 1
fi

map_name="$1"
percentage="$2"
shift 2

make -s mip path-clusters
python3 generator/mapToOsmGraph.py "$map_name" --sin-grilla "$@"
node_count=$(awk 'NR == 1 { print $2 }' "data/generator/input/$map_name.dat")

graph="data/generator/input/${map_name}.dat"
turns="data/generator/input/${map_name}.turns.dat"
solution="output/dist/out_${node_count}.dat"
clusters="data/clusters/clusters_${node_count}.dat"
segments="output/order/route_segments_${node_count}.csv"
hierholzer_segments="output/order/route_segments_${node_count}_h.csv"
source_coordinates="data/generator/coordinates/nodes_${map_name}.dat"
node_mapping="data/generator/mappings/nodes_${map_name}.dat"
coordinates="data/generator/coordinates/nodes_${map_name}.coords.csv"
video="output/videos/route_${node_count}.gif"
hierholzer_video="output/videos/route_${node_count}_h.gif"

{
    echo "node_id,x,y"
    awk '
        NR == FNR {
            internal_id[$1] = $2
            next
        }
        !($1 in internal_id) {
            printf "Error: no hay ID interno para el nodo OSM %s\n", $1 > "/dev/stderr"
            invalid = 1
            next
        }
        {
            print internal_id[$1] "," $2 "," $3
        }
        END { exit invalid }
    ' "$node_mapping" "$source_coordinates"
} > "$coordinates"

if [[ -s "$solution" ]]; then
    echo "La solucion RCPP ya existe en $solution; se omite el solver."
else
    ./solverExec "$graph" "$turns" fixAndOptimize topKDeadheadCost
fi
python3 clusterGeneration/generate_clusters.py "$solution" \
    --percentage "$percentage" \
    --graph "$graph" \
    --coords "$coordinates"
./pathSortClusterExec "$graph" "$turns" "$solution" "$clusters"
python3 tools/generate_route_video.py \
    --segments "$segments" \
    --coords "$coordinates" \
    --output "$video" \
    --cluster
python3 tools/generate_route_video.py \
    --segments "$hierholzer_segments" \
    --coords "$coordinates" \
    --output "$hierholzer_video" \
    --cluster
