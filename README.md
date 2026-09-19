# RCPP-MIP
Rural Chinese Postman Problem

## Resultado de resolución

`solve()` devuelve un `SolveResult` con `has_solution` y el estado simbólico de
CPLEX. Se pueden consultar y exportar soluciones factibles u óptimas. Antes de
resolver, después de modificar el modelo o cuando una resolución no obtiene una
solución, `is_feasible()` devuelve `false`; `get_obj_value()` y
`export_solution()` lanzan `std::logic_error` sin abrir el archivo de salida.

El ejecutable exporta a `out.dat` y devuelve:

- `0`: solución exportada, aunque no se haya probado óptima.
- `2`: resolución sin solución disponible; el diagnóstico distingue la
  infactibilidad de otros estados sin solución.
- `1`: error de CPLEX, de consulta o de exportación.

Una ejecución sin solución no crea ni sobrescribe `out.dat`. Si existe un
archivo de una ejecución anterior, se conserva: los consumidores deben comprobar
el código de salida antes de utilizarlo. La invocación sin argumentos conserva
el comportamiento de mostrar la ayuda y devolver `0`.

## Pruebas

`make test` compila y ejecuta pruebas de grafos y de integración con CPLEX; requiere las
dependencias de compilación del solver, una instalación operativa de CPLEX y
Python 3. Cada prueba utiliza un directorio temporal para aislar `out.dat`.

Se verifican soluciones óptimas y factibles, infactibilidad, interrupciones sin
solución, consultas antes de resolver, invalidación de soluciones anteriores,
errores de CPLEX y exportación, y códigos de salida. Las interrupciones usan un
límite de una solución o un abortador, sin depender de tiempos de ejecución.
La instancia mínima tiene un arco requerido de costo 7 y demanda 3, con ambos
extremos adyacentes al depósito. La instancia infactible aumenta la demanda a
10001, por encima de la capacidad 10000.

Las pruebas de `Graph` y `SuperGraph` cubren grafos mixtos, solo dirigidos y solo
no dirigidos, con tramos requeridos y no requeridos. Comprueban identificadores,
atributos, adyacencias, parejas de orientaciones y conectores. Sus binarios se
pueden compilar sin enlazar CPLEX con
`make build/graph_test build/super_graph_test`; por ejemplo, se ejecutan con
`build/graph_test tests/fixtures/mixed_ids.dat 2` y
`build/super_graph_test tests/fixtures/mixed_ids.dat 2`.
El último argumento indica la cantidad de aristas no dirigidas de la instancia.

También se comprueban la inicialización por defecto y el movimiento de
`SuperGraph`: depósito, arcos, parejas, adyacencias y distancias calculadas,
incluida la asignación sobre un destino con datos y la reasignación del origen.

## Construcción y movimiento de grafos

`Graph` y `SuperGraph` construidos por defecto están sin construir: sus contadores
valen cero, sus contenedores están vacíos y `deposit()` devuelve `-1`. Las
operaciones que requieren nodos válidos o calculan distancias deben realizarse
después de asignarles un grafo construido desde una instancia.

`SuperGraph` no admite copia. Para transferirlo se usa un temporal o
`std::move`; tanto el constructor como la asignación de movimiento son
`noexcept` y transfieren todos sus datos, incluidas las distancias calculadas.
Después del movimiento, el origen solo debe destruirse o recibir un nuevo grafo
por asignación; no se garantiza que quede vacío ni que conserve su contenido.
La construcción desde `Graph` puede propagar excepciones de reserva de memoria.

## Identificadores de tramos

Cada tramo tiene un `Edge::id` global que coincide con su posición en
`Graph::_all_edges`: primero las aristas no dirigidas, después los arcos dirigidos
y finalmente las conexiones sintéticas del depósito. Las posiciones locales en
`_edges` y `_arcs` no cambian ese identificador. `requested_idx` enumera solo los
tramos requeridos y vale `-1` para los demás.

En el supergrafo, `SuperArc::edge_id` conserva el identificador global del tramo
original; ambas orientaciones de una arista comparten ese valor. Los conectores
de giro usan `-1` y los del depósito `-2`. `SuperArc::id` y `pair` pertenecen al
espacio de identificadores del supergrafo, con `pair == -1` para arcos sin pareja.
