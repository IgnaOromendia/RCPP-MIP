#!/usr/bin/env bash

set -euo pipefail

script_directory=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
exec env RCPP_CLUSTER_STRATEGY=streets RCPP_OUTPUT_VARIANT=streets \
    "$script_directory/run_min_cluster_map.sh" "$@"
