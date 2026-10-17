# RCPP-MIP

Solver del *Rural Chinese Postman Problem* (RCPP) implementado en C++17 con
programación entera mixta y CPLEX. El proyecto genera una red, calcula qué
tramos debe recorrer cada vehículo, ordena esas pasadas y puede visualizar la
ruta resultante.

## Flujo general

El proceso completo tiene cinco etapas:

1. **Generar o importar el grafo**, sus coordenadas y las restricciones de giro.
2. **Agrupar la red en clusters** y, opcionalmente, penalizar sus cruces.
3. **Resolver el RCPP** para obtener cuántas veces se recorre cada arco.
4. **Ordenar las pasadas** para formar un circuito continuo.
5. **Exportar y visualizar** el recorrido como CSV, GIF o HTML.

Los runners de la raíz automatizan estas etapas y guardan los artefactos de una
instancia de `N` nodos en `data/N/`.

## Requisitos y compilación

- Compilador compatible con C++17.
- IBM ILOG CPLEX.
- GLib y `pkg-config`.
- Python 3.8 o posterior para generadores, experimentos y visualizaciones.

Para compilar todo:

```sh
make
```

La configuración predeterminada busca CPLEX en
`/Applications/CPLEX_Studio2211` para `arm64_osx`. Para otra instalación:

```sh
make CPLEX_DIR=/ruta/CPLEX_Studio CPLEX_PLATFORM=<plataforma>
```

Los componentes también pueden compilarse por separado:

```sh
make mip                 # solverExec
make mip-best-cluster    # bestClusterPathExec
make path-common         # biblioteca compartida de ordenamiento
make path-first-pass     # pathSortExec
make path-clusters       # pathSortClusterExec
```

## Inicio rápido

### Resolver una instancia generada

El siguiente comando genera un grafo de 100 nodos, crea clusters, ajusta el
costo de sus cruces, resuelve el RCPP y ordena la solución por distancia:

```sh
./run_solver.sh 100 mip
```

Para ejecutar el flujo completo con clusters y generar el GIF:

```sh
./run_solver_cluster.sh 100 10 mip
```

El `10` es el porcentaje objetivo inicial de aristas distintas por cluster. Si
`data/100/min_dist_100.dat` ya existe y no está vacío, el runner reutiliza la
solución y omite el solver. `run_solver.sh` usa un 10 % por defecto, modificable
con la variable `CLUSTER_PERCENTAGE`.

### Resolver una instancia manualmente

```sh
./solverExec input.dat curvas.dat [estrategia]
```

Las estrategias son:

- `mip`: resuelve el modelo completo.
- `fixAndOptimize [maxDeadheadCost|random|topKDeadheadCost]`: aplica la
  heurística Fix-and-Optimize.

Si no se indica nada, se usa `fixAndOptimize topKDeadheadCost`. La vecindad BFS
y la cantidad de candidatos se ajustan durante la búsqueda.

La salida se guarda en `data/N/min_dist_N.dat`. El ejecutable termina con código
`0` si obtuvo una solución, `2` si no obtuvo ninguna y `1` ante un error. La
última línea, `RCPP_RESULT ...`, resume tiempo, objetivo y optimalidad.

### Minimizar cambios entre clusters

La variante `bestClusterPathExec` conserva las variables y restricciones del
RCPP, pero minimiza la cantidad de usos `Y` de las transiciones sintéticas
entre copias de clusters seleccionadas:

```sh
./bestClusterPathExec input.dat curvas.dat aristas.in-out.dat [salida.dat]
```

Si se omite la salida, se escribe `data/N/min_cluster_path_N.dat`. El objetivo
no incluye las variables de servicio `X`, las pasadas físicas, los giros ni los
conectores del depósito.

## Ordenar la solución

La salida del RCPP contiene multiplicidades, no un recorrido ordenado. Hay dos
formas de convertirla en un circuito.

### Ordenamiento por distancia

```sh
./pathSortExec input.dat curvas.dat data/N/min_dist_N.dat \
  [salida_orden.dat] [salida_segmentos.csv]
```

Si se omiten las salidas, se usan `data/N/order_N.dat` y
`data/N/route_segments_N.csv`.

### Ordenamiento por clusters

```sh
./pathSortClusterExec input.dat curvas.dat data/N/min_dist_N.dat \
  data/N/clusters_N.dat [salida_orden.dat] [salida_segmentos.csv] \
  [--lookahead N] [--branch-width N]
```

Esta variante minimiza las transiciones entre clusters. Primero construye un
circuito factible con Hierholzer guiado por cluster (profundidad `4` y ancho `8`
por defecto), lo exporta con sufijo `_h` y lo usa como punto inicial de CPLEX.
Puede variarse el horizonte sin recompilar; `--lookahead 1` usa una decisión
local.

Antes de optimizar, el modelo restringido valida y completa el circuito
inicial. Si CPLEX termina sin una solución disponible, se exporta ese circuito
ya validado como `order_N.dat` y `route_segments_N.csv`. En ese caso el orden es
factible pero no optimizado, y el código de salida sigue siendo `0`.

Para repetir solamente esta etapa y generar el GIF:

```sh
./run_sorter_cluster.sh 100
```

## Archivos principales

```text
data/N/
├── graph_N.dat                 # grafo original
├── graph_N.coords.csv          # coordenadas
├── graph_clusters_N.dat        # clusters previos al solver
├── graph_N.tuned.dat           # grafo expandido por cluster
├── graph_N.tuned.png           # clusters y elementos neutros expandidos
├── graph_N.tuned.mapping.dat   # nodos in/out expandidos → originales
├── graph_N.tuned.in-out.dat    # cambios in/out agrupados por cluster de origen
├── graph_N.tuned.turns.dat     # giros traducidos al grafo expandido
├── min_dist_N.dat              # multiplicidades resueltas por RCPP
├── clusters_N.dat              # cluster de cada pasada
├── order_N.dat                 # recorrido ordenado
├── route_segments_N.csv        # tramos reales en orden
└── route_N.gif                 # animación
```

`route_segments_N.csv` conserva pasadas repetidas, omite conectores virtuales y
usa las columnas `vehiculo,orden,nodo_origen,nodo_destino`. La variante por
clusters agrega la columna `cluster`. El depósito se representa con `D`.

## Formato de entrada

Los archivos usan valores separados por espacios o saltos de línea, sin
comentarios. Los nodos se numeran de `1` a `N`; el depósito se agrega
automáticamente.

### Grafo

```text
V N D E A
d1 ... dD
u v zona costo demanda
...
```

- `V`: vehículos; `N`: nodos.
- `D`: cantidad de nodos adyacentes al depósito, seguida por sus IDs.
- `E` y `A`: cantidad de aristas no dirigidas y arcos dirigidos. Las filas de
  aristas aparecen primero.
- `zona`: `0` para un tramo no requerido, `-1` para uno atendible por cualquier
  vehículo o `1..V` para uno asignado.
- `costo` debe ser positivo y `demanda` no negativa.

### Giros

```text
T P
u v w
...
```

Se escriben primero los `T` giros listados y luego los `P` prohibidos. Cada
triple representa `u → v → w`; su inverso es otro giro. Actualmente solo
los prohibidos restringen el recorrido y los retornos en U siempre se excluyen.
Para no definir giros, usar `0 0`.

## Generar grafos sintéticos

```sh
python3 tools/generate_graph.py 100 --seed 42 --svg
```

Opciones frecuentes:

- `--vehicles`: cantidad de vehículos (2 por defecto).
- `--free`: asigna las aristas interiores a la zona libre `-1`.
- `--demand-type fixed|integer|real`: tipo de demanda.
- `--demand`, `--demand-min`, `--demand-max`: valor fijo o rango.
- `--cost-min`, `--cost-max`: rango de costos.

El generador produce un grafo conexo, plano y no dirigido. El contorno queda en
la zona opcional `0`; las aristas interiores se reparten en zonas conexas, salvo
que se use `--free`.

Para generar instancias desde OpenStreetMap, consultar
[`generator/README.md`](generator/README.md).

## Clusters y ajuste de costos

Para agrupar la instancia antes de resolver:

```sh
python3 clusterGeneration/generate_clusters.py --graph-only \
  --graph data/100/graph_100.dat --nodes 100 --percentage 10 \
  --output data/100/graph_clusters_100.dat
```

El algoritmo construye regiones conexas con BFS. Luego fusiona clusters vecinos
cuando uno tiene al menos el doble de aristas que el otro, por lo que el tamaño
final puede superar el porcentaje inicial.

Después del solver y del re-mapeo al grafo original, el mismo script proyecta
la partición preexistente sobre las pasadas de la solución, sin volver a
ejecutar BFS:

```sh
python3 clusterGeneration/generate_clusters.py data/100/min_dist_100.dat \
  --graph-clusters data/100/graph_clusters_100.dat \
  --graph data/100/graph_100.dat \
  --coords data/100/graph_100.coords.csv
```

Las salidas son `clusters_N.dat`, `.svg` y `.png`. Se suman las pasadas de
servicio y adicionales (`X + Y`), se descartan las de multiplicidad cero y se
propaga a cada pasada y conector virtual el cluster de `graph_clusters_N.dat`.
El re-mapeo debe preceder al path sorter: ambos leen el grafo original y el
remapeador opera sobre multiplicidades, antes de que exista un orden de ruta.

Para expandir las zonas neutras y penalizar un cambio de cluster sin modificar
el original:

```sh
python3 tune_cluster_crossing.py data/100/graph_100.dat \
  data/100/graph_clusters_100.dat data/100/graph_100.tuned.dat \
  data/100/graph_100.tuned.mapping.dat \
  --coords data/100/graph_100.coords.csv \
  --turns data/100/graph_100.turns.dat \
  --expanded-turns data/100/graph_100.tuned.turns.dat
```

Un nodo incidente a aristas de más de un cluster se considera neutro. Cada
componente neutra se copia por cluster de acceso: recorrerla conserva los
costos físicos y pasar de la copia del cluster `i` a la del cluster `j` agrega
`S_i + S_j`, donde `S_k` es la suma de costos originales del cluster `k`.
Las aristas entre dos nodos neutros no pertenecen a ningún cluster y, por lo
tanto, no se incluyen en ninguna suma `S_k`.
Las aristas neutras requeridas conservan la obligación de servicio en una sola
copia; las demás copias sólo permiten recorrido.
Los giros se traducen a todas las copias de sus tres nodos para que las
prohibiciones originales sigan vigentes al resolver el grafo expandido.
La expansión también genera automáticamente `graph_N.tuned.png`. Con `--coords`
usa las coordenadas del mapa original y separa levemente las copias de cada nodo
neutro para hacerlas visibles. Las aristas de cada cluster usan su propio color,
los nodos neutros aparecen en naranja, las aristas neutras en gris discontinuo y
las transiciones sintéticas en naranja punteado. Sin `--coords` conserva el
layout automático.

`solverExec` guarda la solución bajo la cantidad de nodos del grafo expandido.
Para contraer las transiciones sintéticas y recuperar los IDs `in/out` del
grafo original:

```sh
python3 re-map-expanded-graph.py \
  data/M/min_dist_M.dat data/100/graph_100.tuned.mapping.dat \
  data/100/min_dist_100.dat --graph data/100/graph_100.dat \
  --turns data/100/graph_100.turns.dat
```

El grafo y los giros originales permiten reconstruir los conectores entre
pasadas sin introducir retornos en U ni giros prohibidos. Los runners realizan
este remapeo automáticamente antes de generar clusters, ordenar las pasadas o
visualizar el recorrido.

## Visualizaciones

Generar un GIF directamente:

```sh
python3 tools/generate_route_video.py \
  --segments data/100/route_segments_100.csv \
  --coords data/100/graph_100.coords.csv \
  --output data/100/route_100.gif
```

Generar un HTML autocontenido con controles de reproducción:

```sh
python3 tools/generate_route_html.py 100
```

Agregar `--cluster` para colorear por cluster. En instancias OSM, el script
detecta las coordenadas y puede recibir `--map-image` y `--bounds` para usar una
imagen geográfica de fondo.

Para inspeccionar aristas recorridas más de una vez:

```sh
python3 tools/plot_repeated_edges.py data/100/min_dist_100.dat \
  --graph data/100/graph_100.dat \
  --coords data/100/graph_100.coords.csv
```

## Arquitectura

- `cplexSolver/`: infraestructura común de Concert/CPLEX.
- `mipBestPathSolver/`: formulación y heurísticas del RCPP.
- `mipBestClusterPathSolver/`: variante RCPP que cuenta usos `Y` de
  transiciones sintéticas entre clusters.
- `mipPathSort/`: tipos, lectura, escritura y restricciones compartidas.
- `mipPathSort-FirstPass/`: ordenamiento con objetivo de distancia.
- `mipPathSort-Clusters/`: ordenamiento por clusters y warm start de
  Hierholzer.
- `clusterGeneration/`: construcción y proyección de clusters.
- `generator/`: importación geográfica desde OSM.
- `tools/`: generación sintética, experimentos y visualizaciones.

`PathSortInstanceReader` vincula la solución con la misma `Instance` usada por
el solver. Cada `PathEdge` conserva orientación y vehículo, y `times()` devuelve
servicio más recorridos adicionales. El ordenador expande esas multiplicidades,
asigna una posición a cada pasada y enlaza posiciones consecutivas.

## Experimentos y pruebas

Comparar estrategias de Fix-and-Optimize:

```sh
python3 tools/selection_strategy_experiments.py comparacion_estrategias
```

El runner admite `--sizes`, `--seed`, `--repetitions`, opciones de demanda y
vehículos, `--plot-only`, `--no-plots` y `--rerun`. Guarda resultados, gráficos
y logs en `experiments/<nombre>/`.

Pruebas principales:

```sh
make test-unit                        # no requiere CPLEX
make test                             # incluye integración con CPLEX
python3 tests/generate_graph_test.py  # generador sintético
```
