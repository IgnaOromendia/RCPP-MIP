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
            "mkdir -p output/order\n"
            ": > output/order/route_segments_7.csv\n",
        )
        executable(
            directory / "bin" / "python3",
            "#!/usr/bin/env bash\n"
            "printf 'python3 %s\\n' \"$*\" >> calls.log\n"
            "mkdir -p output/videos\n"
            ": > output/videos/route_7.gif\n",
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
            "cluster-sort input/graph_7.dat input/graph_7.turns.dat "
            "output/dist/out_7.dat data/clusters/clusters_7.dat",
            "python3 tools/generate_route_video.py --segments "
            "output/order/route_segments_7.csv --coords "
            "data/coords/graph_7.coords.csv --output output/videos/route_7.gif",
        ], f"unexpected pipeline: {log}")
        check((directory / "output/videos/route_7.gif").exists(),
              "cluster route GIF was not generated")

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
