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

Guarda la solución en `out.dat`. Opciones principales:

- `--capacity <numero>`: capacidad por vehículo (por defecto, 10000).
- `--output <ruta>`: archivo de salida.
- `--help`: ayuda del ejecutable.

Código de salida: `0` si exportó una solución, `2` si no obtuvo ninguna y `1`
si hubo un error. Si no hay solución, un archivo de salida anterior se conserva.

## Generar una instancia

Requiere Python 3.8 o posterior, sin paquetes adicionales.

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
