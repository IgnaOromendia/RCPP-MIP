"""Rutas compartidas por el generador geográfico de RCPP-MIP."""

from dataclasses import dataclass
from pathlib import Path
import re


GENERATOR_DIR = Path(__file__).resolve().parent
REPOSITORY_ROOT = GENERATOR_DIR.parent
MAP_NAME_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]*$")


def validate_map_name(map_name):
    if not MAP_NAME_PATTERN.fullmatch(map_name):
        raise ValueError(
            "El nombre del mapa debe comenzar con una letra o un número y "
            "solo puede contener letras, números, '_' y '-'."
        )
    return map_name


def _inside(path, parent):
    try:
        path.relative_to(parent)
        return True
    except ValueError:
        return False


@dataclass(frozen=True)
class GeneratorPaths:
    """Entradas versionadas y salidas generadas para una raíz del proyecto."""

    repository_root: Path
    output_root: Path
    polygons_root: Path
    resources_root: Path

    @classmethod
    def for_repository(cls, repository_root=REPOSITORY_ROOT, data_root=None,
                       polygons_root=None, resources_root=None):
        repository_root = Path(repository_root).resolve()
        data_directory = (repository_root / "data").resolve()
        output_root = Path(data_root) if data_root is not None else data_directory / "generator"
        if not output_root.is_absolute():
            output_root = repository_root / output_root
        output_root = output_root.resolve()
        if not _inside(output_root, data_directory):
            raise ValueError(
                f"La salida del generador debe estar dentro de {data_directory}."
            )

        source_directory = repository_root / "generator"
        polygons_root = Path(polygons_root) if polygons_root else source_directory / "polygons"
        resources_root = Path(resources_root) if resources_root else source_directory / "resources"
        if not polygons_root.is_absolute():
            polygons_root = repository_root / polygons_root
        if not resources_root.is_absolute():
            resources_root = repository_root / resources_root
        return cls(
            repository_root=repository_root,
            output_root=output_root,
            polygons_root=polygons_root.resolve(),
            resources_root=resources_root.resolve(),
        )

    @property
    def input_dir(self):
        return self.output_root / "input"

    @property
    def coordinates_dir(self):
        return self.output_root / "coordinates"

    @property
    def mappings_dir(self):
        return self.output_root / "mappings"

    @property
    def distances_dir(self):
        return self.output_root / "distances"

    @property
    def attributes_dir(self):
        return self.output_root / "attributes"

    @property
    def osm_dir(self):
        return self.output_root / "osm"

    @property
    def plots_dir(self):
        return self.output_root / "plots"

    def graph(self, map_name):
        return self.input_dir / f"{validate_map_name(map_name)}.dat"

    def turns(self, map_name):
        return self.input_dir / f"{validate_map_name(map_name)}.turns.dat"

    def grid_graph(self, map_name):
        return self.input_dir / f"grid_{validate_map_name(map_name)}.dat"

    def zonification(self, map_name):
        return self.input_dir / f"grid_total_{validate_map_name(map_name)}.sal"

    def node_coordinates(self, map_name):
        return self.coordinates_dir / f"nodes_{validate_map_name(map_name)}.dat"

    def grid_coordinates(self, map_name):
        return self.coordinates_dir / f"grid_{validate_map_name(map_name)}.dat"

    def node_mapping(self, map_name):
        return self.mappings_dir / f"nodes_{validate_map_name(map_name)}.dat"

    def cell_mapping(self, map_name):
        return self.mappings_dir / f"cells_{validate_map_name(map_name)}.dat"

    def cell_distances(self, map_name):
        return self.distances_dir / f"cells_{validate_map_name(map_name)}.dat"

    def time_attributes(self, map_name):
        return self.attributes_dir / f"time_{validate_map_name(map_name)}.dat"

    def demand_attributes(self, map_name):
        return self.attributes_dir / f"demand_{validate_map_name(map_name)}.dat"

    def osm(self, map_name):
        return self.osm_dir / f"{validate_map_name(map_name)}.osm"

    def plot(self, map_name, suffix=""):
        return self.plots_dir / f"{validate_map_name(map_name)}{suffix}.html"

    def polygon(self, map_name):
        return self.polygons_root / f"{validate_map_name(map_name)}.geojson"

    def resource(self, filename):
        return self.resources_root / filename


def add_path_arguments(parser):
    parser.add_argument(
        "--data-root", type=Path,
        help="Raíz de salida dentro de data/ (predeterminado: data/generator).",
    )
    parser.add_argument(
        "--polygons-dir", type=Path,
        help="Directorio de GeoJSON (predeterminado: generator/polygons).",
    )
    parser.add_argument(
        "--resources-dir", type=Path,
        help="Directorio de KML (predeterminado: generator/resources).",
    )


def paths_from_arguments(arguments):
    return GeneratorPaths.for_repository(
        data_root=arguments.data_root,
        polygons_root=arguments.polygons_dir,
        resources_root=arguments.resources_dir,
    )
