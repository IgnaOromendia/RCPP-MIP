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

## Vida útil de CPLEX

Cada `RCPPSolver` posee un único entorno de CPLEX. Un miembro propietario RAII
lo cierra con `IloEnv::end()` después de destruir los miembros dependientes,
tanto al salir normalmente de su alcance como al propagar una excepción.
También libera el entorno si falla la construcción de un miembro posterior
o el cuerpo del constructor. No es necesario cerrar el solver manualmente.

`RCPPSolver` no admite copia ni movimiento. Se pueden construir varias instancias
sucesivas o mantener instancias independientes en un mismo proceso; destruir una
no invalida las demás. Se mantiene el supuesto de archivos de entrada existentes:
las rutas de error que llaman a `exit()` no desenrollan la pila de C++.

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

`tests/solver_lifetime_test.cpp` comprueba destrucción sin construir el modelo,
resoluciones sucesivas en un mismo proceso, independencia entre instancias y
destrucción ante excepciones de construcción y uso. El fallo de construcción
usa un archivo de giros con cantidad negativa para provocar `std::length_error`
en la reserva del vector; no constituye validación del formato de entrada.
También se verifica en compilación que el solver y el propietario del entorno
no admitan copia ni movimiento.

Para comprobar fugas en macOS, se puede ejecutar desde un directorio temporal
`leaks --atExit -- /ruta/RCPP-MIP/build/solver_lifetime_test /ruta/RCPP-MIP/tests/fixtures repeated`.
Los otros casos son `unbuilt`, `independent`, `constructor_error` y `use_error`;
el caso `constructor_error` escribe su archivo de giros en el directorio actual.
Esta comprobación es adicional a `make test` y requiere que macOS permita
inspeccionar el proceso. En la verificación de este cambio, los cinco casos
informaron cero fugas detectadas.

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
