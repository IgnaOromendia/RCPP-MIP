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
    parser.add_argument(
        "--overpass-timeout", type=float, default=90,
        help="Timeout HTTP de Overpass en segundos (predeterminado: 90).",
    )
    parser.add_argument(
        "--overpass-url", action="append", dest="overpass_urls",
        help=("Endpoint alternativo de Overpass; puede repetirse. Si se omite, "
              "se rotan las instancias publicas configuradas."),
    )
    parser.add_argument(
        "--overpass-retries", type=int, default=2,
        help="Cantidad de vueltas por los endpoints de Overpass (predeterminado: 2).",
    )
    parser.add_argument(
        "--overpass-retry-delay", type=float, default=2,
        help="Espera base entre intentos, en segundos (predeterminado: 2).",
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
        if options.overpass_timeout <= 0:
            raise ValueError("--overpass-timeout debe ser positivo.")
        if options.overpass_retries < 1:
            raise ValueError("--overpass-retries debe ser positivo.")
        if options.overpass_retry_delay < 0:
            raise ValueError("--overpass-retry-delay no puede ser negativo.")
        if options.overpass_urls is not None and any(
                not endpoint.strip() for endpoint in options.overpass_urls):
            raise ValueError("--overpass-url no puede estar vacio.")
    except ValueError as error:
        parser.error(str(error))
    return options


def main(arguments=None):
    options = parse_arguments(arguments)
    try:
        paths = paths_from_arguments(options)
        importer = Importer(paths=paths)
        fetcher = Fetcher(
            importer=importer,
            timeout=options.overpass_timeout,
            endpoints=options.overpass_urls,
            retries=options.overpass_retries,
            retry_delay=options.overpass_retry_delay,
        )
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
