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

`make test` compila y ejecuta pruebas de integración con CPLEX; requiere las
dependencias de compilación del solver, una instalación operativa de CPLEX y
Python 3. Cada prueba utiliza un directorio temporal para aislar `out.dat`.

Se verifican soluciones óptimas y factibles, infactibilidad, interrupciones sin
solución, consultas antes de resolver, invalidación de soluciones anteriores,
errores de CPLEX y exportación, y códigos de salida. Las interrupciones usan un
límite de una solución o un abortador, sin depender de tiempos de ejecución.
La instancia mínima tiene un arco requerido de costo 7 y demanda 3, con ambos
extremos adyacentes al depósito. La instancia infactible aumenta la demanda a
10001, por encima de la capacidad 10000.
