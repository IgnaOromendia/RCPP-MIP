"""CLI para generar una instancia vial a partir de GeoJSON y Overpass."""

import argparse
import sys

from fetcher import Fetcher
from generator import Generator
from importer import Importer
from paths import add_path_arguments, paths_from_arguments, validate_map_name


def parse_arguments(arguments=None):
    parser = argparse.ArgumentParser(
        description="Genera un grafo vial para RCPP-MIP, con grilla opcional."
    )
    parser.add_argument("mapa", help="Prefijo de <mapa>.geojson y <mapa>Total.geojson.")
    parser.add_argument(
        "celdas", type=int, nargs="?",
        help="Cantidad máxima de celdas; requerida salvo con --sin-grilla.",
    )
    parser.add_argument(
        "calle", nargs="?",
        help="Calle de referencia; requerida salvo con --sin-grilla.",
    )
    parser.add_argument(
        "--sin-grilla", action="store_true",
        help=("Genera solo el grafo: requeridos en zona -1 y no requeridos "
              "en zona 0."),
    )
    add_path_arguments(parser)
    options = parser.parse_args(arguments)
    try:
        validate_map_name(options.mapa)
        if not options.sin_grilla:
            if options.celdas is None or not options.calle:
                raise ValueError(
                    "Sin --sin-grilla se deben indicar <celdas> y <calle>."
                )
            if options.celdas < 1:
                raise ValueError("La cantidad de celdas debe ser positiva.")
    except ValueError as error:
        parser.error(str(error))
    return options


def main(arguments=None):
    options = parse_arguments(arguments)
    try:
        paths = paths_from_arguments(options)
        importer = Importer(paths=paths)
        fetcher = Fetcher(importer=importer)
        generator = Generator(options.celdas or 0, paths=paths)

        total_zone = fetcher.fetchZoneFrom(options.mapa + "Total")
        requested_zone = importer.importGeoJSON(options.mapa)
        generator.generateFrom2GeoJSON(
            total_zone, requested_zone, options.calle, options.mapa,
            generateGrid=not options.sin_grilla,
        )
        written = generator.export(options.mapa)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1

    print("Archivos generados:")
    for path in written:
        print(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
