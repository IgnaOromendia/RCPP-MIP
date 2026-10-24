#!/usr/bin/env bash
set -uo pipefail


status=0
latexmk -pdf -interaction=nonstopmode -halt-on-error tesis.tex || status=$?
latexmk -c tesis.tex

exit "$status"