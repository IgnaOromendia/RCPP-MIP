#!/usr/bin/env python3
"""Check the complete solver, clustering, ordering and GIF pipeline."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / "run_solver_cluster.sh"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def executable(path: Path, contents: str):
    path.write_text(contents, encoding="utf-8")
    path.chmod(0o755)


def main():
    with tempfile.TemporaryDirectory(prefix="run-solver-cluster-") as raw_directory:
        directory = Path(raw_directory)
        shutil.copy2(RUNNER, directory / RUNNER.name)
        (directory / "tools").mkdir()
        (directory / "clusterGeneration").mkdir()
        (directory / "bin").mkdir()
        calls = directory / "calls.log"

        executable(
            directory / "bin" / "make",
            "#!/usr/bin/env bash\n"
            "printf 'make %s\\n' \"$*\" >> calls.log\n",
        )
        executable(
            directory / "bin" / "python3",
            "#!/usr/bin/env bash\n"
            "printf 'python3 %s\\n' \"$*\" >> calls.log\n"
            "if [[ \"$1\" == 'tools/generate_graph.py' ]]; then\n"
            "  mkdir -p input data/coords\n"
            "  : > \"input/graph_$2.dat\"\n"
            "  : > \"input/graph_$2.turns.dat\"\n"
            "  : > \"input/graph_$2.svg\"\n"
            "elif [[ \"$1\" == 'clusterGeneration/generate_clusters.py' ]]; then\n"
            "  mkdir -p data/clusters\n"
            "  : > data/clusters/clusters_7.dat\n"
            "  : > data/clusters/clusters_7.svg\n"
            "  : > data/clusters/clusters_7.png\n"
            "elif [[ \"$1\" == 'tools/generate_route_video.py' ]]; then\n"
            "  mkdir -p output/videos\n"
            "  if [[ \"$*\" == *'route_7_h.gif'* ]]; then\n"
            "    : > output/videos/route_7_h.gif\n"
            "  else\n"
            "    : > output/videos/route_7.gif\n"
            "  fi\n"
            "fi\n",
        )
        executable(
            directory / "solverExec",
            "#!/usr/bin/env bash\n"
            "printf 'solver %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p output/dist\n"
            ": > output/dist/out_7.dat\n",
        )
        executable(
            directory / "pathSortClusterExec",
            "#!/usr/bin/env bash\n"
            "printf 'cluster-sort %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p output/order\n"
            ": > output/order/route_segments_7.csv\n"
            ": > output/order/route_segments_7_h.csv\n",
        )

        environment = os.environ.copy()
        environment["PATH"] = f"{directory / 'bin'}:{environment['PATH']}"
        result = subprocess.run(
            [directory / RUNNER.name, "7", "10", "fixAndOptimize", "random"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        log = calls.read_text(encoding="utf-8").splitlines()
        check(log == [
            "make -s mip path-clusters",
            "python3 tools/generate_graph.py 7 --free --vehicles 1 --svg",
            "solver input/graph_7.dat input/graph_7.turns.dat fixAndOptimize random",
            "python3 clusterGeneration/generate_clusters.py output/dist/out_7.dat "
            "--percentage 10 --graph input/graph_7.dat "
            "--coords data/coords/graph_7.coords.csv",
            "cluster-sort input/graph_7.dat input/graph_7.turns.dat "
            "output/dist/out_7.dat data/clusters/clusters_7.dat",
            "python3 tools/generate_route_video.py --segments "
            "output/order/route_segments_7.csv --coords "
            "data/coords/graph_7.coords.csv --output output/videos/route_7.gif "
            "--cluster",
            "python3 tools/generate_route_video.py --segments "
            "output/order/route_segments_7_h.csv --coords "
            "data/coords/graph_7.coords.csv --output output/videos/route_7_h.gif "
            "--cluster",
        ], f"unexpected pipeline: {log}")
        check((directory / "data/clusters/clusters_7.dat").exists(),
              "cluster data was not generated")
        check((directory / "data/clusters/clusters_7.svg").exists(),
              "cluster SVG was not generated")
        check((directory / "data/clusters/clusters_7.png").exists(),
              "cluster PNG was not generated")
        check((directory / "output/order/route_segments_7.csv").exists(),
              "cluster path order was not generated")
        check((directory / "output/videos/route_7.gif").exists(),
              "cluster route GIF was not generated")
        check((directory / "output/videos/route_7_h.gif").exists(),
              "Hierholzer route GIF was not generated")

        solution = directory / "output/dist/out_7.dat"
        solution.write_text("OBJ: 42\n", encoding="utf-8")
        calls.write_text("", encoding="utf-8")
        cached = subprocess.run(
            [directory / RUNNER.name, "7", "10", "fixAndOptimize", "random"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(cached.returncode == 0, cached.stdout + cached.stderr)
        cached_log = calls.read_text(encoding="utf-8").splitlines()
        check(cached_log == [
            "make -s mip path-clusters",
            "python3 tools/generate_graph.py 7 --free --vehicles 1 --svg",
            "python3 clusterGeneration/generate_clusters.py output/dist/out_7.dat "
            "--percentage 10 --graph input/graph_7.dat "
            "--coords data/coords/graph_7.coords.csv",
            "cluster-sort input/graph_7.dat input/graph_7.turns.dat "
            "output/dist/out_7.dat data/clusters/clusters_7.dat",
            "python3 tools/generate_route_video.py --segments "
            "output/order/route_segments_7.csv --coords "
            "data/coords/graph_7.coords.csv --output output/videos/route_7.gif "
            "--cluster",
            "python3 tools/generate_route_video.py --segments "
            "output/order/route_segments_7_h.csv --coords "
            "data/coords/graph_7.coords.csv --output output/videos/route_7_h.gif "
            "--cluster",
        ], f"solver was not skipped for a cached solution: {cached_log}")
        check("se omite el solver" in cached.stdout,
              "cached solution did not report that the solver was skipped")
        check(solution.read_text(encoding="utf-8") == "OBJ: 42\n",
              "cached solution was overwritten")

        missing = subprocess.run(
            [directory / RUNNER.name], cwd=directory, env=environment,
            capture_output=True, text=True, timeout=10,
        )
        check(missing.returncode == 1 and "Uso:" in missing.stderr,
              "missing required arguments did not show usage")

        missing_percentage = subprocess.run(
            [directory / RUNNER.name, "7"], cwd=directory, env=environment,
            capture_output=True, text=True, timeout=10,
        )
        check(missing_percentage.returncode == 1 and "Uso:" in missing_percentage.stderr,
              "missing percentage did not show usage")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
