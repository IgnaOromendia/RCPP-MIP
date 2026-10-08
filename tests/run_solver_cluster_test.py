#!/usr/bin/env python3
"""Check that run_solver_cluster stops after solver and cluster generation."""

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
            "  read -r percentage\n"
            "  printf 'k=%s\\n' \"$percentage\" >> calls.log\n"
            "  mkdir -p data/clusters\n"
            "  : > data/clusters/clusters_7.dat\n"
            "  : > data/clusters/clusters_7.svg\n"
            "  : > data/clusters/clusters_7.png\n"
            "fi\n",
        )
        executable(
            directory / "solverExec",
            "#!/usr/bin/env bash\n"
            "printf 'solver %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p output/dist\n"
            ": > output/dist/out_7.dat\n",
        )
        # A trap executable: the runner must never invoke path ordering.
        executable(
            directory / "pathSortExec",
            "#!/usr/bin/env bash\n"
            "printf 'path-sort-called\\n' >> calls.log\n"
            "exit 99\n",
        )

        environment = os.environ.copy()
        environment["PATH"] = f"{directory / 'bin'}:{environment['PATH']}"
        result = subprocess.run(
            [directory / RUNNER.name, "7", "fixAndOptimize", "random"],
            input="10\n",
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        log = calls.read_text(encoding="utf-8").splitlines()
        check(log == [
            "make -s mip",
            "python3 tools/generate_graph.py 7 --free --vehicles 1 --svg",
            "solver input/graph_7.dat input/graph_7.turns.dat fixAndOptimize random",
            "python3 clusterGeneration/generate_clusters.py output/dist/out_7.dat "
            "--graph input/graph_7.dat --coords data/coords/graph_7.coords.csv",
            "k=10",
        ], f"unexpected pipeline: {log}")
        check((directory / "data/clusters/clusters_7.dat").exists(),
              "cluster data was not generated")
        check((directory / "data/clusters/clusters_7.svg").exists(),
              "cluster SVG was not generated")
        check((directory / "data/clusters/clusters_7.png").exists(),
              "cluster PNG was not generated")
        check(not (directory / "output/order").exists(), "path-order output was generated")
        check("path-sort-called" not in log, "pathSortExec was invoked")

        missing = subprocess.run(
            [directory / RUNNER.name], cwd=directory, env=environment,
            capture_output=True, text=True, timeout=10,
        )
        check(missing.returncode == 1 and "Uso:" in missing.stderr,
              "missing node count did not show usage")


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, subprocess.TimeoutExpired) as error:
        print(error, file=sys.stderr)
        raise SystemExit(1)
