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

`bestClusterPathExec` primero minimiza la cantidad de usos `Y` de las
transiciones sintéticas. Luego extrae el circuito de Euler global, conserva
cada pareja entrada/salida como una visita y optimiza la distancia interna de
cada cluster con un vehículo local por visita. Al fusionar, todos esos
vehículos lógicos vuelven al único vehículo global:

```sh
./bestClusterPathExec input.dat curvas.dat aristas.in-out.dat \
  arc-clusters.dat [salida.dat]
```

Si se omite la salida, se escribe `data/N/min_cluster_path_N.dat`. El `OBJ` del
archivo final es la distancia resultante; el ejecutable informa por separado
la cantidad óptima de cambios entre clusters. Para cada subproblema resuelto,
la línea `Cluster ...` también informa `tiempo_ms`, medido desde la creación del
solver local hasta que CPLEX termina la resolución del cluster.

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
├── graph_N.tuned.mapping.dat   # nodos in/out expandidos → originales
├── graph_N.tuned.in-out.dat    # origen/destino de cambios entre clusters
├── graph_N.tuned.arc-clusters.dat # cluster de cada arco expandido
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
  --coords data/100/graph_100.coords.csv \
  --output data/100/graph_clusters_100.dat
```

Para mapas OSM, las avenidas y calles primarias forman las fronteras neutras
iniciales. Dentro del cluster más grande, la estrategia de caminos divisores
elige primero su calle neutra limítrofe más larga y busca cerca del centro una
calle con nombre que llegue hasta el límite opuesto. Si existe, agrega ese
tramo como una nueva frontera y vuelve a clusterizar:

```sh
python3 clusterGeneration/generate_clusters.py --graph-only \
  --graph data/<mapa>/graph_N.dat --nodes N --percentage 10 \
  --coords data/<mapa>/graph_N.coords.csv \
  --neutral-nodes data/<mapa>/graph_N.neutral-nodes.dat \
  --neutral-nodes-output data/<mapa>/graph_N.cluster-neutral-nodes.dat \
  --osm data/generator/osm/<mapa>.osm \
  --node-mapping data/generator/mappings/nodes_<mapa>.dat \
  --output data/<mapa>/graph_clusters_N.dat
```

El `.dat` conserva la asignación que consume el solver. Como única
visualización, los runners OSM generan `graph_clusters_N.html`, un mapa
OpenStreetMap interactivo con zoom, tooltips, leyenda y capas para mostrar u
ocultar clusters, fronteras originales, calles divisorias y nodos neutros. Ya
no se generan los previews estáticos SVG y PNG de los clusters.

La salida de nodos neutros identifica cada camino generado con una razón
`divider_N`. De esta forma, sólo los tramos cuyos extremos pertenecen al mismo
camino se consideran divisores; una calle transversal entre dos caminos
cercanos no se convierte accidentalmente en una frontera.

Los clusters se forman sólo con las aristas obligatorias (`zona != 0`) y el
HTML oculta las calles opcionales (`zona 0`). Para conservar el contrato del
tuner, esas calles opcionales siguen presentes en `graph_clusters_N.dat`, pero
heredan después el cluster obligatorio más cercano y no influyen en la
partición.

La calle divisoria conserva su nombre en todo el recorrido y no atraviesa una
tercera frontera. Si el cluster más grande no tiene una calle así, el algoritmo
termina. Ésta es la estrategia predeterminada `--strategy streets`.
Para una partición BFS reproducible, usar `--strategy bfs`: cada cluster
crece por aristas requeridas adyacentes hasta el porcentaje indicado, y las
aristas opcionales heredan después el cluster requerido más cercano.

Después del solver y del re-mapeo al grafo original, el mismo script proyecta
la partición preexistente sobre las pasadas de la solución, sin volver a
ejecutar el algoritmo:

```sh
python3 clusterGeneration/generate_clusters.py data/100/min_dist_100.dat \
  --graph-clusters data/100/graph_clusters_100.dat \
  --graph data/100/graph_100.dat \
  --coords data/100/graph_100.coords.csv
```

La salida es `clusters_N.dat`. Se suman las pasadas de servicio y adicionales
(`X + Y`), se descartan las de multiplicidad cero y se propaga a cada pasada y
conector virtual el cluster de `graph_clusters_N.dat`.
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
El tuner también exporta el cluster de origen y destino de cada transición y
el archivo `graph_N.tuned.arc-clusters.dat`, consumidos por la optimización
interna de clusters.
La expansión no genera previews estáticos SVG o PNG. Los runners OSM conservan
como visualización únicamente `graph_clusters_N.html`, el mapa interactivo de
la partición previa al solver.

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
pasadas sin introducir retornos en U ni giros prohibidos. Durante esa
reconstrucción se cancelan idas y vueltas muertas redundantes y, cuando una
transición sintética ocultó un giro imposible, se agrega el circuito físico
legal de menor costo que separa las pasadas. Los runners realizan este remapeo
automáticamente antes de generar clusters, ordenar las pasadas o visualizar el
recorrido.

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
imagen geográfica de fondo. Los runners también pasan `--graph-clusters` para
que los nodos y las aristas neutras se detecten sobre el grafo completo, aunque
algún cluster incidente no aparezca en el recorrido ordenado.

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

Ejecutar únicamente el experimento 1 del [plan](plan-experimentacion.md):

```sh
make mip mip-best-cluster
.venv/bin/pip install matplotlib
.venv/bin/python tools/distance_time_experiment.py
```

Compara MIP, F&O con `random`, `maxDeadheadCost` y `topKDeadheadCost`, y
bestClusterPath sobre `acassusoA`, `acassusoB`, `martinezA` y `martinezB`.
Usa una repetición y clusters del 20% de las aristas requeridas, con particiones
BFS y Streets independientes. Reutiliza los datos OSM de `data/generator/`;
si faltan, ejecuta el generador sin grilla (requiere Internet y sus recursos).
Todas las entradas pasan por `tune_cluster_crossing.py`. No ejecuta sorters.

Guarda cada corrida apenas termina en `experiments/experimento_1/resultados.csv`,
sin esperar a que termine el experimento completo. Guarda las instancias y los logs
en la misma carpeta, junto
con `experimento_1_tiempos_bfs.png` y `experimento_1_tiempos_streets.png`.
El eje X ordena los mapas por cantidad de aristas originales (`E + A`); el Y
muestra el tiempo total informado por cada ejecutable, incluido el refinamiento
interno de bestClusterPath, sin generación ni clustering. Los tiempos de corridas
sin solución se incluyen; los errores quedan en el CSV y se omiten del gráfico.
Al repetir el comando, reanuda resultados válidos si las entradas y el binario
coinciden, y reintenta errores. `--rerun` reemplaza las mediciones anteriores.

```sh
.venv/bin/python tools/distance_time_experiment.py --plot-only
.venv/bin/python tools/distance_time_experiment.py --no-plots
.venv/bin/python tools/distance_time_experiment.py prueba --maps acassusoA --repetitions 2
```

`--plot-only` regenera los gráficos desde el CSV sin ejecutar los modelos;
`--no-plots` ejecuta o reanuda las corridas y guarda el CSV sin necesitar matplotlib.
Si se interrumpe el experimento, repetí el mismo comando y nombre para continuar
desde las corridas pendientes.

Ejecutar el experimento 2 del plan (función objetivo de los modelos de distancia):

```sh
make mip mip-best-cluster
.venv/bin/pip install matplotlib
.venv/bin/python tools/distance_objective_experiment.py
```

Usa los mismos mapas, particiones, modelos y preprocesamiento que el experimento
1, pero grafica el costo total del viaje. En MIP y F&O toma `objective`; en
bestClusterPath toma `distance`, porque el `objective` propio de ese ejecutable
representa la cantidad de cruces entre clusters. El CSV conserva ambos campos
como `objective` (distancia comparable) y `model_objective`, además de
`crossings`.

Los resultados se guardan incrementalmente en
`experiments/experimento_2/resultados.csv`; los gráficos son
`experimento_2_objetivo_bfs.png` y `experimento_2_objetivo_streets.png`. También
admite `--plot-only`, `--no-plots`, `--rerun`, `--maps`,
`--cluster-percentage` y `--repetitions`, con la misma semántica de reanudación
del experimento 1.

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
