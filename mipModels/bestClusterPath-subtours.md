# Subtours en el primer MIP: tres opciones para conservar los cruces

Revisión del código: 2026-11-11. Son propuestas; no se modificó el solver.

**Recomiendo descartar las componentes desconectadas si sólo contienen
recorridos internos sin servicio ni cruces (opción 1).** No aportan información
al resultado que interesa conservar. Si contienen servicio, hace falta resolver
la conectividad, como en la opción 3.

## Diagnóstico del modelo actual

[`BestClusterPathSolver`](../mipBestClusterPathSolver/src/model/BestClusterPathSolver.cpp)
hereda las restricciones del RCPP y minimiza:

$$
\min C=\sum_{a\in A_{io}}Y_a.
$$

$A_{io}$ contiene los SuperArcs neutrales dirigidos. Se cuentan **ocurrencias
de cruces**, no cruces distintos. Los recorridos internos y giros tienen
coeficiente cero: agregarles un ciclo balanceado puede no cambiar el objetivo.

El [flujo de demanda](../mipBestPathSolver/src/constraints/FlowConstraintSetter.cpp)
conecta los servicios de demanda positiva al depósito, pero no obliga a conectar
recorridos sin servicio ni servicios de demanda cero. Al sumar sus ecuaciones
sobre una componente desconectada, su demanda servida total debe ser cero.

Actualmente,
[`SolutionEulerCircuit`](../mipBestPathSolver/src/model/SolutionEulerCircuit.cpp)
**rechaza cualquier componente desconectada**, también si sólo contiene `Y`.
La versión anterior de este documento decía que ya se descartaban: eso no
coincide con el código ni con las regresiones actuales.

## Información mínima que conviene conservar

Una fila por SuperArc neutral usado:

```text
global_arc_id | cluster_origen | cluster_destino | from_virtual | to_virtual | cantidad
```

Conservar dirección y multiplicidad. No agrupar únicamente por `edge_id`:
varios SuperArcs pueden compartir el registro neutral y representar cruces
con orientaciones distintas. Para reutilizar IDs virtuales fuera de la ejecución,
conservar también la instancia y su mapeo.

Para un reporte basta leer los `Y > 0` de `get_neutral_arc_ids()`, sin ordenar
el recorrido. Eso evita depender de Hierholzer para exportar, pero no demuestra
que los cruces pertenezcan a un recorrido válido: primero hay que clasificar
las componentes.

Si después se continúa con los subproblemas y la fusión actuales, se necesitan
además las visitas: pareja entrada/salida y servicios asignados. Dos entradas
y dos salidas de un cluster pueden emparejarse de distintas formas; algunas
pueden generar circuitos separados. Se puede exportar sólo cruces y conservar
las visitas como información auxiliar interna. Reemplazar las visitas por
fronteras agregadas requeriría rediseñar esa segunda etapa.

## Opción 1 — Descartar componentes internas sin servicio

**Idea:** conservar la componente del depósito y eliminar sólo las componentes
que no tengan servicio ni cruces.

1. Construir el soporte positivo de `X`, `Y` y conectores del depósito sobre los
   nodos virtuales del `SuperGraph`.
2. Validar balances y separar componentes, ignorando la dirección sólo para
   esta clasificación. Con balance dirigido, cada componente conexa del soporte
   admite un circuito de Euler.
3. En cada componente fuera del depósito, exigir que no haya `X > 0`, ningún
   SuperArc neutral usado ni conectores del depósito.
4. Eliminar todos sus `Y`. Si se conserva una `Solution` con flujo, poner también
   en cero el flujo de esos arcos; sin servicio sólo puede circular.
5. Verificar servicio completo, balances y el mismo vector `Y[a]` para todos
   los arcos neutrales. Ejecutar el extractor estricto sobre la solución limpia.

Borrar una componente balanceada sin servicio no afecta los balances del resto
ni las restricciones de servicio. Como no contiene cruces, conserva tanto su
identidad como su multiplicidad y el objetivo.

**Ventajas:** no agrega variables ni otra resolución. Clasificar el soporte
cuesta $O(|V|+|A^+|)$ sin expandir multiplicidades. Es la opción más directa
cuando no interesa conservar el interior.

**Límites:** no borrar un servicio, aunque su demanda sea cero. Tampoco borrar
silenciosamente una componente con cruces: cambiaría la selección solicitada.
Una componente formada sólo por `Y` que incluya cruces sería eliminable con
mejora de $C$, así que no corresponde a un óptimo exacto del modelo actual;
podría aparecer en un incumbente con gap o límite de tiempo. Registrar ese caso
y revisar el resultado del solver.

**Implementación sugerida:** preprocesamiento específico de BestClusterPath,
entre `extract_solution()` y `ClusterBoundaryExtractor` en
[`main.cpp`](../mipBestClusterPathSolver/src/main.cpp). Mantener estricto el
extractor compartido evita cambiar la validación de otros consumidores.

## Opción 2 — Desempatar minimizando recorridos

**Idea:** primero minimizar cruces y después quitar la degeneración entre
soluciones con la misma cantidad de cruces.

1. Resolver el objetivo actual y obtener $C^*$.
2. Agregar $\sum_{a\in A_{io}}Y_a=C^*$.
3. Minimizar $\sum_{a\in A}Y_a$, incluyendo todos los conectores de giro.
   Se pueden sumar también los recorridos del depósito, con sus variables propias.

Cada `Y` debe tener coeficiente secundario estrictamente positivo. Minimizar
sólo distancia física dejaría gratis los ciclos de conectores de costo cero.
Resolver por etapas evita elegir un epsilon que altere la prioridad de cruces.

Si se quiere conservar **exactamente la selección del primer incumbente**, fijar
además $Y_a=\bar Y_a$ para cada arco neutral, incluidos los ceros. Fijar sólo
$C^*$ permite elegir otros cruces con la misma cantidad total.

**Ventajas:** en un óptimo del segundo objetivo no quedan componentes puramente
internas de `Y`: borrarlas mantendría los cruces y mejoraría ese objetivo. No
necesita nuevas variables de conectividad.

**Límites:** agrega una segunda optimización. No obliga a conectar servicios de
demanda cero. Tampoco garantiza conectar componentes con cruces que se hayan
fijado. Si la segunda resolución termina anticipadamente, todavía pueden quedar
ciclos internos; validar siempre y aplicar la opción 1 cuando corresponda.

La garantía de mínimo global de cruces requiere certificar la primera etapa.
Si se fija un incumbente sin esa certificación, se está desempatando ese valor,
no demostrando el mínimo global. El CLI llama a `solve(0.01)`; distinguir la
tolerancia de parada de una prueba de óptimo exacto.

## Opción 3 — Cortes de conectividad mediante callbacks, como en TSP

**Sí: se puede separar subtours durante la resolución usando lazy constraints.**
Es una alternativa al flujo auxiliar y permite mantener el objetivo original.
CPLEX ofrece el contexto `Candidate` y `rejectCandidate` para rechazar una
solución candidata agregando las restricciones violadas
([documentación de IBM](https://www.ibm.com/docs/en/icos/22.1.2?topic=c-callbackcontext)).

Procedimiento propuesto:

1. Cuando CPLEX presenta una solución candidata entera, leer `X`, `Y` y los
   recorridos del depósito; no usar la instantánea de un `SolveResult` anterior.
2. Construir el soporte positivo sobre los nodos virtuales del `SuperGraph`.
3. Detectar componentes desconectadas del depósito.
4. Agregar cortes que obliguen a conectar cada componente si continúa utilizada,
   y rechazar el candidato. CPLEX sigue buscando dentro de la misma resolución.
5. Repetir en los candidatos posteriores y validar la solución final.

La diferencia con TSP es que acá se pueden repetir calles y hay nodos/arcos
opcionales. No corresponde copiar el corte de cardinalidad interna del TSP
ni exigir visitar para siempre una componente opcional observada una vez.

### Corte que permite conectar o eliminar una componente

Sea $S$ una componente sin depósito, $\delta^-(S)$ el conjunto de **todos los
arcos legales del modelo** que entran a $S$, aunque actualmente valgan cero,
y $t_a=X_a+Y_a$. Donde no existe `X`, tomarla como cero; para arcos del depósito,
usar su variable de recorrido correspondiente.

Para un arco interno $b$ utilizado en el candidato, con cota válida
$0\le t_b\le U_b$, agregar:

$$
U_b\sum_{a\in\delta^-(S)}t_a\ge t_b.
$$

En el candidato desconectado, el lado izquierdo es cero y $t_b>0$, por lo que
el corte lo rechaza. En una solución futura:

- Si se sigue usando $b$, la suma entera de entradas debe ser al menos uno.
- Si se elimina $b$, el corte permite no visitar esa componente. Si quedan
  otros arcos desconectados, el callback volverá a detectarlos y separarlos.

Se pueden agregar cortes para varios arcos internos en una misma llamada.
El balance del recorrido obliga también a salir de $S$. La cota $U_b$ debe
incluir servicio y recorridos cuando ambas variables existen; no usar una
constante arbitraria. Esta formulación evita agregar binarias de activación,
aunque puede ser débil en la relajación fraccionaria.

Si $S$ contiene un servicio que necesariamente debe realizarse íntegramente
allí, se puede usar el corte más fuerte:

$$
\sum_{a\in\delta^-(S)}t_a\ge1.
$$

Esto requiere justificar la obligatoriedad de visitar $S$ en cualquier solución,
considerando también la orientación alternativa de una arista requerida.
Observar un arco `X` en el candidato no basta por sí solo para esa justificación.

**Ventajas:** cubre servicio de demanda cero y componentes puramente `Y`,
agrega restricciones sólo cuando hacen falta y no necesita una segunda
optimización ni un flujo auxiliar por arco. Es la opción que elegiría si se
quiere que el propio primer MIP entregue una solución conectada.

**Costo y límites:** requiere implementar y medir la separación. No garantiza
ser más rápido que el flujo auxiliar. El callback debe comprobar todos los
candidatos pertinentes, aunque una componente ya haya aparecido antes: no
suponer que CPLEX conservará todos los cortes añadidos. Usar lazy constraints
para imponer factibilidad; los user cuts por sí solos no sustituyen ese control.

**Efecto sobre los cruces:** conserva el objetivo de minimizarlos, pero puede
cambiar la selección de un candidato inválido. El solver puede borrar un ciclo
opcional o conectarlo. Si se fija el vector previo de cruces, puede no existir
una solución conectada que lo respete. Los cruces a exportar son los del
incumbente final validado.

**Implementación sugerida:** callback específico de `BestClusterPathSolver`,
registrado en el `IloCplex` que administra `CPLEXSolver`. El wrapper actual no
contiene callbacks; habría que agregar ese punto de extensión. Mantener
`SolutionEulerCircuit` como validación posterior.

El flujo auxiliar descrito inicialmente sigue siendo otra forma de imponer
conectividad: cada nodo utilizado consume una unidad proveniente del depósito.
Los callbacks evitan agregar de entrada esas variables y separan los cortes
necesarios durante la búsqueda.

## Elección y comprobaciones

| Opción | Conserva exactamente los cruces actuales | Servicio desconectado de demanda cero | Esfuerzo estimado |
| --- | --- | --- | --- |
| 1. Limpiar componentes internas | Sí, por condición de aceptación | Lo detecta y rechaza | Bajo |
| 2. Desempatar recorridos | Sólo fijando cada `Y` neutral | No lo resuelve en general | Medio |
| 3. Callbacks con cortes | Sólo si se fijan y siguen siendo factibles | Lo obliga a conectarse | Mayor |

Empezaría clasificando los sobrantes y aplicaría la **opción 1** si todos son
internos sin servicio. Usaría la **opción 3** para exigir conectividad dentro del MIP, incluido
el servicio de demanda cero. La opción 2 sirve para obtener soluciones menos degeneradas desde
el solver, a cambio de otra resolución.

Al implementar, probar: solución ya conectada; uno y varios ciclos internos `Y`;
componente con `X` de demanda cero; componente con cruces; y distintos SuperArcs
neutrales del mismo `edge_id`. En la opción 1 comparar todo el vector de cruces
antes/después, no sólo su suma. Si se continúa con los subproblemas, comprobar
además visitas, servicio, giros y conectividad de la fusión.
