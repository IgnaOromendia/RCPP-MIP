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
./solverExec input.dat curvas.dat
```

Solo acepta los dos archivos de entrada obligatorios, sin opciones adicionales.
Sin argumentos muestra el uso. El programa fija `gap = 0` y `cutsMode = -1`
en el código, usa capacidad `10000` y un máximo de `10000` recorridos sin
servicio por arco y vehículo. La solución se escribe en `out.dat`.

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
- `--vehicles` y `--demand`: cantidad de vehículos y demanda por arista de zona 0
  (ambas con valor 1 por defecto). Las aristas del contorno (zona -1) tienen demanda 0.

## Pruebas

```sh
make test-unit                         # Sin CPLEX
make test                              # Incluye integración con CPLEX
python3 tests/generate_graph_test.py    # Solo el generador
```
