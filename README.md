# RCPP-MIP

Solver del *Rural Chinese Postman Problem* implementado en C++ con programación
entera mixta y CPLEX.

La infraestructura común de Concert/CPLEX vive en `cplexSolver/`. Tanto el
solver principal de `mipSolver/` como el ordenador de caminos de
`mipPathSort/` derivan de esa abstracción compartida.

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
make -C mipSolver    # genera solverExec
make -C mipPathSort  # genera pathSortExec y build/libpathsorter.a
```

El `Makefile` de la raíz solo coordina ambos componentes. Sus targets `mip` y
`path` permiten seleccionar uno sin compilar el otro (`make mip` o
`make path`).

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
# MIP completo (estrategia predeterminada)
./solverExec input.dat curvas.dat mip

# Fix-and-Optimize
./solverExec input.dat curvas.dat fixAndOptimize \
  [maxDeadheadCost|random|topKDeadheadCost]
```

Si no se indica un método de selección, se usa `maxDeadheadCost`.
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
usada por el solver. `PathSorter` recibe el resultado ya interpretado y no
realiza entrada/salida:

```cpp
PathSortInstance input = PathSortInstanceReader::read_file(
    instance, "output/dist/out_100.dat");
PathSorter sorter(std::move(input));
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
suma `service_count + deadhead_count`. Las restricciones auxiliares con `A` se
usan únicamente cuando esa multiplicidad es mayor que uno; si existe una sola
pasada, su posición se obtiene directamente desde `Z`.

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

`route_segments_N.csv` usa las columnas
`vehiculo,orden,nodo_origen,nodo_destino`, conserva las pasadas repetidas y
omite los conectores virtuales de giro y depósito.

El generador crea un grafo conexo, plano y no dirigido. El contorno pertenece a
la zona opcional `0`; las aristas interiores se distribuyen entre las zonas
conexas `1` y `2`, salvo que se use `--free`.

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
