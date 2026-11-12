# Spec: descartar y reconectar subtours del primer MIP de BestClusterPath

Estado: propuesta de implementación. Revisión: 2026-11-12.

Parte de la [opción 1 del análisis de subtours](bestClusterPath-subtours.md#opción-1--descartar-componentes-internas-sin-servicio)
y la amplía con reconexiones provisionales y reasignación de servicios.
No implica que el cambio ya esté implementado o probado.

## 1. Alcance y criterio de implementación

Después del primer MIP global, descartar interiores desconectados sin cruces
y reconectar los que contienen cruces, sin aumentar las ocurrencias neutrales.
Los servicios requeridos se conservan en los subproblemas, aunque se descarte
su recorrido inicial. La salida física final debe ser legal y estar conectada.

**Implementar la solución más simple que cumpla ese comportamiento.** Reutilizar
los tipos, recorridos, mapas y controles existentes. No crear una infraestructura
genérica, nuevas opciones de CLI ni un certificador de soluciones. No agregar
validaciones que ya hagan los lectores, el solver, el extractor, el merger o
las contracciones. Una propiedad debe comprobarse en la etapa que la necesita,
sin repetir el mismo control en todas las etapas.

La entrada es la solución producida por el solver sobre los grafos de esta
misma ejecución. No ampliar el contrato para aceptar entradas arbitrarias ni
agregar comprobaciones de IDs, metadatos, duplicados, vehículos, servicio o
flujo que ya estén garantizadas por esa procedencia. Las fixtures de prueba
deben respetar ese formato.

No cambiar el MIP global, agregar otra resolución global ni modificar las
prohibiciones o las reglas de contracción. Adaptar sólo lo necesario en la
extracción de visitas, construcción de subproblemas y fusión para consumir
el resultado provisional. No modificar los solvers comunes para esta reparación.

## 2. Integración y datos mínimos

Agregar `SubtourBreaker.h` y `SubtourBreaker.cpp` en `lib/model/` y `src/model/`
de `mipBestClusterPathSolver`, sin dependencia directa de CPLEX. Integrarlo en
[`main.cpp`](../mipBestClusterPathSolver/src/main.cpp) después de
`result.extract_solution()` y antes de extraer las visitas.

`SubtourBreaker` debe ser una clase que encapsule la reparación y sea dueña de
su resultado. El constructor recibe los grafos de esta ejecución, la solución
extraída y el vector completo de IDs neutrales, y realiza la reparación. No
exponer una función libre `break_subtours` ni un agregado público
`SubtourRepair`. La integración debe usar el objeto y luego `get_solution()`:

```cpp
const SubtourBreaker subtour_breaker(super_graph, graph,
    result.extract_solution(), solver.get_neutral_arc_ids());
const Solution& global_solution = subtour_breaker.get_solution();
```

`get_solution()` devuelve una referencia constante al soporte reparado, válida
mientras viva el objeto. `get_connections()` expone por referencia constante
las conexiones provisionales y `was_repaired()` informa si hubo reconexión,
para combinar esa marca con la reasignación de servicios al informar optimalidad.
El estado de la reparación permanece privado y sólo necesita:

- El soporte real modificado, reutilizando `Solution`.
- Las conexiones provisionales que no son SuperArcs reales.

Una conexión provisional necesita extremos virtuales y multiplicidad. Su
posición en el vector puede identificarla; el cluster se obtiene de los mapas
existentes. No exigir un ID independiente, otro catálogo ni datos duplicados.
Distinguir enlaces reales y provisionales al extraer el circuito, sin inventar
IDs de SuperArc ni cambiar los extremos de un arco real. No agregarlos al
`SuperGraph` ni a los archivos de salida.

Los servicios por reasignar se deducen en el constructor de subproblemas de los
registros requeridos que aún no pertenecen a una visita. No mantener otra lista
persistente de pendientes ni otro conjunto de servicios si se puede reutilizar
la asignación que ya se construye allí. El grafo expandido es la referencia
para identificar registros originales, copias opcionales y clusters.

Los helpers concretos se eligen al implementar, con la menor API necesaria.
`SolveResult` conserva la solución original. Los consumidores de la
variante deben usar el mismo soporte reparado y sus conexiones provisionales.

`F` y `deposit_flow` se ignoran: no reconstruirlos, validarlos ni limpiarlos para
este paso. Pueden permanecer como datos heredados de la copia; el soporte
provisional no certifica su factibilidad de flujo ni se exporta como solución.
La solución física final se construye con el pipeline local existente.

## 3. Descarte y reconexión

### Soporte y componentes

Construir el soporte positivo de `X + Y`, conectores del depósito y conexiones
provisionales. Usar multiplicidades enteras y adyacencia débil, sin expandir
ocurrencias para clasificar componentes. Normalizar `-1` como depósito al
construir esa adyacencia. `F` no crea conexiones.

Identificar los cruces mediante el vector completo de
`BestClusterPathSolver::get_neutral_arc_ids()`. No agrupar por `edge_id`: varios
SuperArcs pueden representar orientaciones distintas del mismo registro.

Reutilizar el cálculo de componentes para identificar la del depósito y las
pendientes; no hacer otra pasada de conectividad sólo para confirmar lo mismo.
No buscar ciclos dentro de la componente del depósito para eliminarlos por
separado, aunque pueda participar en una reconexión.

### Componente sin cruces

Descartar todo su soporte interno: poner en cero sus usos reales `X` e `Y` y
retirar sus conexiones provisionales. Esto no elimina los registros requeridos
del grafo; sus servicios se asignarán a visitas en §4. Al retirar una componente
completa balanceada no se altera el balance del resto.

Incluir calles, copias opcionales y conectores de giro. Conservar los metadatos
y la numeración de los arcos reales. No imponer un formato nuevo ni reglas de
serialización para este resultado interno.

### Componente con cruces

Buscar una reconexión completa que reduzca al menos una ocurrencia neutral,
sin agregar cruces, tocar `X`, modificar conectores del depósito ni crear giros
en U. Los enlaces nuevos deben quedar dentro del mismo cluster. Si no existe
un SuperArc directo legal, pueden representar un camino provisional a resolver
por el solver local.

Definir todos los decrementos e incrementos de la operación. Por ejemplo,
retirar `d` ocurrencias de `u -> v` y `p -> q` y agregar `d` de `u -> q` y
`p -> v` conserva los balances. Los enlaces retirados deben tener multiplicidad
suficiente; los reales se decrementan en `Y`. También pueden consumirse enlaces
provisionales anteriores. Al menos un decremento debe retirar un cruce neutral
de la componente tratada.

Este intercambio es un ejemplo de operación admisible, no una garantía de que
siempre exista. Restringir la búsqueda a candidatos compatibles con las
orientaciones, el cluster y la regla sin U. Preferir una operación que conserve
balances por construcción, sin volver a calcular todos los balances para cada
candidato. Se permite desbalance entre instrucciones internas, pero al terminar
cada operación el soporte debe quedar balanceado y con valores no negativos.

Actualizar las multiplicidades y reclasificar. Descartar los nuevos fragmentos
sin cruces y continuar con los demás. La disminución estricta de cruces asegura
progreso: no agregar detección de estados repetidos ni una segunda condición de
terminación. La búsqueda de candidatos debe recorrer un conjunto finito; si no
encuentra una reconexión admisible, fallar con un mensaje simple.

No prometer tiempo lineal para toda la reparación: la búsqueda y las
reclasificaciones tienen costo adicional. Evitar búsquedas lineales por ID de
arco cuando ya existe un índice y no expandir multiplicidades en esta etapa.

## 4. Visitas y reconstrucción local

El extractor ordena el soporte real más sus enlaces provisionales y obtiene
las visitas con sus portales y servicios activos. Debe usar una extracción
euleriana estricta: antes de pasar a subproblemas, todo uso positivo restante
debe pertenecer al circuito balanceado del depósito.

Reutilizar la lógica de extracción existente con la adaptación mínima para
reconocer enlaces provisionales; no duplicar Hierholzer ni agregar un modo que
ignore subtours. Mantener el contrato estricto de
[`SolutionEulerCircuit`](../mipBestPathSolver/src/model/SolutionEulerCircuit.cpp)
para sus consumidores actuales. Sus controles de continuidad y conexión bastan:
no agregar otro certificador previo o posterior de esas mismas propiedades.

En el constructor de subproblemas, asignar cada registro requerido sin visita
a la primera visita activa compatible de su cluster, usando el orden existente.
No agregar una heurística de asignación, búsqueda de alternativas ni pruebas de
factibilidad por cada candidato. Si no hay visita compatible, fallar. La demanda
cero no cambia esta regla. Las copias opcionales no generan otro servicio.

Una visita con servicios asignados o conexiones provisionales siempre se
optimiza, incluso si antes era passthrough. Sólo conservar directamente los
passthrough que no necesiten reconstrucción bajo las reglas actuales.

El solver local reconstruye el interior de la visita entre sus portales reales
y sirve sus registros asignados. Las conexiones provisionales no se agregan
como atajos al grafo local ni obligan a conservar su recorrido interno literal.
La sustitución se resuelve por visita, aprovechando el problema local existente;
no agregar variables o restricciones para materializar cada enlace por separado.
Si el problema local no tiene solución, mantener el fallo actual del pipeline.

La fusión recibe únicamente recorridos locales reales y cruces supervivientes.
Nunca copiar un enlace provisional como arco físico. Reutilizar los controles
actuales del merger y de las contracciones para servicio, continuidad, conexión
y prohibiciones; no repetirlos en `SubtourBreaker`. Una solución sin visitas
que deje servicios sin asignar falla por la misma regla de asignación.

## 5. Objetivo, errores y salida

Actualizar el objetivo del soporte a la suma de `Y` sobre los IDs neutrales que
quedan. Las operaciones sólo pueden reducir esa suma. Mantener las claves
existentes de `BEST_CLUSTER_PATH_RESULT`: `objective`/`crossings` son los cruces
reparados y `distance` la distancia física fusionada.

No agregar estadísticas detalladas de arcos, pasadas o componentes descartadas,
ni lógica para reprocesar el resultado y comprobar idempotencia. Para diagnóstico
basta informar los cruces antes y después, reutilizando los valores disponibles.

Si hubo reconexión o reasignación de servicios, informar `optimal=false`; basta
una marca de reparación. Sin esos cambios, conservar el criterio actual de
estados global y locales. No ejecutar otra optimización para certificarlo.

Reutilizar excepciones, mensajes y códigos del CLI: código 1 para no poder
reparar o asignar un servicio; código 2 para una resolución sin solución.
Agregar sólo contexto útil al fallo nuevo: cruce o componente tratada, o registro
y cluster del servicio sin visita. No crear una jerarquía nueva de errores ni
exigir un esquema de diagnóstico para todos los casos de entrada inválida.
No exportar un soporte provisional ni escribir nuevas salidas si falla el pipeline.

## 6. Pruebas necesarias

Extender las pruebas actuales en
[`best_cluster_path_solver_test.cpp`](../tests/best_cluster_path_solver_test.cpp)
y la cobertura del pipeline en `tests/best_cluster_pipeline_test.py`.
Usar fixtures pequeñas y válidas con SuperArcs reales; representar los enlaces
provisionales con el mecanismo nuevo. No depender de que CPLEX produzca
espontáneamente un subtour. Agrupar los casos que puedan compartir una fixture.

Cubrir el comportamiento nuevo, sin crear una batería de validación de entradas
ni repetir pruebas de lectores, solver, merger o contracción:

| Caso | Resultado requerido |
| --- | --- |
| Solución conectada y ciclo interno conectado al depósito | Sin cambios por la reparación. |
| Componente desconectada sin cruces | Se retira todo el interior, incluidos giros y multiplicidades mayores que uno. |
| Servicio en un interior descartado | Se asigna a una visita del mismo cluster y se sirve una vez; incluir demanda cero y una visita antes passthrough. |
| Componente con cruces reconectable | La operación conserva balances, reduce cruces y permite reclasificar los fragmentos resultantes. |
| No hay reconexión o no hay visita compatible para un servicio | Fallo con el código correspondiente y sin salida nueva. |
| Conexión provisional sin SuperArc directo legal | El extractor la procesa y el solver local reconstruye un recorrido legal, incluso en una visita sin servicio previo. |
| Reconstrucción local infactible | Mantener la salida de resolución sin solución; no fusionar ni exportar. |

En un caso completo de reparación, verificar servicio único, conservación del
vector neutral reparado al fusionar y ausencia de enlaces provisionales en la
salida. Aprovechar las comprobaciones existentes del pipeline para legalidad,
contracción y sorter. Conservar las regresiones del extractor estricto.
No exigir contadores, copias de todos los vectores ni tests de helpers que sólo
repitan su implementación.

Validación para la implementación, desde la raíz:

```sh
make mip-best-cluster-tests mip-best-cluster
./build/best_cluster_path_solver_test
make test-unit
make test
```

Los targets de compilación no ejecutan sus tests. Los `.cpp` nuevos del módulo
entran por el wildcard actual del Makefile; reutilizar el binario de pruebas
existente en lugar de agregar otro sin necesidad.

## 7. Regresión con mapas y aceptación

Reutilizar las entradas normalizadas de
`experiments/cluster_crossings/20261110_185407_988880/{martinezA,martinezB}/inputs/`
y los grafos y giros `data/{martinezA,martinezB}/streets/graph_3525.*`.
Guardar logs y salidas en un directorio nuevo de experimentos.

La referencia en
[`bestClusterPath.md`](bestClusterPath.md#validación-de-las-copias-físicas-2026-11-11)
es martinezA con 114 cruces y distancia `26045.801270907708`, y martinezB con
objetivo global diagnosticado 50 y rechazo del recorrido desconectado
`id=153`, `306 -> 307`. Intentar completar ambos pipelines; si no hay reparación
o solución local admisible, registrar el fallo sin ampliar el algoritmo.

Comparar cruces antes y después dentro de cada ejecución, sin exigir reproducir
exactamente un incumbente histórico con gap `0.01`. La aceptación requiere las
pruebas anteriores y una salida final legal, conectada, con todos los servicios
y sin aumentar los cruces. Actualizar `bestClusterPath.md` y
`bestClusterPath-merge.md` al implementar y documentar sólo lo realmente ejecutado.
