# RCPP-MIP

Solver del *Rural Chinese Postman Problem* implementado en C++ con programación
entera mixta y CPLEX.

## Requisitos

- Compilador compatible con C++17
- IBM ILOG CPLEX
- GLib y `pkg-config`
- Python 3.8 o posterior para generar instancias y ejecutar experimentos

## Compilación

```sh
make
```

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
El radio BFS de la vecindad es adaptativo: comienza en 15 y se ajusta entre 5
y 25 durante la búsqueda. `topKDeadheadCost` también adapta internamente la
cantidad de candidatos.

La solución se guarda en `out.dat`. El código de salida es `0` si se obtuvo una
solución, `2` si no se obtuvo ninguna y `1` ante un error. La última línea de la
salida, `RCPP_RESULT ...`, resume el tiempo, disponibilidad, optimalidad y
objetivo para los runners de experimentos.

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

Los archivos se guardan en `input/` y la solución en `out.dat`. Para controlar
la generación directamente:

```sh
python3 tools/generate_graph.py 100 --seed 42 --svg
python3 tools/generate_graph.py 100 --seed 42 \
  --demand-type integer --demand-min 1 --demand-max 20
```

Opciones principales del generador:

- `--seed`: semilla reproducible.
- `--svg`: genera una imagen del grafo.
- `--vehicles`: cantidad de vehículos (2 por defecto).
- `--demand-type`: `fixed`, `integer` o `real`.
- `--demand`, `--demand-min` y `--demand-max`: valor o rango de demanda.
- `--cost-min` y `--cost-max`: rango de costos.

El generador crea un grafo conexo, plano y no dirigido. El contorno pertenece a
la zona opcional `0`; las aristas interiores se distribuyen entre las zonas
conexas `1` y `2`.

## Experimentos

Los runners son reanudables y guardan el CSV, los gráficos y los logs en
`experiments/<nombre>/`. Los gráficos requieren `matplotlib`.

### Estrategias de Fix-and-Optimize

Compara el MIP predeterminado contra `random`, `maxDeadheadCost` y
`topKDeadheadCost`, usando los parámetros adaptativos internos de la heurística:

```sh
python3 tools/selection_strategy_experiments.py comparacion_estrategias
```

También admite `--sizes`, `--seed`, `--demand-type` y `--repetitions`, por
ejemplo:

```sh
python3 tools/selection_strategy_experiments.py comparacion_estrategias \
  --sizes 100 140 180 220 --repetitions 3
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
