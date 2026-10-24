#!/usr/bin/env python3
"""Compatibilidad: los experimentos de tiempo y objetivo ahora se ejecutan juntos."""

from distance_time_experiment import *  # noqa: F401,F403


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, OSError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
