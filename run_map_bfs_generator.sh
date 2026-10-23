#!/usr/bin/env bash

set -euo pipefail

script_directory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec env RCPP_CLUSTER_STRATEGY=bfs RCPP_OUTPUT_VARIANT=bfs \
    "$script_directory/run_min_cluster_map.sh" "$@"
