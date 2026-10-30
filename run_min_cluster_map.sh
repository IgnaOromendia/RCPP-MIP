#!/usr/bin/env bash

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "Uso: $0 <mapa> <porcentaje_aristas> [opciones del generador]" >&2
    exit 1
fi

map_name="$1"
percentage="$2"
shift 2

cluster_strategy="${RCPP_CLUSTER_STRATEGY:-streets}"
output_variant="${RCPP_OUTPUT_VARIANT:-}"
if [[ "$cluster_strategy" != "bfs" && "$cluster_strategy" != "streets" ]]; then
    echo "Error: estrategia de clusters invalida: $cluster_strategy" >&2
    exit 1
fi

if [[ ! "$map_name" =~ ^[A-Za-z0-9][A-Za-z0-9_-]*$ ]]; then
    echo "Error: nombre de mapa invalido: $map_name" >&2
    exit 1
fi

generated_graph="data/generator/input/${map_name}.dat"
generated_turns="data/generator/input/${map_name}.turns.dat"
source_coordinates="data/generator/coordinates/nodes_${map_name}.dat"
node_mapping="data/generator/mappings/nodes_${map_name}.dat"
generated_neutral_nodes="data/generator/mappings/neutral_nodes_${map_name}.dat"
generated_osm="data/generator/osm/${map_name}.osm"

make -s mip-best-cluster path-clusters
if [[ -f "$generated_graph" && -f "$generated_turns" \
      && -f "$source_coordinates" && -f "$node_mapping" \
      && ( "$cluster_strategy" == "bfs" \
           || ( -f "$generated_neutral_nodes" && -f "$generated_osm" ) ) ]]; then
    echo "Los datos del mapa $map_name ya existen; se omite mapToOsmGraph."
else
    python3 generator/mapToOsmGraph.py "$map_name" --sin-grilla "$@"
fi
node_count=$(awk 'NR == 1 { print $2 }' "data/generator/input/$map_name.dat")

output_directory="data/${map_name}"
if [[ -n "$output_variant" ]]; then
    output_directory="${output_directory}/${output_variant}"
fi
graph="${output_directory}/graph_${node_count}.dat"
turns="${output_directory}/graph_${node_count}.turns.dat"
solution="${output_directory}/min_cluster_path_${node_count}.dat"
solution_signature="${solution}.inputs.cksum"
clusters="${output_directory}/clusters_${node_count}.dat"
graph_clusters="${output_directory}/graph_clusters_${node_count}.dat"
cluster_html="${output_directory}/graph_clusters_${node_count}.html"
order="${output_directory}/order_${node_count}.dat"
segments="${output_directory}/route_segments_${node_count}.csv"
hierholzer_segments="${output_directory}/route_segments_${node_count}_h.csv"
coordinates="${output_directory}/graph_${node_count}.coords.csv"
neutral_nodes="${output_directory}/graph_${node_count}.neutral-nodes.dat"
cluster_neutral_nodes="${output_directory}/graph_${node_count}.cluster-neutral-nodes.dat"
video="${output_directory}/route_${node_count}.gif"
hierholzer_video="${output_directory}/route_${node_count}_h.gif"
html="${output_directory}/route_${node_count}.html"
hierholzer_html="${output_directory}/route_${node_count}_h.html"

mkdir -p "$output_directory"
cp "$generated_graph" "$graph"
cp "$generated_turns" "$turns"
if [[ "$cluster_strategy" == "streets" ]]; then
    cp "$generated_neutral_nodes" "$neutral_nodes"
fi

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

if [[ "$cluster_strategy" == "streets" ]]; then
    python3 clusterGeneration/generate_clusters.py --graph-only \
        --graph "$graph" --nodes "$node_count" --percentage "$percentage" \
        --coords "$coordinates" --neutral-nodes "$neutral_nodes" \
        --neutral-nodes-output "$cluster_neutral_nodes" \
        --osm "$generated_osm" --node-mapping "$node_mapping" \
        --output "$graph_clusters"
    python3 generator/plot_clusters.py \
        --clusters "$graph_clusters" --graph "$graph" --coords "$coordinates" \
        --neutral-nodes "$cluster_neutral_nodes" --output "$cluster_html"
else
    python3 clusterGeneration/generate_clusters.py --graph-only \
        --graph "$graph" --nodes "$node_count" --percentage "$percentage" \
        --coords "$coordinates" --strategy bfs \
        --neutral-nodes-output "$cluster_neutral_nodes" \
        --output "$graph_clusters"
    python3 generator/plot_clusters.py \
        --clusters "$graph_clusters" --graph "$graph" --coords "$coordinates" \
        --output "$cluster_html"
fi
current_solution_signature=$({
    printf 'strategy %s\n' "$cluster_strategy"
    printf 'percentage %s\n' "$percentage"
    printf 'format neutral-expanded-v1\n'
    cksum "$graph" "$turns" "$graph_clusters" "$cluster_neutral_nodes"
    if [[ "$cluster_strategy" == "streets" ]]; then
        cksum "$neutral_nodes"
    fi
})
cached_solution_signature=""
if [[ -f "$solution_signature" ]]; then
    cached_solution_signature=$(<"$solution_signature")
fi
if [[ -s "$solution" && "$cached_solution_signature" == "$current_solution_signature" ]]; then
    echo "La solucion por clusters ya existe en $solution; se omite el solver."
else
    ./bestClusterPathExec "$graph" "$turns" --clusters "$graph_clusters" \
        --neutral-nodes "$cluster_neutral_nodes" --output "$solution"
    printf '%s\n' "$current_solution_signature" > "${solution_signature}.tmp"
    mv "${solution_signature}.tmp" "$solution_signature"
fi
python3 clusterGeneration/generate_clusters.py "$solution" \
    --graph-clusters "$graph_clusters" \
    --graph "$graph" \
    --nodes "$node_count" \
    --coords "$coordinates" \
    --output "$clusters"
sorter_status=0
./pathSortClusterExec "$graph" "$turns" "$solution" "$clusters" \
    "$order" "$segments" \
    || sorter_status=$?
if [[ "$sorter_status" -eq 0 ]]; then
    python3 tools/generate_route_video.py \
        --segments "$segments" \
        --coords "$coordinates" \
        --output "$video" \
        --neutral-nodes "$cluster_neutral_nodes" \
        --cluster
    python3 tools/generate_route_html.py "$node_count" \
        --segments "$segments" \
        --coords "$coordinates" \
        --graph-clusters "$graph_clusters" \
        --neutral-nodes "$cluster_neutral_nodes" \
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
    --neutral-nodes "$cluster_neutral_nodes" \
    --cluster
python3 tools/generate_route_html.py "$node_count" \
    --segments "$hierholzer_segments" \
    --coords "$coordinates" \
    --graph-clusters "$graph_clusters" \
    --neutral-nodes "$cluster_neutral_nodes" \
    --output "$hierholzer_html"
