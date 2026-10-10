#!/usr/bin/env python3
"""Check that the simple solver runners preprocess cluster crossings first."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def executable(path: Path, contents: str):
    path.write_text(contents, encoding="utf-8")
    path.chmod(0o755)


def run_case(runner_name: str, generation_tail: str):
    runner = ROOT / runner_name
    with tempfile.TemporaryDirectory(prefix="run-solver-preprocess-") as raw_directory:
        directory = Path(raw_directory)
        shutil.copy2(runner, directory / runner.name)
        (directory / "bin").mkdir()
        (directory / "tools").mkdir()
        (directory / "clusterGeneration").mkdir()
        executable(
            directory / "bin/make",
            "#!/usr/bin/env bash\nprintf 'make %s\\n' \"$*\" >> calls.log\n",
        )
        executable(
            directory / "bin/python3",
            "#!/usr/bin/env bash\n"
            "printf 'python3 %s\\n' \"$*\" >> calls.log\n"
            "if [[ \"$1\" == 'tools/generate_graph.py' ]]; then\n"
            "  mkdir -p data/7\n"
            "  : > data/7/graph_7.dat\n"
            "  : > data/7/graph_7.turns.dat\n"
            "elif [[ \"$1\" == 'clusterGeneration/generate_clusters.py' ]]; then\n"
            "  mkdir -p data/7\n"
            "  : > data/7/graph_clusters_7.dat\n"
            "elif [[ \"$1\" == 'tune_cluster_crossings.py' ]]; then\n"
            "  : > data/7/graph_7.tuned.dat\n"
            "fi\n",
        )
        executable(
            directory / "solverExec",
            "#!/usr/bin/env bash\nprintf 'solver %s\\n' \"$*\" >> calls.log\n",
        )
        executable(
            directory / "pathSortExec",
            "#!/usr/bin/env bash\nprintf 'sort %s\\n' \"$*\" >> calls.log\n",
        )
        environment = os.environ.copy()
        environment["PATH"] = f"{directory / 'bin'}:{environment['PATH']}"
        environment["CLUSTER_PERCENTAGE"] = "25"
        result = subprocess.run(
            [directory / runner.name, "7", "mip"], cwd=directory,
            env=environment, capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        log = (directory / "calls.log").read_text(encoding="utf-8").splitlines()
        check(log == [
            "make -s all",
            f"python3 tools/generate_graph.py 7 {generation_tail}",
            "python3 clusterGeneration/generate_clusters.py --graph-only "
            "--graph data/7/graph_7.dat --nodes 7 --percentage 25 "
            "--output data/7/graph_clusters_7.dat",
            "python3 tune_cluster_crossings.py data/7/graph_7.dat "
            "data/7/graph_clusters_7.dat data/7/graph_7.tuned.dat",
            "solver data/7/graph_7.tuned.dat data/7/graph_7.turns.dat mip",
            "sort data/7/graph_7.tuned.dat data/7/graph_7.turns.dat data/7/min_dist_7.dat",
            "python3 tools/generate_route_video.py --segments "
            "data/7/route_segments_7.csv --coords "
            "data/7/graph_7.coords.csv --output data/7/route_7.gif",
        ], f"unexpected {runner_name} pipeline: {log}")


def main():
    run_case("run_solver.sh", "--free --vehicles 1 --svg")
    run_case(
        "run_solver_easy_instance.sh",
        "--free --vehicles 1 --svg --demand-type integer --demand-min 1 --demand-max 1",
    )


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
