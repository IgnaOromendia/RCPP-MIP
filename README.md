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
./solverExec input.dat curvas.dat [mip|fixAndOptimize reachability [deadheadCost|random]]
```

La estrategia es opcional y por defecto es `mip`, que resuelve únicamente el
modelo MIP. También puede indicarse explícitamente con `mip`, sin pasar un valor
de reachability. Para ejecutar la heurística se usa `fixAndOptimize` seguido por
`reachability`, un entero no negativo que controla el radio BFS de la vecindad.
La estrategia de selección de la vecindad puede ser `deadheadCost` (valor por
defecto, pondera los arcos por su costo de recorrido sin servicio) o `random`.
El programa usa capacidad `10000` y un máximo de
`10000` recorridos sin servicio por arco y vehículo. La solución se escribe en
`out.dat`.

Al comenzar una corrida válida, el ejecutable informa `Strategy: mip` o
`Strategy: fixAndOptimize`. En el segundo caso también imprime
`Selection strategy: deadheadCost` o `Selection strategy: random`.

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
configurar parámetros, resolver y consultar valores. Cada invocación al motor
de CPLEX tiene un límite fijo de 300 segundos. `RCPPSolver` conserva la
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
./run_solver.sh 100 mip
```

El primer parámetro es la cantidad exacta de nodos (entero >= 3, sin ceros
iniciales). Los argumentos restantes se reenvían al solver; por ejemplo,
`./run_solver.sh 100 fixAndOptimize 2 random` ejecuta la heurística con
reachability 2 y selección aleatoria.
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
./solverExec input/graph_100.dat input/graph_100.turns.dat fixAndOptimize 2
```

Genera un grafo conexo, plano y no dirigido con la cantidad indicada de nodos
(mínimo 3), contorno no requerido (zona 0), aristas interiores requeridas
repartidas 50/50 entre las zonas conexas 1 y 2, y grado promedio 4 desde 14 nodos.
Cada arista recibe un costo de recorrido real aleatorio reproducible. Los archivos
se guardan en `input/`; repetir el tamaño los reemplaza.

- `--seed`: semilla para reproducir la instancia.
- `--svg`: genera una imagen del grafo.
- `--vehicles`: cantidad de vehículos (2 por defecto y como mínimo).
- `--demand-type`: `fixed`, `integer` o `real`. En los dos últimos modos se
  sortea una demanda independiente para cada arista requerida de zonas 1 y 2.
- `--demand-min` y `--demand-max`: rango de la demanda aleatoria (1 y 10 por
  defecto). En modo `integer` los límites deben ser enteros y son inclusivos;
  en modo `real` se usa una distribución uniforme. La semilla hace reproducible
  el muestreo sin alterar la topología ni los giros.
- `--demand`: valor fijo positivo, entero o real, usado solamente con
  `--demand-type fixed` (modo predeterminado, con demanda 1).
- `--cost-min` y `--cost-max`: rango uniforme del costo real aleatorio de cada
  arista (1 y 10 por defecto). El muestreo usa la semilla sin alterar la
  topología, los giros ni las demandas.

Las aristas del contorno (zona 0) siempre tienen demanda 0. Las aristas interiores
se reparten de forma reproducible entre las zonas conexas 1 y 2; si su cantidad
es impar, una zona contiene una arista más que la otra. La conectividad se mide
entre aristas que comparten un nodo. En los casos mínimos donde hay menos de dos
aristas interiores, alguna zona puede quedar vacía.

## Experimentos de reachability

Después de compilar, el benchmark completo se ejecuta con:

```sh
make
python3 tools/reachability_experiments.py nombre_del_experimento
```

Para cada tamaño ejecuta primero una corrida default pasando explícitamente la
estrategia `mip`. Luego, para cada reachability configurada, ejecuta
Fix-and-Optimize una vez con cada estrategia de selección disponible: `random` y
`deadheadCost`. La estrategia queda registrada en el CSV y forma parte de la
clave de reanudación, por lo que una corrida ya completada no omite ni
sobrescribe la correspondiente a la otra estrategia.
Los tamaños predeterminados son `100 120 180 220 260 300 340`, y las
reachabilities predeterminadas son `5%`, `15%` y `25%` de `n`. El runner no
impone un timeout al proceso: cada llamada al
motor de CPLEX usa su límite interno fijo de 300 segundos. El proceso es reanudable:
cada resultado se agrega inmediatamente a
`experiments/nombre_del_experimento/resultados.csv` y las combinaciones ya
presentes se omiten. El primer parámetro es el nombre del experimento y todos
sus artefactos se guardan en `experiments/<nombre>/`. Use `--rerun` para repetir
las combinaciones.

Los artefactos principales son:

- `tiempo_por_reachability.png`: tamaño vs. tiempo, una línea para MIP default y
  una por cada combinación de reachability y estrategia de selección.
- `optimalidad_por_reachability.png`: mapa de calor del valor objetivo mediano,
  incluida una fila para MIP default. El color se escala de forma independiente
  para cada `n`: el menor objetivo es el mejor (verde), una diferencia del 15%
  o más respecto del menor es la peor (rojo), y los valores intermedios usan el
  degradé; la etiqueta también indica si las corridas terminaron en `Optimal`.
- `logs/`: stdout y stderr de cada ejecución.

Los plots requieren `matplotlib`. Los tamaños, reachabilities, cantidad de
repeticiones, semilla y tipo de demanda son configurables. El parámetro
`--demand-type` acepta `fixed`, `integer` o `real` (predeterminado), y se pasa al
generador de instancias. En el modo `real` predeterminado, cada arista requerida
recibe una demanda uniforme reproducible entre 1 y 10. Por ejemplo:

```sh
python3 tools/reachability_experiments.py \
  comparacion_reachability \
  --sizes 100 500 700 \
  --reachability-percentages 5 10 15 20 25 \
  --seed 42 \
  --demand-type integer \
  --repetitions 3
```

La formulación actual construye estructuras densas sobre el supergrafo y la
construcción de giros también es costosa. Por eso los tamaños mayores pueden
agotar memoria, y el tiempo total del proceso puede superar el límite aplicado
a cada llamada de CPLEX. Para generar solo el CSV use
`--no-plots`, y para recrear las figuras sin ejecutar el solver use
`--plot-only`.

## Pruebas

```sh
make test-unit                         # Sin CPLEX
make test                              # Incluye integración con CPLEX
python3 tests/generate_graph_test.py    # Solo el generador
```
