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
            "elif [[ \"$1\" == 'clusterGeneration/generate_clusters.py' ]]; then\n"
            "  mkdir -p data/7\n"
            "  if [[ \"$*\" == *'--graph-only'* ]]; then\n"
            "    : > data/7/graph_clusters_7.dat\n"
            "  else\n"
            "    : > data/7/clusters_7.dat\n"
            "  fi\n"
            "elif [[ \"$1\" == 'tune_cluster_crossings.py' ]]; then\n"
            "  : > data/7/graph_7.tuned.dat\n"
            "elif [[ \"$1\" == 'tools/generate_route_video.py' ]]; then\n"
            "  mkdir -p data/7\n"
            "  if [[ \"$*\" == *'route_7_h.gif'* ]]; then\n"
            "    : > data/7/route_7_h.gif\n"
            "  else\n"
            "    : > data/7/route_7.gif\n"
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
            "mkdir -p data/7\n"
            ": > data/7/route_segments_7.csv\n"
            ": > data/7/route_segments_7_h.csv\n",
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
        coordinates = "data/7/graph_7.coords.csv"
        check(log == [
            "make -s mip path-clusters",
            "python3 generator/mapToOsmGraph.py sample --sin-grilla "
            "--resources-dir resources",
            "python3 clusterGeneration/generate_clusters.py --graph-only "
            "--graph data/7/graph_7.dat --nodes 7 --percentage 10 "
            "--output data/7/graph_clusters_7.dat",
            "python3 tune_cluster_crossings.py data/7/graph_7.dat "
            "data/7/graph_clusters_7.dat "
            "data/7/graph_7.tuned.dat",
            "solver data/7/graph_7.tuned.dat "
            "data/7/graph_7.turns.dat fixAndOptimize topKDeadheadCost",
            "python3 clusterGeneration/generate_clusters.py data/7/min_dist_7.dat "
            "--percentage 10 --graph data/7/graph_7.tuned.dat "
            f"--coords {coordinates}",
            "cluster-sort data/7/graph_7.tuned.dat "
            "data/7/graph_7.turns.dat data/7/min_dist_7.dat "
            "data/7/clusters_7.dat",
            "python3 tools/generate_route_video.py --segments "
            f"data/7/route_segments_7.csv --coords {coordinates} "
            "--output data/7/route_7.gif --cluster",
            "python3 tools/generate_route_video.py --segments "
            f"data/7/route_segments_7_h.csv --coords {coordinates} "
            "--output data/7/route_7_h.gif --cluster",
        ], f"unexpected pipeline: {log}")
        coordinate_rows = (directory / coordinates).read_text(encoding="utf-8").splitlines()
        check(coordinate_rows == [
            "node_id,x,y", "1,-58.1,-34.1", "2,-58.2,-34.2"
        ], f"unexpected coordinate CSV: {coordinate_rows}")

        solution = directory / "data/7/min_dist_7.dat"
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
            "python3 generator/mapToOsmGraph.py sample --sin-grilla "
            "--resources-dir resources",
            "python3 clusterGeneration/generate_clusters.py --graph-only "
            "--graph data/7/graph_7.dat --nodes 7 --percentage 10 "
            "--output data/7/graph_clusters_7.dat",
            "python3 tune_cluster_crossings.py data/7/graph_7.dat "
            "data/7/graph_clusters_7.dat "
            "data/7/graph_7.tuned.dat",
            "python3 clusterGeneration/generate_clusters.py data/7/min_dist_7.dat "
            "--percentage 10 --graph data/7/graph_7.tuned.dat "
            f"--coords {coordinates}",
            "cluster-sort data/7/graph_7.tuned.dat "
            "data/7/graph_7.turns.dat data/7/min_dist_7.dat "
            "data/7/clusters_7.dat",
            "python3 tools/generate_route_video.py --segments "
            f"data/7/route_segments_7.csv --coords {coordinates} "
            "--output data/7/route_7.gif --cluster",
            "python3 tools/generate_route_video.py --segments "
            f"data/7/route_segments_7_h.csv --coords {coordinates} "
            "--output data/7/route_7_h.gif --cluster",
        ], f"solver was not skipped for a cached solution: {cached_log}")
        check("se omite el solver" in cached.stdout,
              "cached solution did not report that the solver was skipped")
        check(solution.read_text(encoding="utf-8") == "OBJ: 42\n",
              "cached solution was overwritten")

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
