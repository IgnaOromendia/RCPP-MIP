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

Por cada cluster se construye un `SuperGraph` local con un depósito artificial.
Para cada terminal `v` se fijan las entradas y salidas obtenidas de la solución
global:

$$
Y^{DK}_{v}=e_{v}, \qquad Y^{KD}_{v}=s_{v}.
$$

Se valida:

$$
\sum_v e_v=\sum_v s_v
$$

para cada cluster. Las variables admiten multiplicidades mayores
que uno; las restricciones del RCPP base que limitan a una salida y una llegada
al depósito no se aplican en estos subproblemas.

Cada servicio pertenece al único camión. Para una arista no dirigida, el
subproblema puede volver a elegir cuál de sus dos orientaciones utiliza.

## Salida

Las soluciones locales se traducen a IDs globales, se combinan y se vuelven a
insertar las transiciones entre clusters. El campo `OBJ` de la solución final
es la distancia física combinada. La cantidad de cambios se informa por
separado en `BEST_CLUSTER_PATH_RESULT`.
