# RCPP con cambios mínimos entre clusters

El ejecutable resuelve dos niveles desde el mismo `main`:

1. `BestClusterPathSolver` minimiza los cambios entre clusters.
2. `ClusterRCPPSolver` fija esas fronteras y minimiza la distancia interna de
   cada cluster.

## Objetivo global

Sea $A_{io}$ el conjunto de transiciones sintéticas entre copias de clusters.
La variable $Y_a$ indica la cantidad de usos de la transición $a$ por el único
camión.

$$
\min \sum_{a\in A_{io}}Y_a.
$$

Las variables de servicio, las aristas físicas, los giros y los conectores del
depósito no forman parte de este primer objetivo.

## Expansión en memoria

El contrato del ejecutable es:

```sh
bestClusterPathExec input.dat curvas.dat --clusters graph-clusters.dat \
  [--neutral-nodes neutral-nodes.dat] [--output salida.dat]
```

`Instance(graph_path, turns_path, clusters_path, neutral_path)` delega la lectura
a `BestClusterPathInstanceReader`, que compone `InstanceReader` y
`ClusterAssignmentReader`. Los lectores viven en el módulo común. El archivo de
clusters usa la cabecera `arista cluster origen destino vehiculo servicio
recorridos pasadas`, con una fila por registro, clusters positivos y las cuatro
columnas de solución en cero. Se admiten los comentarios del generador. El
archivo de neutros usa `node_id reasons` y nodos originales únicos, desde uno.
La lectura convierte IDs a índices desde cero una sola vez.

El único grafo global es `NeutralExpandedGraph`, del módulo común sin CPLEX.
De él se construye un único `SuperGraph` global. Los grafos locales son
independientes y contienen exclusivamente registros físicos de su cluster.

Antes de expandir se calcula $S_k$, suma de costos de cada registro original del
cluster $k$, contando cada arista no dirigida una sola vez. Cada nodo neutro
conserva una representación por cluster incidente. Entre cada par distinto de
representaciones se agregan transiciones dirigidas de cluster cero, demanda
cero y costo $S_{c_1}+S_{c_2}$. Se admiten tres o más clusters por nodo.

Cada registro físico original conserva su cluster y servicio. Si ambos extremos
son neutros, se agrega una copia en cada otro cluster que tenga representación
de ambos extremos. Cada copia conserva costo, dirección y carácter dirigido o
no dirigido, pero es opcional, de zona cero y demanda cero. Los registros
paralelos mantienen identidades independientes. Las copias son calles físicas:
no pertenecen al conjunto de transiciones neutrales y usarlas dentro de un
cluster no suma cruces. Al contraer se acumulan sus recorridos sobre el registro
original; el servicio sigue exigiéndose una sola vez.

Las prohibiciones originales se expanden también a las copias. Los cruces
conectan directamente las orientaciones físicas de llegada y salida y respetan
los giros prohibidos y los retornos en U entre representaciones. Todos los
SuperArcs neutrales aportan coeficiente uno en su variable $Y$, aunque varios
correspondan al mismo registro sintético. Sus costos no intervienen en ese
objetivo ni en la distancia final.
No se consumen archivos `.tuned.*`, `in-out`, `arc-clusters` ni `mapping`.

## Optimización interna

Después de extraer la solución global se aplica `SubtourBreaker` (ver la
[spec de opción 1](bestClusterPath-subtours-option1-spec.md)). Clasifica el
soporte positivo de `X`, `Y` y depósito sin expandir multiplicidades, y retira
las componentes desconectadas sin cruces, incluidos sus servicios iniciales.
Los registros requeridos permanecen en el grafo.

Para una componente con cruces busca un intercambio de dos cruces con
orientaciones opuestas de clusters: decrementa ambos `Y` por la menor
multiplicidad y agrega dos conexiones internas, conservando balances. Usa
SuperArcs de giro legales cuando existen; en otro caso guarda conexiones
provisionales por extremos virtuales y multiplicidad, sin agregarlas al grafo.
Descarta candidatos en U, incluyendo representaciones de un mismo neutral.
Reclasifica después de cada intercambio y falla si no encuentra un par
admisible; no busca operaciones de más de dos cruces. Los cruces disminuyen
estrictamente y los ciclos dentro de la componente del depósito se conservan.
`SolveResult` retiene la solución original; `F` y `deposit_flow` se ignoran.

Luego se expanden las multiplicidades reparadas, incluidas las conexiones
provisionales, y se aplica el mismo Hierholzer estricto para obtener el
circuito global ordenado. Cada tramo comprendido entre una entrada y una salida de un cluster se registra como
una visita `(entrada, salida)`.

Por cada cluster se construye un `SuperGraph` local con un depósito artificial
y un vehículo lógico por visita. Para la visita `r` se fijan sus dos extremos:

$$
\sum_{a\in\delta^+(D,v_r^{in})}Y^{DK}_{a,r}=1, \qquad
\sum_{a\in\delta^-(v_r^{out},D)}Y^{KD}_{a,r}=1.
$$

Cada vehículo local tiene exactamente una entrada y una salida, por lo que no
puede intercambiar su salida con otra visita. El circuito global también
determina en qué visita aparece cada servicio requerido; esa asignación se
conserva como zona del vehículo local. Cada registro requerido que haya quedado
sin visita se asigna a la primera visita activa de su cluster, también con
demanda cero. Si no hay ninguna, se falla antes de resolver o exportar. Las
copias opcionales no generan servicios adicionales. Después de una reparación
la factibilidad local depende de sus portales y de esa asignación; si el solver
local no encuentra solución se conserva el código de salida 2.

Una visita degenerada que sólo atraviesa conectores de giro entre dos cruces
no crea un vehículo local: no contiene red física del cluster que optimizar.
Sus conectores se conservan literalmente al fusionar, manteniendo también su
pareja entrada/salida. Una visita con servicios reasignados o conexiones
provisionales siempre se optimiza. Los enlaces provisionales no se incorporan
al grafo local: el solver reconstruye el interior completo entre los portales
reales y la fusión recibe sólo arcos reales.

El vehículo del problema original sigue siendo único: los vehículos locales
son sólo etiquetas de visita. Para una arista no dirigida, el subproblema puede
volver a elegir cuál de sus dos orientaciones utiliza.

## Salida

Las soluciones locales se traducen a IDs globales y se fusionan por ID de
visita. Los subproblemas incluyen las copias físicas opcionales de su cluster;
la fusión suma su costo físico y conserva las transiciones entre clusters.
El merger valida servicio, continuidad y conexión al depósito.

Para la salida por registros, `NeutralExpandedGraph::contract_solution` acumula
originales y copias sobre los IDs originales y descarta los registros sintéticos.
El campo `OBJ` es la distancia física combinada. La cantidad de cambios se
informa por separado en `BEST_CLUSTER_PATH_RESULT`: `objective` y `crossings`
contienen el total reparado. Se informan los cruces antes y después; una
reconexión o reasignación fuerza `optimal=false`. El mero descarte de recorridos
internos sin servicio conserva el criterio de estados global y locales.

La opción `--sorter-output` exporta con la numeración virtual del `SuperGraph`
original, construido sólo cuando se solicita esa salida. Se mapean las
orientaciones físicas de cada copia a su original, se acumulan multiplicidades
y se traducen los conectores a giros y accesos al depósito existentes en el
supergrafo original. Esto evita depender de una numeración virtual que cambia
al agregar copias.

## Validación de las copias físicas (2026-11-11)

Se ejecutaron `make test`, `make test-unit` y la regresión del pipeline con el
sorter. Las pruebas cubren dos y tres clusters, extremos sin representación
común, registros paralelos dirigidos/no dirigidos, servicio único, prohibiciones
y vueltas en U entre copias, contracción de multiplicidades, subproblemas,
fusión y exportación con IDs virtuales originales.

Se reutilizaron las entradas normalizadas de la corrida
`experiments/cluster_crossings/20261110_185407_988880` para repetir los mapas
Streets de 3525 nodos, sin modificar sus particiones ni giros:

| Mapa | Cruces previos | Cruces con copias | Resultado |
| --- | ---: | ---: | --- |
| martinezA | 130 | 114 | Pipeline completo, distancia 26045.801270907708, optimalidad informada verdadera |
| martinezB | 88 | 50 | Sólo MIP global en diagnóstico separado; el pipeline no exporta solución |

Los logs y salidas quedan en `experiments/neutral_copies_20261111/`.
En martinezB el pipeline rechaza una componente de recorridos `Y` desconectada
del depósito (`id=153`, nodos virtuales `306 -> 307`). La resolución aislada
del MIP global termina con estado `Optimal` y objetivo 50, pero ese resultado
no valida una solución fusionada ni una distancia final. Se conserva el rechazo
existente de subtours; cambiar su tratamiento queda fuera del spec de copias.
La comparación no presupone una cota de `2 × clusters`.

## Validación de subtours, opción 1 (2026-11-12)

Se ejecutaron `make mip-best-cluster-tests mip-best-cluster`,
`./build/best_cluster_path_solver_test`, `make test-unit` y `make test`.
Las fixtures explícitas cubren descarte de X/Y y giros con multiplicidades,
servicios de demanda cero, promoción de passthrough, intercambios balanceados,
reclasificación de fragmentos, enlaces provisionales sin arco directo, visitas
sin servicio, reconstrucción local infactible y rechazo sin candidato sin U.
La fusión conserva el vector neutral reparado y sirve cada requerido una vez;
el extractor compartido sigue rechazando subtours. El CLI comprueba que un
servicio sin visita falla con código 1 sin crear ni sobrescribir salidas.

Se reutilizaron exactamente las entradas normalizadas y los grafos Streets de
3525 nodos de la regresión anterior. Logs, salidas físicas, exportaciones para
sorter y órdenes quedan en `experiments/subtour_option1_20261112/`.

| Mapa | Cruces antes | Cruces después | Distancia física | Resultado |
| --- | ---: | ---: | ---: | --- |
| martinezA | 114 | 114 | 26045.801270907708 | Pipeline completo |
| martinezB | 50 | 50 | 25464.57481896157 | Pipeline completo |

Ambas ejecuciones informaron `optimal=true`: no necesitaron reconectar cruces
ni reasignar servicios. MartinezB ya permite retirar sus interiores opcionales
desconectados, que antes bloqueaban el extractor. Estas ejecuciones comparan
antes/después dentro del mismo incumbente con gap 0.01; no certifican optimalidad
global de distancia. Las dos salidas también se proyectaron y ordenaron con el
sorter de clusters.
