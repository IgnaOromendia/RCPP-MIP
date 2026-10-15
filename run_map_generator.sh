#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Uso: $0 <mapa> <porcentaje_aristas> [opciones del generador]" >&2
    exit 1
fi

map_name="$1"
percentage="$2"
shift 2

if [[ ! "$map_name" =~ ^[A-Za-z0-9][A-Za-z0-9_-]*$ ]]; then
    echo "Error: nombre de mapa invalido: $map_name" >&2
    exit 1
fi

generated_graph="data/generator/input/${map_name}.dat"
generated_turns="data/generator/input/${map_name}.turns.dat"
source_coordinates="data/generator/coordinates/nodes_${map_name}.dat"
node_mapping="data/generator/mappings/nodes_${map_name}.dat"

make -s mip path-clusters
if [[ -f "$generated_graph" && -f "$generated_turns" \
      && -f "$source_coordinates" && -f "$node_mapping" ]]; then
    echo "Los datos del mapa $map_name ya existen; se omite mapToOsmGraph."
else
    python3 generator/mapToOsmGraph.py "$map_name" --sin-grilla "$@"
fi
node_count=$(awk 'NR == 1 { print $2 }' "data/generator/input/$map_name.dat")

output_directory="data/${node_count}"
graph="${output_directory}/graph_${node_count}.dat"
tuned_graph="${output_directory}/graph_${node_count}.tuned.dat"
mapping="${output_directory}/graph_${node_count}.tuned.mapping.dat"
turns="${output_directory}/graph_${node_count}.turns.dat"
expanded_turns="${output_directory}/graph_${node_count}.tuned.turns.dat"
solution="${output_directory}/min_dist_${node_count}.dat"
clusters="${output_directory}/clusters_${node_count}.dat"
graph_clusters="${output_directory}/graph_clusters_${node_count}.dat"
segments="${output_directory}/route_segments_${node_count}.csv"
hierholzer_segments="${output_directory}/route_segments_${node_count}_h.csv"
coordinates="${output_directory}/graph_${node_count}.coords.csv"
video="${output_directory}/route_${node_count}.gif"
hierholzer_video="${output_directory}/route_${node_count}_h.gif"
html="${output_directory}/route_${node_count}.html"
hierholzer_html="${output_directory}/route_${node_count}_h.html"

mkdir -p "$output_directory"
cp "$generated_graph" "$graph"
cp "$generated_turns" "$turns"

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

python3 clusterGeneration/generate_clusters.py --graph-only \
    --graph "$graph" --nodes "$node_count" --percentage "$percentage" \
    --output "$graph_clusters"
python3 tune_cluster_crossing.py "$graph" "$graph_clusters" "$tuned_graph" "$mapping" \
    --coords "$coordinates" --turns "$turns" --expanded-turns "$expanded_turns"
if [[ -s "$solution" ]]; then
    echo "La solucion RCPP ya existe en $solution; se omite el solver."
else
    expanded_nodes=$(awk 'NR == 1 { print $2 }' "$tuned_graph")
    expanded_solution="data/${expanded_nodes}/min_dist_${expanded_nodes}.dat"
    ./solverExec "$tuned_graph" "$expanded_turns" fixAndOptimize topKDeadheadCost
    python3 re-map-expanded-graph.py "$expanded_solution" "$mapping" "$solution" \
        --graph "$graph" --turns "$turns"
fi
python3 clusterGeneration/generate_clusters.py "$solution" \
    --percentage "$percentage" \
    --graph "$graph" \
    --coords "$coordinates"
sorter_status=0
./pathSortClusterExec "$graph" "$turns" "$solution" "$clusters" \
    || sorter_status=$?
if [[ "$sorter_status" -eq 0 ]]; then
    python3 tools/generate_route_video.py \
        --segments "$segments" \
        --coords "$coordinates" \
        --output "$video" \
        --cluster
    python3 tools/generate_route_html.py "$node_count" \
        --segments "$segments" \
        --coords "$coordinates" \
        --output "$html"
elif [[ "$sorter_status" -eq 2 ]]; then
    echo "No se encontro un orden factible; se generan solo las visualizaciones de Hierholzer."
else
    exit "$sorter_status"
fi
python3 tools/generate_route_video.py \
    --segments "$hierholzer_segments" \
    --coords "$coordinates" \
    --output "$hierholzer_video" \
    --cluster
python3 tools/generate_route_html.py "$node_count" \
    --segments "$hierholzer_segments" \
    --coords "$coordinates" \
    --output "$hierholzer_html"
