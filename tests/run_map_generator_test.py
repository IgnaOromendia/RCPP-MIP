#!/usr/bin/env python3
"""Check the map generator, solver, clustering, ordering and GIF pipeline."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / "run_map_generator.sh"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def executable(path: Path, contents: str):
    path.write_text(contents, encoding="utf-8")
    path.chmod(0o755)


def main():
    with tempfile.TemporaryDirectory(prefix="run-map-generator-") as raw_directory:
        directory = Path(raw_directory)
        shutil.copy2(RUNNER, directory / RUNNER.name)
        (directory / "bin").mkdir()
        (directory / "generator").mkdir()
        (directory / "clusterGeneration").mkdir()
        (directory / "tools").mkdir()

        executable(
            directory / "bin" / "make",
            "#!/usr/bin/env bash\n"
            "printf 'make %s\\n' \"$*\" >> calls.log\n",
        )
        executable(
            directory / "bin" / "python3",
            "#!/usr/bin/env bash\n"
            "printf 'python3 %s\\n' \"$*\" >> calls.log\n"
            "if [[ \"$1\" == 'generator/mapToOsmGraph.py' ]]; then\n"
            "  mkdir -p data/generator/input data/generator/coordinates "
            "data/generator/mappings\n"
            "  printf '1 7 1 0 0\\n1\\n' > data/generator/input/sample.dat\n"
            "  : > data/generator/input/sample.turns.dat\n"
            "  printf '4338404152 -58.1 -34.1\\n12542889046 -58.2 -34.2\\n' > "
            "data/generator/coordinates/nodes_sample.dat\n"
            "  printf '4338404152 1\\n12542889046 2\\n' > "
            "data/generator/mappings/nodes_sample.dat\n"
            "  printf 'node_id reasons\\n2 avenida,primary\\n' > "
            "data/generator/mappings/neutral_nodes_sample.dat\n"
            "elif [[ \"$1\" == 'clusterGeneration/generate_clusters.py' ]]; then\n"
            "  mkdir -p data/sample\n"
            "  if [[ \"$*\" == *'--graph-only'* ]]; then\n"
            "    : > data/sample/graph_clusters_7.dat\n"
            "    printf 'node_id reasons\\n2 avenida,divider\\n' > "
            "data/sample/graph_7.cluster-neutral-nodes.dat\n"
            "  else\n"
            "    : > data/sample/clusters_7.dat\n"
            "  fi\n"
            "elif [[ \"$1\" == 'generator/plot_clusters.py' ]]; then\n"
            "  : > data/sample/graph_clusters_7.html\n"
            "elif [[ \"$1\" == 'tune_cluster_crossing.py' ]]; then\n"
            "  printf '1 7 1 0 0\\n1\\n' > data/sample/graph_7.tuned.dat\n"
            "  : > data/sample/graph_7.tuned.mapping.dat\n"
            "elif [[ \"$1\" == 're-map-expanded-graph.py' ]]; then\n"
            "  : > \"$4\"\n"
            "elif [[ \"$1\" == 'tools/generate_route_video.py' ]]; then\n"
            "  mkdir -p data/sample\n"
            "  if [[ \"$*\" == *'route_7_h.gif'* ]]; then\n"
            "    : > data/sample/route_7_h.gif\n"
            "  else\n"
            "    : > data/sample/route_7.gif\n"
            "  fi\n"
            "elif [[ \"$1\" == 'tools/generate_route_html.py' ]]; then\n"
            "  mkdir -p data/sample\n"
            "  if [[ \"$*\" == *'route_7_h.html'* ]]; then\n"
            "    : > data/sample/route_7_h.html\n"
            "  else\n"
            "    : > data/sample/route_7.html\n"
            "  fi\n"
            "fi\n",
        )
        executable(
            directory / "solverExec",
            "#!/usr/bin/env bash\n"
            "printf 'solver %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p data/7\n"
            ": > data/7/min_dist_7.dat\n",
        )
        executable(
            directory / "pathSortClusterExec",
            "#!/usr/bin/env bash\n"
            "printf 'cluster-sort %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p data/sample\n"
            ": > data/sample/route_segments_7.csv\n"
            ": > data/sample/route_segments_7_h.csv\n"
            "if [[ \"${PATH_SORT_NO_SOLUTION:-0}\" == '1' ]]; then exit 2; fi\n",
        )

        environment = os.environ.copy()
        environment["PATH"] = f"{directory / 'bin'}:{environment['PATH']}"
        result = subprocess.run(
            [directory / RUNNER.name, "sample", "10", "--resources-dir", "resources"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        log = (directory / "calls.log").read_text(encoding="utf-8").splitlines()
        coordinates = "data/sample/graph_7.coords.csv"
        check(log == [
            "make -s mip path-clusters",
            "python3 generator/mapToOsmGraph.py sample --sin-grilla "
            "--resources-dir resources",
            "python3 clusterGeneration/generate_clusters.py --graph-only "
            "--graph data/sample/graph_7.dat --nodes 7 --percentage 10 "
            f"--coords {coordinates} --neutral-nodes "
            "data/sample/graph_7.neutral-nodes.dat "
            "--neutral-nodes-output data/sample/graph_7.cluster-neutral-nodes.dat "
            "--strategy straight-dividers "
            "--output data/sample/graph_clusters_7.dat",
            "python3 generator/plot_clusters.py --clusters "
            "data/sample/graph_clusters_7.dat "
            f"--coords {coordinates} --neutral-nodes "
            "data/sample/graph_7.cluster-neutral-nodes.dat "
            "--output data/sample/graph_clusters_7.html",
            "python3 tune_cluster_crossing.py data/sample/graph_7.dat "
            "data/sample/graph_clusters_7.dat "
            "data/sample/graph_7.tuned.dat data/sample/graph_7.tuned.mapping.dat "
            f"--coords {coordinates} --neutral-nodes "
            "data/sample/graph_7.cluster-neutral-nodes.dat "
            "--turns data/sample/graph_7.turns.dat --expanded-turns "
            "data/sample/graph_7.tuned.turns.dat",
            "solver data/sample/graph_7.tuned.dat "
            "data/sample/graph_7.tuned.turns.dat fixAndOptimize topKDeadheadCost",
            "python3 re-map-expanded-graph.py data/7/min_dist_7.dat "
            "data/sample/graph_7.tuned.mapping.dat data/sample/min_dist_7.dat "
            "--graph data/sample/graph_7.dat --turns data/sample/graph_7.turns.dat",
            "python3 clusterGeneration/generate_clusters.py data/sample/min_dist_7.dat "
            "--graph-clusters data/sample/graph_clusters_7.dat "
            "--graph data/sample/graph_7.dat "
            f"--coords {coordinates} --output data/sample/clusters_7.dat",
            "cluster-sort data/sample/graph_7.dat "
            "data/sample/graph_7.turns.dat data/sample/min_dist_7.dat "
            "data/sample/clusters_7.dat data/sample/order_7.dat "
            "data/sample/route_segments_7.csv",
            "python3 tools/generate_route_video.py --segments "
            f"data/sample/route_segments_7.csv --coords {coordinates} "
            "--output data/sample/route_7.gif --cluster",
            "python3 tools/generate_route_html.py 7 --segments "
            f"data/sample/route_segments_7.csv --coords {coordinates} "
            "--graph-clusters data/sample/graph_clusters_7.dat "
            "--output data/sample/route_7.html",
            "python3 tools/generate_route_video.py --segments "
            f"data/sample/route_segments_7_h.csv --coords {coordinates} "
            "--output data/sample/route_7_h.gif --cluster",
            "python3 tools/generate_route_html.py 7 --segments "
            f"data/sample/route_segments_7_h.csv --coords {coordinates} "
            "--graph-clusters data/sample/graph_clusters_7.dat "
            "--output data/sample/route_7_h.html",
        ], f"unexpected pipeline: {log}")
        coordinate_rows = (directory / coordinates).read_text(encoding="utf-8").splitlines()
        check(coordinate_rows == [
            "node_id,x,y", "1,-58.1,-34.1", "2,-58.2,-34.2"
        ], f"unexpected coordinate CSV: {coordinate_rows}")
        check((directory / "data/sample/route_7.html").is_file(),
              "optimized HTML was not created")
        check((directory / "data/sample/route_7_h.html").is_file(),
              "Hierholzer HTML was not created")
        check((directory / "data/sample/graph_clusters_7.html").is_file(),
              "interactive cluster HTML was not created")

        solution = directory / "data/sample/min_dist_7.dat"
        solution.write_text("OBJ: 42\n", encoding="utf-8")
        (directory / "calls.log").write_text("", encoding="utf-8")
        cached = subprocess.run(
            [directory / RUNNER.name, "sample", "10", "--resources-dir", "resources"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(cached.returncode == 0, cached.stdout + cached.stderr)
        cached_log = (directory / "calls.log").read_text(encoding="utf-8").splitlines()
        check(cached_log == [
            "make -s mip path-clusters",
            "python3 clusterGeneration/generate_clusters.py --graph-only "
            "--graph data/sample/graph_7.dat --nodes 7 --percentage 10 "
            f"--coords {coordinates} --neutral-nodes "
            "data/sample/graph_7.neutral-nodes.dat "
            "--neutral-nodes-output data/sample/graph_7.cluster-neutral-nodes.dat "
            "--strategy straight-dividers "
            "--output data/sample/graph_clusters_7.dat",
            "python3 generator/plot_clusters.py --clusters "
            "data/sample/graph_clusters_7.dat "
            f"--coords {coordinates} --neutral-nodes "
            "data/sample/graph_7.cluster-neutral-nodes.dat "
            "--output data/sample/graph_clusters_7.html",
            "python3 tune_cluster_crossing.py data/sample/graph_7.dat "
            "data/sample/graph_clusters_7.dat "
            "data/sample/graph_7.tuned.dat data/sample/graph_7.tuned.mapping.dat "
            f"--coords {coordinates} --neutral-nodes "
            "data/sample/graph_7.cluster-neutral-nodes.dat "
            "--turns data/sample/graph_7.turns.dat --expanded-turns "
            "data/sample/graph_7.tuned.turns.dat",
            "python3 clusterGeneration/generate_clusters.py data/sample/min_dist_7.dat "
            "--graph-clusters data/sample/graph_clusters_7.dat "
            "--graph data/sample/graph_7.dat "
            f"--coords {coordinates} --output data/sample/clusters_7.dat",
            "cluster-sort data/sample/graph_7.dat "
            "data/sample/graph_7.turns.dat data/sample/min_dist_7.dat "
            "data/sample/clusters_7.dat data/sample/order_7.dat "
            "data/sample/route_segments_7.csv",
            "python3 tools/generate_route_video.py --segments "
            f"data/sample/route_segments_7.csv --coords {coordinates} "
            "--output data/sample/route_7.gif --cluster",
            "python3 tools/generate_route_html.py 7 --segments "
            f"data/sample/route_segments_7.csv --coords {coordinates} "
            "--graph-clusters data/sample/graph_clusters_7.dat "
            "--output data/sample/route_7.html",
            "python3 tools/generate_route_video.py --segments "
            f"data/sample/route_segments_7_h.csv --coords {coordinates} "
            "--output data/sample/route_7_h.gif --cluster",
            "python3 tools/generate_route_html.py 7 --segments "
            f"data/sample/route_segments_7_h.csv --coords {coordinates} "
            "--graph-clusters data/sample/graph_clusters_7.dat "
            "--output data/sample/route_7_h.html",
        ], f"solver was not skipped for a cached solution: {cached_log}")
        check("se omite el solver" in cached.stdout,
              "cached solution did not report that the solver was skipped")
        check("se omite mapToOsmGraph" in cached.stdout,
              "cached map data did not report that mapToOsmGraph was skipped")
        check(solution.read_text(encoding="utf-8") == "OBJ: 42\n",
              "cached solution was overwritten")

        no_order_environment = environment.copy()
        no_order_environment["PATH_SORT_NO_SOLUTION"] = "1"
        (directory / "calls.log").write_text("", encoding="utf-8")
        no_order = subprocess.run(
            [directory / RUNNER.name, "sample", "10"],
            cwd=directory,
            env=no_order_environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(no_order.returncode == 0, no_order.stdout + no_order.stderr)
        no_order_log = (directory / "calls.log").read_text(
            encoding="utf-8").splitlines()
        check(not any("--segments data/sample/route_segments_7.csv" in call
                      for call in no_order_log),
              f"optimized GIF was generated without a feasible order: {no_order_log}")
        check(any("--segments data/sample/route_segments_7_h.csv" in call
                  for call in no_order_log),
              f"Hierholzer visualizations were not generated without a feasible order: "
              f"{no_order_log}")
        check(not any("--segments data/sample/route_segments_7.csv" in call
                      and "generate_route_html.py" in call
                      for call in no_order_log),
              f"optimized HTML was generated without a feasible order: {no_order_log}")
        check(any("generate_route_html.py 7 --segments "
                  "data/sample/route_segments_7_h.csv" in call
                  for call in no_order_log),
              "Hierholzer HTML was not generated without a feasible order: "
              f"{no_order_log}")
        check("visualizaciones de Hierholzer" in no_order.stdout,
              "missing Hierholzer fallback diagnostic")

        (directory / "data/generator/mappings/nodes_sample.dat").unlink()
        (directory / "calls.log").write_text("", encoding="utf-8")
        partial_cache = subprocess.run(
            [directory / RUNNER.name, "sample", "10"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(partial_cache.returncode == 0,
              partial_cache.stdout + partial_cache.stderr)
        partial_log = (directory / "calls.log").read_text(encoding="utf-8").splitlines()
        check("python3 generator/mapToOsmGraph.py sample --sin-grilla" in partial_log,
              f"mapToOsmGraph was skipped for an incomplete cache: {partial_log}")

        for arguments in ([], ["sample"]):
            missing = subprocess.run(
                [directory / RUNNER.name, *arguments],
                cwd=directory,
                env=environment,
                capture_output=True,
                text=True,
                timeout=10,
            )
            check(missing.returncode == 1 and "Uso:" in missing.stderr,
                  "missing required arguments did not show usage")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
