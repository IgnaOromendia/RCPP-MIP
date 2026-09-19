# RCPP-MIP

Solver del Rural Chinese Postman Problem con programación entera mixta en C++ y CPLEX.

## Compilar

Requiere un compilador C++17, IBM ILOG CPLEX, GLib y `pkg-config`.

```sh
make
```

Por defecto usa CPLEX en `/Applications/CPLEX_Studio2211` y la plataforma
`arm64_osx`. Para otra instalación:

```sh
make CPLEX_DIR=/ruta/CPLEX_Studio CPLEX_PLATFORM=<plataforma>
```

## Usar

```sh
./solverExec input.dat curvas.dat [gap] [cutsMode] [opciones]
```

Los dos archivos son obligatorios. Los parámetros entre corchetes son opcionales:

| Parámetro | Descripción | Por defecto |
| --- | --- | --- |
| `gap` | Tolerancia relativa de optimalidad; `0.01` equivale a 1 %. | `0` |
| `cutsMode` | Entero enviado a CPLEX para cortes de cliques, covers y flow covers. | `-1` |
| `--capacity <numero>` | Capacidad por vehículo, positiva y finita. | `10000` |
| `--max-traversals <entero>` | Máximo de recorridos sin servicio por arco y vehículo, no negativo. | `10000` |
| `--output <ruta>` | Archivo de solución. | `out.dat` |
| `--help` / `-h` | Muestra la ayuda. | — |

Para indicar `cutsMode` también hay que indicar `gap`; CPLEX valida sus rangos.

```sh
./solverExec input.dat curvas.dat 0.01 -1 --capacity 500 --output solucion.dat
```

Código de salida: `0` si exportó una solución, `2` si no obtuvo ninguna y `1`
si hubo un error. Si no hay solución, un archivo de salida anterior se conserva.

## Formato de entrada

Ambos archivos contienen valores separados por espacios o saltos de línea,
sin comentarios ni datos adicionales. Los nodos se numeran de `1` a `N`.

**`input.dat` (grafo):**

```text
V N D E A
d1 ... dD
u v zona costo demanda
...
```

- `V`: cantidad de vehículos; `N`: cantidad de nodos. Ambos positivos.
- `D`: cantidad de nodos adyacentes al depósito (`0 <= D <= N`), seguidos por sus
  identificadores `d1 ... dD`. El depósito se agrega automáticamente.
- `E` y `A`: cantidades no negativas de aristas no dirigidas y arcos dirigidos.
  Luego de los adyacentes vienen exactamente `E` filas de aristas y `A` de arcos,
  en ese orden. En los arcos, el sentido es `u → v`.
- `zona`: `0` para un tramo no requerido, `-1` para uno requerido que puede
  atender cualquier vehículo, o `1..V` para uno requerido por ese vehículo.
- `costo`: número positivo y finito; `demanda`: número no negativo y finito.

**`curvas.dat` (giros):**

```text
T P
u v w
...
```

`T` y `P` son cantidades no negativas: primero vienen `T` triples de giros
listados y luego `P` triples de giros prohibidos. Cada triple representa el
recorrido `u → v → w`; el sentido inverso es otro giro. Actualmente solo los
prohibidos restringen el recorrido; los retornos en U se excluyen siempre.
Para no listar giros, el archivo debe contener `0 0`.

## Generar una instancia

Requiere Python 3.8 o posterior, sin paquetes adicionales.

Para generar una instancia, compilar y correr el modelo desde la raíz del repositorio:

```sh
./run_solver.sh 100
```

El parámetro es la cantidad exacta de nodos (entero >= 3, sin ceros iniciales).
Usa la semilla predeterminada 0 y los giros generados. Guarda la instancia en
`input/graph_100.dat`, los giros en `input/graph_100.turns.dat` y la solución
en `out.dat`, dentro del repositorio. Repetir el tamaño reemplaza la instancia;
una nueva solución reemplaza `out.dat`.

Para configurar la generación por separado:

```sh
python3 tools/generate_graph.py 100 --seed 42 --svg
./solverExec input/graph_100.dat input/graph_100.turns.dat
```

Genera un grafo conexo, plano y no dirigido con la cantidad indicada de nodos
(mínimo 3), contorno requerido y grado promedio 4 desde 14 nodos.
Los archivos se guardan en `input/`; repetir el tamaño los reemplaza.

- `--seed`: semilla para reproducir la instancia.
- `--svg`: genera una imagen del grafo.
- `--vehicles` y `--demand`: cantidad de vehículos y demanda por arista del contorno
  (ambas con valor 1 por defecto).

## Pruebas

```sh
make test-unit                         # Sin CPLEX
make test                              # Incluye integración con CPLEX
python3 tests/generate_graph_test.py    # Solo el generador
```
