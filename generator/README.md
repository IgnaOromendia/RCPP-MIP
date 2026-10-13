# Generador geográfico de RCPP-MIP

La carpeta `generator/` convierte una zona y su red vial de OpenStreetMap (OSM)
en una instancia compatible con `solverExec`. Opcionalmente crea una grilla de
demanda, aplica una zonificación externa y genera mapas HTML para inspección.

Los GeoJSON y KML son entradas dentro de `generator/`; todos los resultados se
guardan bajo `data/generator/`.

## Flujo general

1. Leer los polígonos de la zona y descargar la red vial desde Overpass.
2. Limpiar y clasificar calles, calcular costos y detectar giros.
3. Asignar IDs consecutivos y conectar un depósito sintético.
4. Si corresponde, construir la grilla y sus distancias.
5. Validar y exportar los archivos para el solver.
6. Aplicar una zonificación externa y visualizarla.

## Requisitos

Desde la raíz del repositorio:

```sh
python3 -m pip install -r generator/requirements.txt
```

Se necesita Python 3.8 o posterior. La descarga inicial requiere Internet para
consultar Overpass; `folium` solo es necesario para los mapas HTML.

## Entradas

Para una instancia llamada `<mapa>` se esperan:

```text
generator/polygons/<mapa>.geojson
generator/polygons/<mapa>Total.geojson
generator/resources/empedrados.kml
generator/resources/cruces-ffc.kml
generator/resources/Semaforos.kml
generator/resources/Semaforos-giro-izq.kml
generator/resources/Barrios-Informales.kml
generator/resources/Barrios-cerrados.kml
```

- `<mapa>Total>.geojson` delimita la consulta a Overpass.
- `<mapa>.geojson` delimita las calles requeridas.
- Cada GeoJSON debe ser un `FeatureCollection` cuya primera geometría sea la
  zona.
- Los KML no están incluidos en el repositorio. Puede indicarse otra ubicación
  con `--resources-dir <directorio>`.

## Uso rápido

### Solo red vial

Para crear la instancia sin grilla ni zonificación:

```sh
python3 generator/mapToOsmGraph.py acassusoA --sin-grilla
```

Las calles dentro de `acassusoA.geojson` se exportan como requeridas (`-1`) y
las demás como opcionales (`0`). No se generan archivos de celdas ni matrices
de distancia.

### Red vial y grilla

```sh
python3 generator/mapToOsmGraph.py \
  acassusoA 100 "Avenida del Libertador"
```

Los argumentos son:

1. nombre de la instancia y prefijo de sus GeoJSON;
2. cantidad máxima de celdas;
3. nombre OSM exacto de una calle usada para orientar la grilla.

La calle debe existir y definir una orientación válida. Los argumentos 2 y 3
también se aceptan con `--sin-grilla`, pero se ignoran.

Opciones de rutas:

- `--data-root`: raíz de salida dentro de `data/`; por defecto,
  `data/generator/`.
- `--polygons-dir`: ubicación alternativa de los GeoJSON.
- `--resources-dir`: ubicación alternativa de los KML.

## Runner completo

El runner genera o reutiliza la red, resuelve el RCPP, agrupa las pasadas por
cluster, ordena la ruta y crea el GIF:

```sh
./run_map_generator.sh acassusoA 10 \
  --resources-dir /ruta/a/los/kml
```

El `10` es el porcentaje objetivo inicial de aristas distintas por cluster. El
runner sigue esta lógica:

1. Si ya existen grafo, giros, coordenadas y mapeo de nodos, reutiliza el
   conjunto; si falta uno, vuelve a generarlo completo.
2. Si la solución RCPP existe y no está vacía, omite el solver.
3. Ejecuta el ordenador por clusters.
4. Genera el GIF optimizado cuando hay una solución disponible.
5. Aunque el ordenador no pueda exportar un orden final, conserva el GIF `_h`
   del circuito inicial de Hierholzer.

## Salidas

```text
data/generator/
├── input/
│   ├── <mapa>.dat
│   ├── <mapa>.turns.dat
│   ├── grid_<mapa>.dat           # solo con grilla
│   └── grid_total_<mapa>.sal     # zonificación externa
├── coordinates/
│   ├── nodes_<mapa>.dat
│   └── grid_<mapa>.dat           # solo con grilla
├── mappings/
│   ├── nodes_<mapa>.dat
│   └── cells_<mapa>.dat          # solo con grilla
├── distances/cells_<mapa>.dat        # solo con grilla
├── attributes/
│   ├── time_<mapa>.dat
│   └── demand_<mapa>.dat
├── osm/<mapa>.osm
└── plots/
    ├── <mapa>.html
    ├── <mapa>_lines.html
    └── <mapa>_turns.html
```

`grid_total_<mapa>.sal` no lo crea el generador: es la salida de un modelo de
zonificación y debe copiarse a esa ruta.

## Contrato con el solver

`input/<mapa>.dat` usa el formato:

```text
vehiculos nodos adyacentes_deposito aristas arcos
d1 ... dD
origen destino zona costo demanda
...
```

Los nodos se numeran de `1` a `N`; el `0` interno de Python no se exporta.
Primero aparecen las aristas no dirigidas y luego los arcos dirigidos. Las zonas
significan:

- `0`: tramo no requerido;
- `-1`: tramo requerido atendible por cualquier vehículo;
- `1..V`: tramo asignado a ese vehículo.

El archivo `<mapa>.turns.dat` comienza con la cantidad de giros listados y
prohibidos, seguida por los triples de cada grupo. El solver actual solo usa los
prohibidos para excluir conexiones.

La instancia puede resolverse directamente:

```sh
./solverExec \
  data/generator/input/acassusoA.dat \
  data/generator/input/acassusoA.turns.dat
```

## Aplicar una zonificación

El archivo `grid_total_<mapa>.sal` debe incluir exactamente una fila por celda:

```text
cantidad_de_celdas
id_celda zona_base_cero
...
```

Para aplicarlo:

```sh
python3 generator/zoneMap.py acassusoA
```

El comando asigna las celdas al único vehículo y reescribe la instancia de
forma atómica. Un tramo requerido con ambos extremos asignados pasa a la zona
`1`; si no, queda en `-1`. Los tramos opcionales permanecen en `0`.

Para inspeccionar el resultado:

```sh
python3 generator/plot_zone.py acassusoA 1
```

El último argumento vale `1` para mostrar la grilla y `0` para ocultarla. Los
mapas se escriben en `data/generator/plots/`.

## Lógica interna

- `Fetcher` convierte el polígono total al formato de Overpass y descarga las
  clases viales admitidas con un `User-Agent` identificable.
- `OsmGraph` filtra calles y áreas restringidas, clasifica aristas y arcos,
  elimina terminales y calcula tiempo y demanda.
- Los nodos reciben IDs consecutivos; el `0` queda reservado internamente.
- Se eligen nodos cercanos al depósito sintético y se detectan giros.
- `Grid` rota y divide la envolvente, elimina celdas sin demanda y calcula
  adyacencias y distancias.
- `Exporter` valida el contrato y escribe los archivos atómicamente.

La configuración actual usa un vehículo, velocidad de `5.55 m/s`, dos bolsas de
demanda cada 100 metros y el depósito `(-34.5196029, -58.5550397)`. Los cruces
ferroviarios multiplican el tiempo por `1.5`; el empedrado reduce la velocidad
por un factor de `0.85`.

## Limitaciones

- Algunas excepciones viales manuales solo cubren zonas de San Isidro.
- La detección de avenidas depende de que el nombre OSM contenga `Avenida`.
- Los puntos KML se asocian a nodos OSM con una tolerancia fija de `1e-4`
  grados.
- La limpieza de terminales itera hasta alcanzar un punto fijo y falla
  explícitamente si detecta terminales pero no puede eliminarlos.
- Overpass puede devolver datos diferentes con el tiempo; conservar las
  salidas cuando se necesite reproducibilidad exacta.

## Pruebas

La regresión específica usa datos locales y un directorio temporal:

```sh
python3 tests/osm_generator_test.py --reader build/instance_reader_test
```

También forma parte de la suite unitaria:

```sh
make test-unit
```
