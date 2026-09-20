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
./solverExec input.dat curvas.dat reachability
```

`reachability` es un entero no negativo: controla el radio BFS de la vecindad
que libera Fix-and-Optimize. El programa usa capacidad `10000` y un máximo de
`10000` recorridos sin servicio por arco y vehículo. La solución se escribe en
`out.dat`.

Además de la salida descriptiva, el ejecutable siempre imprime una última línea
`RCPP_RESULT ...` con tiempo total en milisegundos, disponibilidad de solución,
estado óptimo, estado de CPLEX y objetivo. Esa línea es el contrato usado por el
runner de experimentos.

Código de salida: `0` si exportó una solución, `2` si no obtuvo ninguna y `1`
si hubo un error. Si no hay solución, un archivo de salida anterior se conserva.

En la API C++, `solve()` devuelve un `SolveResult` con `has_solution` y `status`.
`RCPPSolver` recibe y conserva un `const SuperGraph&`: el supergrafo debe vivir
más que el solver y no debe modificarse ni moverse mientras este lo utiliza.
El constructor rechaza supergrafos temporales para evitar referencias colgantes.
El resultado conserva una copia independiente de la solución: se consulta con
`result.get_obj_value()` y se obtiene como `Solution` con
`result.extract_solution()`, incluso después de destruir el solver o volver a
resolver. Ambas consultas lanzan `std::logic_error` si no hay solución.

`RCPPSolver` hereda de `CPLEXSolver`, que administra el entorno, modelo y motor
de CPLEX. La clase base ofrece métodos protegidos para crear variables y
expresiones, consultar y modificar cotas, agregar restricciones y el objetivo,
configurar parámetros, resolver y consultar valores. `RCPPSolver` conserva la
formulación del problema, la resolución de vecindarios y la captura de
`SolveResult`, incluida la invalidación del resultado antes de modificar o
volver a resolver el modelo.
Para fijar variables temporalmente, `CPLEXSolver` guarda sus cotas en una lista
de `VariableBounds` mediante `fix_and_save_bounds()` y las restaura con
`restore_bounds()`. La resolución de vecindarios restaura esa lista también
cuando ocurre una excepción.

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
./run_solver.sh 100 2
```

El parámetro es la cantidad exacta de nodos (entero >= 3, sin ceros iniciales).
El segundo parámetro opcional es `reachability` y vale 2 por defecto.
Usa la semilla predeterminada 0 y los giros generados. Guarda la instancia en
`input/graph_100.dat`, los giros en `input/graph_100.turns.dat` y la solución
en `out.dat`, dentro del repositorio. Repetir el tamaño reemplaza la instancia;
una nueva solución reemplaza `out.dat`.

Para configurar la generación por separado:

```sh
# Una demanda aleatoria entera por arista, entre 1 y 20 inclusive:
python3 tools/generate_graph.py 100 --seed 42 \
  --demand-type integer --demand-min 1 --demand-max 20 --svg

# Una demanda aleatoria real por arista, entre 0.5 y 10:
python3 tools/generate_graph.py 100 --seed 42 \
  --demand-type real --demand-min 0.5 --demand-max 10
./solverExec input/graph_100.dat input/graph_100.turns.dat 2
```

Genera un grafo conexo, plano y no dirigido con la cantidad indicada de nodos
(mínimo 3), contorno requerido y grado promedio 4 desde 14 nodos.
Los archivos se guardan en `input/`; repetir el tamaño los reemplaza.

- `--seed`: semilla para reproducir la instancia.
- `--svg`: genera una imagen del grafo.
- `--vehicles`: cantidad de vehículos (1 por defecto).
- `--demand-type`: `fixed`, `integer` o `real`. En los dos últimos modos se
  sortea una demanda independiente para cada arista de zona 0.
- `--demand-min` y `--demand-max`: rango de la demanda aleatoria (1 y 10 por
  defecto). En modo `integer` los límites deben ser enteros y son inclusivos;
  en modo `real` se usa una distribución uniforme. La semilla hace reproducible
  el muestreo sin alterar la topología ni los giros.
- `--demand`: valor fijo positivo, entero o real, usado solamente con
  `--demand-type fixed` (modo predeterminado, con demanda 1).

Las aristas del contorno (zona -1) siempre tienen demanda 0.

## Experimentos de reachability

Después de compilar, el benchmark completo se ejecuta con:

```sh
make
python3 tools/reachability_experiments.py nombre_del_experimento
```

Por defecto prueba cinco reachabilities para cada tamaño: `5%`, `10%`, `15%`,
`20%` y `25%` de `n`. Por ejemplo, para `n=1000` usa `50 100 150 200 250`.
Los dieciséis tamaños son `1000 1400 1800 2200 2600 3000 3400 3800 4200 4600
5000 5400 5800 6200 6600 7000`. Cada ejecución tiene un timeout de 300 segundos
(5 minutos). El proceso es reanudable: cada resultado se agrega inmediatamente a
`experiments/nombre_del_experimento/resultados.csv` y las combinaciones ya
presentes se omiten. El primer parámetro es el nombre del experimento y todos
sus artefactos se guardan en `experiments/<nombre>/`. Use `--rerun` para repetir
las combinaciones.

Los artefactos principales son:

- `tiempo_por_reachability.png`: tamaño vs. tiempo, una línea por reachability.
- `optimalidad_por_reachability.png`: mapa de calor del porcentaje de corridas
  cuyo estado final fue `Optimal`.
- `logs/`: stdout y stderr de cada ejecución.

Los plots requieren `matplotlib`. Los tamaños, reachabilities, cantidad de
repeticiones, semilla y timeout son configurables; por ejemplo:

```sh
python3 tools/reachability_experiments.py \
  comparacion_reachability \
  --sizes 1000 5000 10000 \
  --reachability-percentages 5 10 15 20 25 \
  --repetitions 3 --timeout-seconds 1800
```

La formulación actual construye estructuras densas sobre el supergrafo y la
construcción de giros también es costosa. Por eso los tamaños mayores pueden
agotar memoria o alcanzar el timeout; el runner registra esos casos sin
confundirlos con tiempos de resolución válidos. Para generar solo el CSV use
`--no-plots`, y para recrear las figuras sin ejecutar el solver use
`--plot-only`.

## Pruebas

```sh
make test-unit                         # Sin CPLEX
make test                              # Incluye integración con CPLEX
python3 tests/generate_graph_test.py    # Solo el generador
```
