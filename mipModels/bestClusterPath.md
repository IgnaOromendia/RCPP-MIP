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
cluster $k$, contando cada arista no dirigida una sola vez. Cada componente de
neutros debe tocar exactamente dos clusters: el menor usa el camino original
$P$ y el mayor su copia $P'$. Cada nodo $v$ tiene exactamente una copia $v'$ y
dos registros dirigidos independientes $v \to v'$ y $v' \to v$, de cluster cero,
demanda cero y costo $S_{c_1}+S_{c_2}$. Los registros entre neutros se copian una
sola vez y conservan dirección; sólo $P$ mantiene el servicio requerido.

La clase es dueña de los costos originales, los mapeos, la asignación por
registro y las transiciones in/out. Cada sentido genera exactamente un
`SuperArc` sin pareja y aporta coeficiente uno en su variable $Y$. Los costos
de transición no intervienen en ese objetivo ni en la distancia final.
No se consumen archivos `.tuned.*`, `in-out`, `arc-clusters` ni `mapping`.

## Optimización interna

Primero se expanden las multiplicidades de `X`, `Y` y de los conectores del
depósito y se aplica Hierholzer para obtener el circuito global ordenado. Cada
tramo comprendido entre una entrada y una salida de un cluster se registra como
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
conserva como zona del vehículo local, evitando simetrías entre visitas y
garantizando un incumbente factible por construcción.

Una visita degenerada que sólo atraviesa conectores de giro entre dos cruces
no crea un vehículo local: no contiene red física del cluster que optimizar.
Sus conectores se conservan literalmente al fusionar, manteniendo también su
pareja entrada/salida.

El vehículo del problema original sigue siendo único: los vehículos locales
son sólo etiquetas de visita. Para una arista no dirigida, el subproblema puede
volver a elegir cuál de sus dos orientaciones utiliza.

## Salida

Las soluciones locales se traducen a IDs globales y se fusionan por ID de
visita. Después se acumulan sobre el vehículo global 1 y se vuelven a insertar
las transiciones entre clusters. `NeutralExpandedGraph::contract_solution` descompone la solución fusionada en
circuitos del depósito, conserva cada ocurrencia física, elimina únicamente
los registros in/out y recompone los conectores con la numeración virtual
canónica original. La clase usa la `Instance` original y mapeos compactos; no
construye un segundo `Graph` ni `SuperGraph` global. Valida los giros originales,
el servicio único, balance y conexión al depósito. Los subtours sin servicio
también son errores; no se eliminan ni se agregan rutas de reparación. El campo `OBJ` de la solución final es la
distancia física combinada. La cantidad de cambios se informa por separado en
`BEST_CLUSTER_PATH_RESULT`.
