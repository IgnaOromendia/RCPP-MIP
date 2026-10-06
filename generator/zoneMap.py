"""CLI para aplicar al grafo el resultado del modelo de zonificación."""

import argparse
import sys

from generator import Generator
from paths import add_path_arguments, paths_from_arguments, validate_map_name


def zoneMap(mapName, paths=None):
    return Generator(paths=paths).applyZonificationTo(mapName)


def parse_arguments(arguments=None):
    parser = argparse.ArgumentParser(
        description="Aplica grid_total_<mapa>.sal a una instancia vial."
    )
    parser.add_argument("mapa", help="Nombre de la instancia generada.")
    add_path_arguments(parser)
    options = parser.parse_args(arguments)
    try:
        validate_map_name(options.mapa)
    except ValueError as error:
        parser.error(str(error))
    return options


def main(arguments=None):
    options = parse_arguments(arguments)
    try:
        output = zoneMap(options.mapa, paths_from_arguments(options))
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    print(f"Instancia zonificada: {output}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
