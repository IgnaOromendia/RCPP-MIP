#!/usr/bin/env python3
"""Check the explicit BFS and street-divider map runner entry points."""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parent.parent
RUNNERS = {
    "run_map_bfs_generator.sh": ("bfs", "bfs"),
    "run_map_streets_generator.sh": ("streets", "streets"),
}


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    with tempfile.TemporaryDirectory(prefix="run-map-cluster-variants-") as raw_directory:
        directory = Path(raw_directory)
        for runner_name in RUNNERS:
            shutil.copy2(ROOT / runner_name, directory / runner_name)
        common_runner = directory / "run_min_cluster_map.sh"
        common_runner.write_text(
            "#!/usr/bin/env bash\n"
            "printf '%s\\n' \"${RCPP_CLUSTER_STRATEGY:-}\" "
            "\"${RCPP_OUTPUT_VARIANT:-}\" \"$*\"\n",
            encoding="utf-8",
        )
        common_runner.chmod(0o755)

        environment = os.environ.copy()
        for runner_name, (strategy, variant) in RUNNERS.items():
            result = subprocess.run(
                [directory / runner_name, "sample", "10", "--resources-dir", "fixtures"],
                cwd=directory,
                env=environment,
                capture_output=True,
                text=True,
                timeout=10,
            )
            check(result.returncode == 0, result.stdout + result.stderr)
            check(
                result.stdout.splitlines() == [
                    strategy,
                    variant,
                    "sample 10 --resources-dir fixtures",
                ],
                f"{runner_name} selected the wrong pipeline: {result.stdout!r}",
            )


if __name__ == "__main__":
    main()
