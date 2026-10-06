# Generador geográfico de RCPP-MIP

La carpeta `generator/` transforma una zona geográfica y la red vial de
OpenStreetMap (OSM) en una instancia que puede leer `solverExec`. También genera
una grilla de demanda, aplica el resultado de un modelo de zonificación y produce
mapas HTML para inspección.

Todos los artefactos generados se guardan bajo `data/generator/`. Los GeoJSON y
KML son recursos de entrada y permanecen dentro de `generator/`.

## Requisitos

Usar Python 3.8 o posterior e instalar las dependencias desde la raíz:

```sh
python3 -m pip install -r generator/requirements.txt
```

`folium` solamente se necesita para generar los mapas HTML. La generación
inicial consulta `https://overpass-api.de/api/interpreter` y, por lo tanto,
requiere acceso a Internet.

## Recursos de entrada

Para una instancia llamada `<mapa>` se necesitan:

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

Los GeoJSON deben ser `FeatureCollection` y su primer `feature` debe contener
la geometría de la zona. `<mapa>Total>.geojson` delimita la consulta a Overpass;
`<mapa>.geojson` delimita las calles requeridas.

Los KML no están incluidos actualmente en el repositorio. Si se encuentran en
otra ubicación, se puede indicar `--resources-dir <directorio>`.

## Generar el grafo y la grilla

Ejecutar desde la raíz del repositorio:

```sh
python3 generator/mapToOsmGraph.py \
  acassusoA 100 "Avenida del Libertador"
```

Los argumentos posicionales son:

1. nombre de la instancia y prefijo de los GeoJSON;
2. cantidad máxima de celdas de la grilla;
3. nombre OSM exacto de una calle cuya orientación se usa para rotar la grilla.

La calle debe existir en el grafo descargado. El comando falla con un mensaje
explícito si no puede encontrarla o si no define una orientación.

### Generar sin grilla ni zonificación

Para generar solamente la instancia vial:

```sh
python3 generator/mapToOsmGraph.py acassusoA --sin-grilla
```

También hay un atajo desde la raíz del proyecto:

```sh
./run_map_generator.sh acassusoA
```

El primer argumento es el prefijo de `<mapa>.geojson` y
`<mapa>Total.geojson`. Las opciones adicionales se reenvían al generador, por
ejemplo:

```sh
./run_map_generator.sh acassusoA \
  --resources-dir /ruta/a/los/kml
```

Este modo no necesita una cantidad de celdas ni una calle de referencia. Las
calles requeridas, determinadas por `generator/polygons/acassusoA.geojson`, se
exportan con zona `-1`; las no requeridas se exportan con zona `0`. No se crean
`grid_<mapa>.dat`, coordenadas de celdas, mapeos nodo-celda ni matrices de
distancia entre celdas.

Por compatibilidad también se aceptan los argumentos de celdas y calle junto a
`--sin-grilla`, pero se ignoran:

```sh
python3 generator/mapToOsmGraph.py \
  acassusoA 100 "Avenida del Libertador" --sin-grilla
```

Opciones de rutas:

```text
--data-root       raíz de salida, siempre dentro de data/
--polygons-dir    ubicación alternativa de los GeoJSON
--resources-dir   ubicación alternativa de los KML
```

El valor predeterminado de `--data-root` es `data/generator/`. El generador crea
automáticamente los directorios de salida que necesita.

## Salidas

```text
data/generator/
├── input/
│   ├── <mapa>.dat
│   ├── <mapa>.turns.dat
│   ├── grid_<mapa>.dat                 # solo con grilla
│   └── grid_total_<mapa>.sal       # resultado externo de zonificación
├── coordinates/
│   ├── nodes_<mapa>.dat
│   └── grid_<mapa>.dat                # solo con grilla
├── mappings/
│   ├── nodes_<mapa>.dat
│   └── cells_<mapa>.dat               # solo con grilla
├── distances/
│   └── cells_<mapa>.dat               # solo con grilla
├── attributes/
│   ├── time_<mapa>.dat
│   └── demand_<mapa>.dat
├── osm/
│   └── <mapa>.osm
└── plots/
    ├── <mapa>.html
    ├── <mapa>_lines.html
    └── <mapa>_turns.html
```

`grid_total_<mapa>.sal` no lo produce este generador: debe copiarse a esa ruta
después de ejecutar el modelo de zonificación.

## Formato compatible con el solver

`input/<mapa>.dat` usa el contrato de `InstanceReader`:

```text
vehiculos nodos adyacentes_deposito aristas arcos
d1 ... dD
origen destino zona costo demanda
...
```

Los nodos del archivo se numeran de `1` a `N`. El índice `0` reservado por las
estructuras Python no se exporta ni se suma a `N`. Primero aparecen las aristas
no dirigidas y luego los arcos dirigidos.

Las zonas tienen estos significados:

- `0`: tramo no requerido;
- `-1`: tramo requerido que puede atender cualquier vehículo;
- `1..V`: tramo requerido asignado al vehículo correspondiente.

`input/<mapa>.turns.dat` comienza con la cantidad de giros listados y de giros
prohibidos. Luego contiene los triples de la primera lista y finalmente los
prohibidos. El solver actual solamente usa la segunda lista para excluir
conexiones; la primera no es una lista exhaustiva de movimientos permitidos.

La instancia puede ejecutarse directamente con:

```sh
./solverExec \
  data/generator/input/acassusoA.dat \
  data/generator/input/acassusoA.turns.dat
```

## Aplicar una zonificación

El archivo `data/generator/input/grid_total_<mapa>.sal` debe tener este formato:

```text
cantidad_de_celdas
id_celda zona_base_cero
...
```

Debe incluir exactamente una fila para cada celda. Para aplicarlo:

```sh
python3 generator/zoneMap.py acassusoA
```

El comando relee la instancia, asigna las celdas al único vehículo y reescribe
`input/acassusoA.dat` de forma atómica. Un tramo requerido cuyos extremos están
asignados recibe la zona `1`; en otro caso recibe `-1`. Los tramos no requeridos
permanecen en `0`.

## Visualizar la zonificación

```sh
python3 generator/plot_zone.py acassusoA 1
```

El último argumento vale `1` para mostrar la grilla y `0` para ocultarla. Los
tres mapas HTML se escriben en `data/generator/plots/`.

## Funcionamiento interno

1. `Fetcher` convierte el polígono total al formato de Overpass y descarga por
   POST solamente las clases viales que procesa el generador, junto con sus
   nodos. La solicitud usa un `User-Agent` identificable para respetar las
   políticas de las instancias públicas.
2. `OsmGraph` filtra las clases viales admitidas, excluye áreas restringidas,
   clasifica aristas y arcos, elimina terminales y calcula tiempo y demanda.
3. Los nodos se reasignan a IDs internos consecutivos; `0` queda reservado.
4. Se seleccionan nodos cercanos al depósito sintético y se detectan giros.
5. `Grid` rota y divide la envolvente del grafo, elimina celdas sin demanda y
   calcula adyacencias y distancias.
6. `Exporter` valida el contrato y escribe atómicamente los resultados.

La configuración actual usa un solo vehículo, velocidad de `5.55 m/s`, demanda de
dos bolsas cada 100 metros y el depósito `(-34.5196029, -58.5550397)`. Un cruce
ferroviario multiplica el tiempo por `1.5`; un tramo empedrado reduce la
velocidad por un factor de `0.85`.

## Limitaciones conocidas

- Las excepciones viales codificadas manualmente solo existen para algunas
  zonas de San Isidro. Los demás mapas usan las reglas generales.
- La detección de avenidas depende de que el nombre OSM contenga `Avenida`.
- La asociación de puntos KML con nodos OSM usa una tolerancia fija de `1e-4`
  grados.
- La limpieza de terminales se detiene después de 100 iteraciones.
- La respuesta de Overpass puede cambiar con el tiempo; conservar los archivos
  generados cuando se necesite reproducibilidad exacta.

## Pruebas

La regresión específica no consulta Internet y usa un directorio temporal:

```sh
python3 tests/osm_generator_test.py --reader build/instance_reader_test
```

La suite unitaria del proyecto también la ejecuta:

```sh
make test-unit
```

La prueba verifica que las salidas permanezcan bajo `data/`, el conteo de nodos,
la lectura por `InstanceReader`, los giros y el ciclo exportar-importar-zonificar.
