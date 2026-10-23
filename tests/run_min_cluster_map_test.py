#!/usr/bin/env python3
"""Check the map runner backed by bestClusterPathExec."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / "run_min_cluster_map.sh"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def executable(path: Path, contents: str):
    path.write_text(contents, encoding="utf-8")
    path.chmod(0o755)


def main():
    with tempfile.TemporaryDirectory(prefix="run-min-cluster-map-") as raw_directory:
        directory = Path(raw_directory)
        shutil.copy2(RUNNER, directory / RUNNER.name)
        for child in ("bin", "generator", "clusterGeneration", "tools"):
            (directory / child).mkdir()

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
            "data/generator/mappings data/generator/osm\n"
            "  printf '1 7 1 0 0\\n1\\n' > data/generator/input/sample.dat\n"
            "  : > data/generator/input/sample.turns.dat\n"
            "  printf '100 -58.1 -34.1\\n200 -58.2 -34.2\\n' > "
            "data/generator/coordinates/nodes_sample.dat\n"
            "  printf '100 1\\n200 2\\n' > data/generator/mappings/nodes_sample.dat\n"
            "  printf 'node_id reasons\\n2 avenida,primary\\n' > "
            "data/generator/mappings/neutral_nodes_sample.dat\n"
            "  printf '<osm/>\\n' > data/generator/osm/sample.osm\n"
            "elif [[ \"$1\" == 'clusterGeneration/generate_clusters.py' ]]; then\n"
            "  mkdir -p data/sample\n"
            "  if [[ \"$*\" == *'--graph-only'* ]]; then\n"
            "    printf '%s\\n' \"$*\" > data/sample/graph_clusters_7.dat\n"
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
            "  : > data/sample/graph_7.tuned.turns.dat\n"
            "  printf 'source_cluster target_cluster in_node out_node\\n"
            "1 2 1 2\\n' > "
            "data/sample/graph_7.tuned.in-out.dat\n"
            "  printf 'arc cluster\\n1 1\\n' > "
            "data/sample/graph_7.tuned.arc-clusters.dat\n"
            "elif [[ \"$1\" == 're-map-expanded-graph.py' ]]; then\n"
            "  printf 'OBJ: 1\\n' > \"$4\"\n"
            "fi\n",
        )
        executable(
            directory / "bestClusterPathExec",
            "#!/usr/bin/env bash\n"
            "printf 'best-cluster %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p data/7\n"
            "printf 'OBJ: 1\\n' > data/7/min_cluster_path_7.dat\n",
        )
        executable(
            directory / "pathSortClusterExec",
            "#!/usr/bin/env bash\n"
            "printf 'cluster-sort %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p data/sample\n"
            ": > data/sample/route_segments_7.csv\n"
            ": > data/sample/route_segments_7_h.csv\n",
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
        check(log[0] == "make -s mip-best-cluster path-clusters", log)
        check(
            "best-cluster data/sample/graph_7.tuned.dat "
            "data/sample/graph_7.tuned.turns.dat "
            "data/sample/graph_7.tuned.in-out.dat "
            "data/sample/graph_7.tuned.arc-clusters.dat" in log,
            f"bestClusterPathExec was not called with the tuned inputs: {log}",
        )
        check(not any(call.startswith("solver ") for call in log),
              f"the distance solver was called: {log}")
        check((directory / "data/sample/graph_clusters_7.html").is_file(),
              "interactive cluster HTML was not created")
        check(
            "python3 re-map-expanded-graph.py data/7/min_cluster_path_7.dat "
            "data/sample/graph_7.tuned.mapping.dat "
            "data/sample/min_cluster_path_7.dat "
            "--graph data/sample/graph_7.dat "
            "--turns data/sample/graph_7.turns.dat" in log,
            f"the cluster-path solution was not remapped: {log}",
        )
        check(
            "python3 clusterGeneration/generate_clusters.py "
            "data/sample/min_cluster_path_7.dat --graph-clusters "
            "data/sample/graph_clusters_7.dat --graph "
            "data/sample/graph_7.dat --nodes 7 --coords "
            "data/sample/graph_7.coords.csv --output "
            "data/sample/clusters_7.dat" in log,
            f"the original node count was not passed for the renamed solution: {log}",
        )
        check(
            "cluster-sort data/sample/graph_7.dat "
            "data/sample/graph_7.turns.dat "
            "data/sample/min_cluster_path_7.dat "
            "data/sample/clusters_7.dat data/sample/order_7.dat "
            "data/sample/route_segments_7.csv" in log,
            f"the remapped cluster-path solution was not sorted: {log}",
        )

        (directory / "calls.log").write_text("", encoding="utf-8")
        cached = subprocess.run(
            [directory / RUNNER.name, "sample", "10"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(cached.returncode == 0, cached.stdout + cached.stderr)
        cached_log = (directory / "calls.log").read_text(
            encoding="utf-8").splitlines()
        check(not any(call.startswith("best-cluster ") for call in cached_log),
              f"cached cluster-path solution was recomputed: {cached_log}")
        check("se omite el solver" in cached.stdout,
              "cached cluster-path solution did not report the skipped solver")

        (directory / "calls.log").write_text("", encoding="utf-8")
        changed = subprocess.run(
            [directory / RUNNER.name, "sample", "20"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(changed.returncode == 0, changed.stdout + changed.stderr)
        changed_log = (directory / "calls.log").read_text(
            encoding="utf-8").splitlines()
        check(any(call.startswith("best-cluster ") for call in changed_log),
              f"changed cluster inputs reused a stale solution: {changed_log}")
        check("se omite el solver" not in changed.stdout,
              "changed cluster inputs reported a cached solution")

        invalid = subprocess.run(
            [directory / RUNNER.name, "../sample", "10"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(invalid.returncode == 1 and "nombre de mapa invalido" in invalid.stderr,
              "an unsafe map name was accepted")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
