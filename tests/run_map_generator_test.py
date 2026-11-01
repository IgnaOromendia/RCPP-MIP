#!/usr/bin/env python3
"""Check that the compatibility map runner uses the in-memory cluster solver."""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / "run_map_generator.sh"


def check(condition, message):
    if not condition:
        raise RuntimeError(message)


def main():
    with tempfile.TemporaryDirectory(prefix="run-map-generator-") as raw_directory:
        directory = Path(raw_directory)
        shutil.copy2(RUNNER, directory / RUNNER.name)
        common_runner = directory / "run_min_cluster_map.sh"
        common_runner.write_text(
            "#!/usr/bin/env bash\n"
            "printf '%s\\n' \"${RCPP_CLUSTER_STRATEGY:-}\" "
            "\"${RCPP_OUTPUT_VARIANT:-}\" \"$*\"\n",
            encoding="utf-8",
        )
        common_runner.chmod(0o755)

        environment = os.environ.copy()
        environment.pop("RCPP_CLUSTER_STRATEGY", None)
        environment.pop("RCPP_OUTPUT_VARIANT", None)
        result = subprocess.run(
            [directory / RUNNER.name, "sample", "10", "--resources-dir", "fixtures"],
            cwd=directory,
            env=environment,
            capture_output=True,
            text=True,
            timeout=10,
        )

        check(result.returncode == 0, result.stdout + result.stderr)
        check(
            result.stdout.splitlines() == [
                "",
                "",
                "sample 10 --resources-dir fixtures",
            ],
            f"run_map_generator.sh selected the wrong pipeline: {result.stdout!r}",
        )


if __name__ == "__main__":
    main()
