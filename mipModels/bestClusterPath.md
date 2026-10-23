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

## Archivo de transiciones

`tune_cluster_crossing.py` genera `graph_N.tuned.in-out.dat`:

```text
source_cluster target_cluster in_node out_node
1 2 1 2
1 3 3 4
2 1 5 6
```

Los clusters deben ser positivos y distintos. Los nodos virtuales se numeran
desde uno en el archivo y se convierten a índices desde cero en C++.

El tuner también genera `graph_N.tuned.arc-clusters.dat`:

```text
arc cluster
1 1
2 1
3 0
```

Los arcos se numeran desde uno. El cluster `0` identifica una transición
sintética; los valores positivos asignan cada registro físico expandido a su
subproblema.

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
las transiciones entre clusters. Antes de exportar se validan servicio único,
balance y conexión al depósito. El campo `OBJ` de la solución final es la
distancia física combinada. La cantidad de cambios se informa por separado en
`BEST_CLUSTER_PATH_RESULT`.
