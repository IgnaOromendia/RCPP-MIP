# RCPP-MIP

Solver del *Rural Chinese Postman Problem* implementado en C++ con programación
entera mixta y CPLEX.

La infraestructura común de Concert/CPLEX vive en `cplexSolver/`. Tanto el
solver principal de `mipSolver/` como los modelos de ordenamiento de caminos
derivan de esa abstracción compartida. `mipPathSort/` contiene
`PathSolver`, sus restricciones de orden compartidas, sus tipos y su
entrada/salida; `mipPathSort-FirstPass/` contiene el objetivo de distancia
actual y `mipPathSort-Clusters/` implementa el objetivo por clusters y genera
`pathSortClusterExec`.

## Requisitos

- Compilador compatible con C++17
- IBM ILOG CPLEX
- GLib y `pkg-config`
- Python 3.8 o posterior para generar instancias y ejecutar experimentos

## Compilación

```sh
make
```

También se puede compilar cada componente por separado:

```sh
make -C mipSolver               # genera solverExec
make -C mipPathSort             # genera build/libpathsortcommon.a
make -C mipPathSort-FirstPass   # genera pathSortExec y su biblioteca
make -C mipPathSort-Clusters    # genera pathSortClusterExec y su biblioteca
```

El `Makefile` de la raíz coordina los componentes. Sus targets `mip`,
`path-common`, `path-first-pass` y `path-clusters` permiten compilarlos por
separado. `make path` se conserva como alias de `path-first-pass`.

El Makefile usa por defecto CPLEX en `/Applications/CPLEX_Studio2211` con la
plataforma `arm64_osx`. Para usar otra instalación:

```sh
make CPLEX_DIR=/ruta/CPLEX_Studio CPLEX_PLATFORM=<plataforma>
```

## Uso

```sh
./solverExec input.dat curvas.dat [estrategia]
```

Estrategias disponibles:

```sh
# MIP completo
./solverExec input.dat curvas.dat mip

# Fix-and-Optimize (estrategia predeterminada)
./solverExec input.dat curvas.dat fixAndOptimize \
  [maxDeadheadCost|random|topKDeadheadCost]
```

Si no se indica una estrategia, se usa Fix-and-Optimize con
`topKDeadheadCost`. Ese mismo método de selección se usa al indicar
`fixAndOptimize` sin un método explícito.
El radio BFS de la vecindad es adaptativo: comienza en 10 y se ajusta entre 5
y 30 durante la búsqueda. El BFS selecciona nodos virtuales y libera todos los
super-arcos inducidos, incluidos los conectores del depósito adyacentes.
`topKDeadheadCost` también adapta internamente la cantidad de candidatos.

La solución se guarda en `output/dist/out_N.dat`, donde `N` es la cantidad de
nodos del grafo original leída desde la instancia. El directorio se crea
automáticamente. El código de salida es `0` si se obtuvo una solución, `2` si no
se obtuvo ninguna y `1` ante un error. La última línea de la salida,
`RCPP_RESULT ...`, resume el tiempo, disponibilidad, optimalidad y objetivo para
los runners de experimentos.

El orden de las pasadas de una solución de RCPP se obtiene con:

```sh
./pathSortExec input.dat curvas.dat output/dist/out_N.dat \
  [salida_orden.dat] [salida_segmentos.csv]
```

El cuarto argumento es opcional; si se omite, el resultado se escribe en
`output/order/out_N.dat`, usando nuevamente la cantidad de nodos de la
instancia, y el directorio se crea automáticamente. Cada fila contiene la
posición, el vehículo, los nodos virtuales de origen y destino, el número de
pasada, el identificador del super-arco y el identificador del tramo original.
Si también se omite el quinto argumento, los tramos se escriben en
`output/order/route_segments_N.csv`. El depósito se representa con `D`. El
código de salida es `0` si se exportó el orden, `2` si el MIP no encontró una
solución y `1` ante argumentos o archivos inválidos, errores de CPLEX o de
escritura.

`PathSortInstanceReader` consume ese archivo junto con la misma `Instance`
usada por el solver. `PathSorterFirstPass` recibe el resultado ya interpretado
y no realiza entrada/salida:

```cpp
PathSortInstance input = PathSortInstanceReader::read_file(
    instance, "output/dist/out_100.dat");
PathSorterFirstPass sorter(std::move(input));
for (const PathEdge& edge : sorter.edges()) {
    // Cantidad total de pasadas por el arco para edge.vehicle.
    const long long passages = edge.times(); // X + Y
}
```

Si todavía no se cargó la instancia del grafo, el reader puede leer los tres
archivos en una sola llamada:

```cpp
PathSortInstance input = PathSortInstanceReader::read_files(
    "input.dat", "curvas.dat", "output/dist/out_100.dat");
```

El vector conserva la orientación y el vehículo. `original_edge_id` indexa
primero `Instance::edges` y luego `Instance::arcs`; vale `-1` para conectores de
giro y `-2` para conectores del depósito. En `from` y `to`, el depósito se
representa mediante `PathSortInstance::deposit`, cuyo valor es `|V(G)|` (el
índice inmediatamente posterior a los nodos virtuales del supergrafo). Solo se
guardan entradas cuya multiplicidad total es positiva.
La multiplicidad de cada entrada se consulta mediante `PathEdge::times()`, que
suma `service_count + deadhead_count`. El ordenador representa cada pasada de
forma explícita: `X` guarda su posición y `Z` enlaza pasadas consecutivas. El
modelo supone un único vehículo activo y ordena sus posiciones de `1` a `K`.
El lector específico de clusters vive en `mipPathSort-Clusters` y combina la
instancia de orden con `clusters_N.dat`. Su vector queda alineado con
`PathSortInstance::edges`, aunque el archivo tenga otro orden de filas:

```cpp
ClusterPathSortInstance input = ClusterPathSortInstanceReader::read_files(
    "input.dat", "curvas.dat", "output/dist/out_100.dat",
    "data/clusters/clusters_100.dat");
PathSorterCluster sorter(std::move(input));
const int cantidad_clusters = sorter.cluster_count();
const std::vector<int>& cluster_por_arista = sorter.edge_clusters();
```

El lector empareja por origen, destino y vehículo, y valida también las pasadas.
Los archivos numeran clusters desde `1`; `ClusterPathSortInstance` resta uno a
cada identificador, conservando posibles huecos entre ellos. El modelo ignora
esos identificadores ausentes al construir las restricciones entre clusters.
`PathSorterCluster` genera y resuelve el modelo, y permite recuperar el orden
con `extract_order()` después de una solución factible. Su objetivo minimiza la
cantidad de transiciones consecutivas entre pasadas de clusters diferentes,
es decir, la suma de `Z[r][s]` para pares con distinto cluster.

La variante por clusters también está disponible desde línea de comandos:

```sh
./pathSortClusterExec input.dat curvas.dat output/dist/out_N.dat \
  data/clusters/clusters_N.dat [salida_orden.dat] [salida_segmentos.csv]
```

Si se omiten las salidas opcionales, usa los mismos destinos que
`pathSortExec`: `output/order/out_N.dat` y
`output/order/route_segments_N.csv`. El circuito inicial de Hierholzer se
exporta antes de resolver CPLEX como CSV de segmentos con el sufijo `_h`, por
ejemplo `output/order/route_segments_N_h.csv`. Los runners de clusters que
generan videos también producen `output/videos/route_N_h.gif` a partir de ese
circuito inicial.

## Formato de entrada

Los archivos contienen valores separados por espacios o saltos de línea, sin
comentarios ni datos adicionales. Los nodos se numeran de `1` a `N`.

### Grafo (`input.dat`)

```text
V N D E A
d1 ... dD
u v zona costo demanda
...
```

- `V`: vehículos; `N`: nodos.
- `D`: nodos adyacentes al depósito, seguidos por sus identificadores.
- `E` y `A`: número de aristas no dirigidas y arcos dirigidos. Sus filas se
  escriben en ese orden.
- `zona`: `0` si el tramo no es requerido, `-1` si puede atenderlo cualquier
  vehículo, o `1..V` si corresponde a un vehículo específico.
- `costo` debe ser positivo y `demanda` no negativa.

El depósito se agrega automáticamente.

### Giros (`curvas.dat`)

```text
T P
u v w
...
```

Primero se escriben los `T` giros listados y luego los `P` giros prohibidos.
Cada triple representa `u → v → w`; el sentido inverso es otro giro. Actualmente
solo los prohibidos restringen el recorrido y los retornos en U siempre están
excluidos. Para no definir giros, use `0 0`.

## Generar y resolver una instancia

El atajo siguiente genera un grafo de 100 nodos, compila y ejecuta el solver:

```sh
./run_solver.sh 100 mip
```

También acepta los argumentos de Fix-and-Optimize:

```sh
./run_solver.sh 100 fixAndOptimize random
```

Para ejecutar el flujo completo —generación, solver RCPP, clustering BFS,
ordenamiento por clusters y GIF—:

```sh
./run_solver_cluster.sh 100 10 mip
```

`run_solver_cluster.sh` acepta las mismas estrategias opcionales que
`run_solver.sh`; el segundo argumento obligatorio es el porcentaje maximo de
aristas distintas por cluster. Genera `data/clusters/clusters_N.dat`, sus
visualizaciones SVG y PNG, el orden en `output/order/`, y
`output/videos/route_N.gif` mediante `./pathSortClusterExec`. Si
`output/dist/out_N.dat` ya existe y no esta vacio, reutiliza esa solucion y
omite la ejecucion del solver. `run_map_generator.sh` aplica el mismo criterio.

Con esos archivos ya generados, el ordenamiento por clusters y su GIF se crean
con:

```sh
./run_sorter_cluster.sh 100
```

El script compila y ejecuta exactamente `./pathSortClusterExec`, escribe el CSV
de segmentos y genera `output/videos/route_100.gif`.

Los archivos del solver se guardan en `input/`, las coordenadas en
`data/coords/graph_N.coords.csv`, la solución agregada en `output/dist/out_N.dat`, el
recorrido completo en `output/order/out_N.dat` y sus tramos reales en
`output/order/route_segments_N.csv`. El GIF se genera en
`output/videos/route.gif`. El script sólo ejecuta el ordenador si RCPP termina
correctamente.

Si falta Pillow, se instala con `python3 -m pip install pillow`.

Para controlar la generación directamente:

```sh
python3 tools/generate_graph.py 100 --seed 42 --svg
python3 tools/generate_graph.py 100 --seed 42 \
  --demand-type integer --demand-min 1 --demand-max 20
python3 tools/generate_graph.py 100 --seed 42 --vehicles 1 --free
```

Opciones principales del generador:

- `--seed`: semilla reproducible.
- `--svg`: genera una imagen del grafo.
- `--vehicles`: cantidad de vehículos, al menos 1 (2 por defecto).
- `--demand-type`: `fixed`, `integer` o `real` (`real` por defecto).
- `--demand`, `--demand-min` y `--demand-max`: valor fijo o rango de demanda;
  usar `--demand` selecciona demanda fija aunque se omita `--demand-type fixed`.
- `--cost-min` y `--cost-max`: rango de costos.
- `--free`: asigna todas las aristas interiores a la zona `-1`, para que
  cualquiera de los vehículos pueda atenderlas; el contorno permanece en `0`.

El CSV de coordenadas se escribe siempre con las columnas `node_id,x,y`. El
animador también puede ejecutarse de forma independiente:

```sh
python3 tools/generate_route_video.py \
  --segments output/order/route_segments_100.csv \
  --coords data/coords/graph_100.coords.csv \
  --output output/videos/route.gif
```

También se puede generar una animación HTML autocontenida pasando únicamente la
cantidad de nodos:

```sh
python3 tools/generate_route_html.py 100
open output/videos/route_100.html
```

El script lee `data/coords/graph_100.coords.csv` y
`output/order/route_segments_100.csv`, y genera
`output/videos/route_100.html`. El navegador redibuja el recorrido en un canvas
adaptado a la resolución y densidad de píxeles de la pantalla. El HTML incluye controles
para reproducir, pausar, avanzar, retroceder, cambiar la velocidad y usar
pantalla completa, y no necesita conservar acceso a los CSV originales.

Para las instancias OSM, el script detecta el nombre del mapa a partir de la
cantidad de nodos, usa `data/generator/coordinates/nodes_<mapa>.coords.csv` e
incrusta el PNG y sus límites geográficos en el propio HTML. Busca esos archivos
en `data/generator/maps/` y, por compatibilidad con el proyecto original, en
`../RESIDUOS-SI/pngMaps/`. También se pueden indicar manualmente:

```sh
python3 tools/generate_route_html.py 2361 \
  --map-image ../RESIDUOS-SI/pngMaps/acassusoA.png \
  --bounds ../RESIDUOS-SI/pngMaps/acassusoA.bounds
```

Los CSV generados por `pathSortClusterExec` incluyen una columna `cluster`.
Agregar `--cluster` al comando anterior colorea cada tramo con el color de su
cluster; sin esa opción se conserva la coloración normal por vehículo.

`route_segments_N.csv` usa las columnas
`vehiculo,orden,nodo_origen,nodo_destino`, conserva las pasadas repetidas y
omite los conectores virtuales de giro y depósito.

El generador crea un grafo conexo, plano y no dirigido. El contorno pertenece a
la zona opcional `0`; las aristas interiores se distribuyen entre las zonas
conexas `1` y `2`, salvo que se use `--free`.

## Generar clusters BFS

`clusterGeneration/generate_clusters.py` toma la solucion agregada del solver,
suma las pasadas `X + Y` de cada arco y descarta los arcos con cero pasadas.
Usa `--graph` para colapsar las orientaciones y los vehiculos del supergrafo
sobre sus aristas originales, calcula sobre estas los clusters conexos mediante
BFS y luego propaga cada cluster a los arcos virtuales de la solucion. Los
conectores de giro heredan el cluster de la arista original a la que ingresan y
los conectores de llegada al deposito, el de la arista de la que salen. El
parametro obligatorio `--percentage` representa el porcentaje maximo de
aristas originales distintas por cluster; el ultimo cluster puede ser mas
pequeno.

```sh
python3 clusterGeneration/generate_clusters.py output/dist/out_100.dat \
  --percentage 10 \
  --graph input/graph_100.dat
```

Para dibujar sobre la geometria original, como hace `run_solver_cluster.sh`:

```sh
python3 clusterGeneration/generate_clusters.py output/dist/out_100.dat \
  --percentage 10 \
  --graph input/graph_100.dat \
  --coords data/coords/graph_100.coords.csv
```

Las salidas predeterminadas son `data/clusters/clusters_N.dat`,
`data/clusters/clusters_N.svg` y `data/clusters/clusters_N.png`, donde `N` se
obtiene del nombre `out_N.dat`.
Cada fila del archivo de datos asigna una arista dirigida y un vehiculo a un
cluster, conservando por separado servicio, recorridos adicionales y pasadas
totales. El SVG y el PNG colorean las aristas por cluster, usan el grosor para
representar las pasadas y muestran con linea punteada —sin cambiar su color— las
aristas cuyos extremos pertenecen a regiones visuales distintas. Con `--graph`
y `--coords`, las imagenes muestran la red original en gris y proyectan sobre
sus coordenadas las aristas recorridas; los conectores virtuales de giro se
omiten porque colapsan sobre una misma interseccion. Sin coordenadas se usa un
layout determinista del grafo original. Los arcos del deposito usan `D` como
extremo. En mapas con coordenadas reales, `D` se ubica con un desplazamiento
puramente visual respecto del nodo adyacente y no participa de la escala
geografica. Para otro nombre de entrada se
puede indicar `--nodes N`; `--output RUTA` cambia el nombre del `.dat` y las
imagenes usan el mismo nombre con extensiones `.svg` y `.png`. La rasterizacion
requiere `rsvg-convert`, ImageMagick o CairoSVG.

## Experimentos

Los runners son reanudables y guardan el CSV, los gráficos y los logs en
`experiments/<nombre>/`. Los gráficos requieren `matplotlib`.

### Estrategias de Fix-and-Optimize

Compara el MIP predeterminado contra `random`, `maxDeadheadCost` y
`topKDeadheadCost`, usando los parámetros adaptativos internos de la heurística:

```sh
python3 tools/selection_strategy_experiments.py comparacion_estrategias
```

También admite `--sizes`, `--seed`, `--demand-type`, `--vehicles`, `--free` y
`--repetitions`. Por ejemplo, para usar un único vehículo y asignarle todas las
aristas requeridas mediante la zona libre `-1`:

```sh
python3 tools/selection_strategy_experiments.py comparacion_estrategias \
  --sizes 100 140 180 220 --repetitions 3 --vehicles 1 --free
```

Para comparar solamente el MIP predeterminado contra Top-K:

```sh
python3 tools/selection_strategy_experiments.py default_vs_topk \
  --selection-strategies topKDeadheadCost
```

El runner acepta `--plot-only` para regenerar gráficos y `--no-plots` para
generar solo resultados. Use `--rerun` para repetir combinaciones ya guardadas.

## Pruebas

```sh
make test-unit                       # Sin CPLEX
make test                            # Incluye integración con CPLEX
python3 tests/generate_graph_test.py  # Solo el generador
```
