#!/usr/bin/env python3
"""Check the clustered sorter-to-GIF shell pipeline."""

from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / "run_sorter_cluster.sh"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def executable(path: Path, contents: str):
    path.write_text(contents, encoding="utf-8")
    path.chmod(0o755)


def main():
    with tempfile.TemporaryDirectory(prefix="run-sorter-cluster-") as raw_directory:
        directory = Path(raw_directory)
        shutil.copy2(RUNNER, directory / RUNNER.name)
        (directory / "tools").mkdir()
        (directory / "bin").mkdir()

        executable(
            directory / "bin" / "make",
            "#!/usr/bin/env bash\n"
            "printf 'make %s\\n' \"$*\" >> calls.log\n",
        )
        executable(
            directory / "pathSortClusterExec",
            "#!/usr/bin/env bash\n"
            "printf 'cluster-sort %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p data/7\n"
            ": > data/7/route_segments_7.csv\n"
            ": > data/7/route_segments_7_h.csv\n",
        )
        executable(
            directory / "bin" / "python3",
            "#!/usr/bin/env bash\n"
            "printf 'python3 %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p data/7\n"
            "if [[ \"$*\" == *'route_7_h.gif'* ]]; then\n"
            "  : > data/7/route_7_h.gif\n"
            "else\n"
            "  : > data/7/route_7.gif\n"
            "fi\n",
        )

        environment = os.environ.copy()
        environment["PATH"] = f"{directory / 'bin'}:{environment['PATH']}"
        result = subprocess.run(
            [directory / RUNNER.name, "7"], cwd=directory, env=environment,
            capture_output=True, text=True, timeout=10,
        )
        check(result.returncode == 0, result.stdout + result.stderr)
        log = (directory / "calls.log").read_text(encoding="utf-8").splitlines()
        check(log == [
            "make -s path-clusters",
            "cluster-sort data/7/graph_7.tuned.dat data/7/graph_7.turns.dat "
            "data/7/min_dist_7.dat data/7/clusters_7.dat",
            "python3 tools/generate_route_video.py --segments "
            "data/7/route_segments_7.csv --coords "
            "data/7/graph_7.coords.csv --output data/7/route_7.gif "
            "--cluster",
            "python3 tools/generate_route_video.py --segments "
            "data/7/route_segments_7_h.csv --coords "
            "data/7/graph_7.coords.csv --output data/7/route_7_h.gif "
            "--cluster",
        ], f"unexpected pipeline: {log}")
        check((directory / "data/7/route_7.gif").exists(),
              "cluster route GIF was not generated")
        check((directory / "data/7/route_7_h.gif").exists(),
              "Hierholzer route GIF was not generated")

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
