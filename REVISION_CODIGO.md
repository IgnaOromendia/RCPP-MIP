# Revisión de código

Fecha: 18 de septiembre de 2026.

Revisión del código fuente, el Makefile y la documentación del proyecto. Este documento no implica modificaciones al código.

## Alcance y supuestos

- Los costos de los tramos originales no son cero.
- Los archivos de entrada siempre existen.
- Se excluyen las recomendaciones sobre tramos de costo cero y archivos inexistentes.
- La existencia de los archivos no garantiza que su contenido tenga un formato válido; se mantiene esa validación como recomendación.
- Los hallazgos provienen de revisión estática y de una comprobación de compilación sintáctica. No se ejecutaron optimizaciones ni se validaron resultados numéricos.

## Correcciones prioritarias

### 1. Manejar explícitamente la ausencia de solución

**Prioridad: alta.**

`solve()` retorna cuando no encuentra una solución, pero `main()` llama igualmente a `export_solution()`. El exportador consulta `getObjValue()` antes de comprobar el estado, por lo que una instancia infactible puede terminar en una excepción.

Además, `is_feasible()` considera factible cualquier estado distinto de `3`, incluidos estados que no garantizan una solución disponible.

**Mejora propuesta:** devolver un resultado explícito desde `solve()`, usar los estados simbólicos de CPLEX y permitir consultar o exportar valores únicamente cuando haya una solución disponible. Manejar las excepciones en el punto de entrada del programa.

Referencias: [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 355–388; [main.cpp](main.cpp), líneas 23–24.

### 2. Validar el contenido de entrada y los argumentos

**Prioridad: alta si los datos no se validan antes de ejecutar el programa.**

Las lecturas no comprueban que cada extracción tenga éxito ni que las cantidades, los índices de nodos y las zonas estén dentro de rango. Un nodo `0`, una zona mayor que la cantidad de vehículos o un archivo incompleto pueden provocar accesos inválidos.

`atof` y `atoi` tampoco permiten distinguir correctamente una entrada inválida de un cero válido. Por otra parte, varios errores terminan con `exit(0)`, que comunica éxito al proceso que invoca el programa.

**Mejora propuesta:** validar el formato y los rangos antes de construir el grafo o el modelo; informar el dato o registro incorrecto; validar los parámetros de ejecución y devolver códigos de salida adecuados. Si el contenido está garantizado por otro sistema, documentar ese contrato.

Referencias: [Graph.cpp](mipSolver/src/Graph.cpp), líneas 13–60; [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 20–68; [main.cpp](main.cpp), líneas 5–17.

### 3. Unificar los identificadores de aristas y arcos

**Prioridad: media; parte del problema está latente en funciones que hoy no se utilizan.**

Al leer arcos dirigidos, `_requested_idx_to_edge` guarda `i`, pero la posición correspondiente en `_all_edges` es `i + mEdges`. En un grafo mixto, `requested()` puede devolver una arista equivocada.

También se utiliza `i` como identificador dentro de `_arcs`, lo que produce colisiones con los identificadores de las aristas cuando esos valores se trasladan a `SuperArc::edge_id`.

**Mejora propuesta:** utilizar un identificador global consistente para cada tramo original y distinguirlo de los índices locales de los contenedores. Agregar un caso de prueba con aristas y arcos requeridos en la misma instancia.

`requested()` no se invoca en el flujo actual del solver; no se atribuye a este hallazgo un resultado numérico incorrecto observado.

Referencia: [Graph.cpp](mipSolver/src/Graph.cpp), líneas 53–60 y 90–95.

### 4. Corregir la semántica de copia y movimiento de `SuperGraph`

**Prioridad: media.**

`operator=(SuperGraph&)` utiliza `move`: una asignación desde un objeto normal vacía los contenedores del origen. Además, omite `_deposit`. El constructor y el operador de movimiento omiten `_depo_dist`, por lo que las distancias calculadas no se transfieren correctamente.

Los constructores vacíos de `Graph` y `SuperGraph` también dejan campos escalares sin inicializar.

**Mejora propuesta:** eliminar explícitamente la copia y utilizar operaciones de movimiento predeterminadas cuando sea posible, o implementar una copia real si es necesaria. Inicializar todos los miembros con valores definidos y revisar el `noexcept` del constructor de `SuperGraph`, que realiza asignaciones de memoria susceptibles de lanzar excepciones.

Referencias: [SuperGraph.cpp](mipSolver/src/SuperGraph.cpp), líneas 3–71; [Graph.cpp](mipSolver/src/Graph.cpp), línea 3.

### 5. Gestionar la vida útil de los recursos de CPLEX

**Prioridad: media; especialmente relevante para resolver varias instancias en un proceso.**

El destructor de `RCPPSolver` está vacío y no hay una llamada a `_env.end()`. La inicialización del entorno y de los objetos asociados también se realiza mediante asignaciones dentro del cuerpo del constructor.

**Mejora propuesta:** definir claramente la propiedad del entorno, inicializar los miembros en el orden adecuado y garantizar la liberación de recursos tanto en la salida normal como ante excepciones. Evitar copias accidentales de un objeto que administra esos recursos.

Referencia: [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 3–18.

### 6. Conservar la precisión en Dijkstra

**Prioridad: media; la función no se utiliza en el flujo actual.**

`calculate_depo_dists()` utiliza prioridades `long long`, aunque las distancias son `double`. Si los costos tienen decimales, la conversión trunca las prioridades y puede procesar nodos en un orden incorrecto. Combinado con `visited`, esto puede generar distancias incorrectas.

El valor `1e9` también funciona como una cota arbitraria para representar distancias desconocidas.

**Mejora propuesta:** utilizar una cola mínima con prioridades `double` e infinito real como distancia inicial. Probar caminos alternativos cuyos costos difieran en menos de una unidad.

Referencia: [SuperGraph.cpp](mipSolver/src/SuperGraph.cpp), líneas 259–284.

## Rendimiento y uso de memoria

### 7. Representar las variables por arco y vehículo

`_X`, `_Y` y `_F` reservan estructuras de tamaño cuadrático en la cantidad de nodos virtuales, aunque solo algunas parejas de nodos tienen conexión. No se crean variables para todas esas parejas, pero sí se reserva la estructura intermedia cuadrática.

**Mejora propuesta:** indexar las variables como `[arc_id][vehículo]`. Crear variables de servicio solo para arcos requeridos y vehículos habilitados, y variables de depósito únicamente para nodos adyacentes. Esto también evitaría reservar posiciones para el vehículo `0`, que no participa en los bucles de resolución.

Referencia: [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 104–155.

### 8. Construir las conexiones de giro por intersección

Actualmente se compara cada arco contra todos los demás para identificar conexiones posibles.

**Mejora propuesta:** agrupar los arcos entrantes y salientes por nodo original y comparar únicamente los que comparten una intersección. El trabajo pasaría a depender de las combinaciones locales relevantes, aunque una intersección de grado alto todavía puede requerir muchas conexiones.

Referencia: [SuperGraph.cpp](mipSolver/src/SuperGraph.cpp), líneas 83–116.

### 9. Simplificar las adyacencias y evitar asignaciones repetidas

Los nodos virtuales tienen índices consecutivos, pero sus adyacencias se almacenan mediante un wrapper de GLib. Además, cada consulta construye un nuevo vector de punteros y esas consultas se repiten durante la generación de restricciones.

**Mejora propuesta:** evaluar `vector<vector<int>>` para almacenar identificadores de arcos por nodo y consultar esas listas por referencia. Esto podría eliminar la dependencia de GLib, dado su uso actual en el proyecto.

Los nombres `in` y `out` están invertidos respecto de la dirección de los arcos: `_node_to_super_in` registra arcos que salen del nodo y `_node_to_super_out`, arcos que entran. Corregir los nombres facilitaría revisar las restricciones sin cambiar su comportamiento.

Referencias: [HashMap.h](mipSolver/lib/HashMap.h); [HashMap.cpp](mipSolver/src/HashMap.cpp); [SuperGraph.cpp](mipSolver/src/SuperGraph.cpp), líneas 159–167 y 219–244.

## Diseño y mantenibilidad

### 10. Separar lectura, representación, modelo y exportación

El archivo del grafo se abre y se interpreta parcialmente dos veces: una para obtener la cantidad de vehículos y otra dentro de `Graph`.

**Mejora propuesta:** leer una sola vez y construir una estructura de instancia validada. Mantener separadas la transformación a supergrafo, la construcción del modelo, la resolución y la exportación. Esto permitiría probar buena parte del proyecto sin CPLEX.

Hacer configurables la capacidad y la ruta de salida, hoy fijadas en `10000` y `out.dat`. Revisar también la cota de `_Y`: se usa la capacidad de carga como límite de cantidad de recorridos, aunque representan magnitudes distintas.

Referencias: [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 20–36 y 134–136; [RCPPSolver.h](mipSolver/lib/RCPPSolver.h), líneas 55–59.

### 11. Ordenar las interfaces y convenciones

- Eliminar el include circular entre `Graph.h` y `SuperGraph.h` mediante declaraciones adelantadas e includes directos de los tipos utilizados.
- Evitar `using namespace std` en headers públicos.
- Incluir explícitamente los headers estándar necesarios, sin depender de includes transitivos.
- Reemplazar los valores especiales de tipo de arco y estado por constantes con nombre o enumeraciones.
- Usar `double` consistentemente para costos y demandas, evitando perder precisión al leerlos o pasarlos a constructores con parámetros `float`.
- Unificar el idioma y corregir nombres como `depoist` y `generar_variables` según la convención elegida.
- Eliminar miembros y alias sin uso, como `_Z`, y código comentado que ya no aporte contexto.
- Redondear de forma controlada los valores de variables enteras al exportarlos, en lugar de truncar directamente un `double` a `int`.

Referencias: [Graph.h](mipSolver/lib/Graph.h); [SuperGraph.h](mipSolver/lib/SuperGraph.h); [Edges.h](mipSolver/lib/Edges.h); [RCPPSolver.h](mipSolver/lib/RCPPSolver.h); [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 411–429.

## Supuestos del modelo que requieren documentación

### 12. Demandas cero y conectividad

Este punto se refiere a la demanda, no al costo del tramo.

Si se permiten tramos requeridos con demanda cero, las restricciones de flujo de demanda no impiden por sí solas atender un ciclo desconectado del depósito: ese ciclo puede satisfacer continuidad y servicio sin consumir flujo.

**Mejora propuesta:** documentar y validar que las demandas requeridas sean estrictamente positivas si esa es una condición del problema. Si se permiten demandas cero, revisar cómo se garantiza la conectividad de los servicios con el depósito.

Referencia: [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 164–323.

### 13. Significado de los giros y las zonas

El vector `turns` se lee y ordena, pero no se utiliza al construir el supergrafo. Solo se consultan los giros prohibidos.

**Mejora propuesta:** documentar qué representa esa primera lista y determinar si debe influir en las conexiones o si es información redundante. Explicitar también el significado de zona `0`, zona `-1` y zonas positivas, junto con la relación entre zonas, viajes y vehículos.

Revisar que `mipModel.md` describa la formulación implementada, incluidas esas asignaciones y las convenciones de índices.

Referencias: [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 39–66 y 164–190; [SuperGraph.cpp](mipSolver/src/SuperGraph.cpp), líneas 83–104; [mipModel.md](mipModel.md).

## Pruebas, compilación y documentación

### 14. Incorporar pruebas con resultados comprobables

`make test` solo ejecuta el programa sin argumentos: se imprime la ayuda y se termina con código exitoso. No se comprueba la lectura, la construcción del supergrafo ni una solución del modelo.

Casos sugeridos:

- Grafo mixto con aristas y arcos requeridos, verificando sus identificadores.
- Giros prohibidos y restricciones de retorno en U.
- Instancia pequeña con recorrido y objetivo conocidos.
- Instancia infactible y capacidad insuficiente.
- Servicio asignado a una zona y servicio libre con zona `-1`.
- Contenido de entrada inválido, si no se garantiza su validez externamente.
- Costos positivos con decimales para el cálculo de distancias.
- Copia o movimiento de estructuras, según la interfaz que se decida admitir.

Separar las pruebas de estructuras y lectura de las pruebas de integración que necesitan CPLEX. Para estas últimas, comprobar también que los recorridos cumplen servicio, conectividad, capacidad y giros permitidos.

Referencia: [Makefile](Makefile), líneas 38–40.

### 15. Mejorar el soporte de desarrollo y la documentación

- Habilitar advertencias del compilador para el código propio.
- Ofrecer una configuración de depuración; actualmente se define `NDEBUG` y se utiliza optimización `-O3` por defecto.
- Documentar dependencias, compilación y parámetros de ejecución, incluido `cutsMode`, que no aparece en la ayuda.
- Explicar el formato de ambos archivos de entrada y del archivo de salida.
- Incluir una instancia mínima reproducible y su resultado esperado.
- Registrar estado de resolución, objetivo, mejor cota y gap cuando estén disponibles; actualmente la cota y el gap se calculan pero no se muestran.

Referencias: [Makefile](Makefile); [README.md](README.md); [main.cpp](main.cpp), línea 7; [RCPPSolver.cpp](mipSolver/src/RCPPSolver.cpp), líneas 364–370.

## Verificación realizada y orden sugerido

Se comprobaron todos los archivos `.cpp` con compilación sintáctica y `-Wall -Wextra -Wpedantic`. No hubo errores de compilación. Aparecieron advertencias del código propio por variables y parámetros sin uso, llamadas no calificadas a `move` y comparaciones entre tipos con y sin signo; también apareció una advertencia dentro de los headers de CPLEX.

No se ejecutaron pruebas numéricas: el repositorio no incluye instancias de prueba. Los problemas potenciales del modelo quedan sujetos a los supuestos de entrada indicados.

Orden sugerido:

1. Corregir el manejo de resultados de CPLEX, los identificadores y la vida útil de los objetos.
2. Definir los contratos de entrada y agregar pruebas pequeñas de regresión.
3. Cambiar la representación de variables y optimizar la construcción del supergrafo.
4. Completar la separación de responsabilidades, las convenciones y la documentación.
