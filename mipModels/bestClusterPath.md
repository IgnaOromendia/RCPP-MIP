# RCPP con objetivo de pasadas físicas por cluster

Esta variante conserva la formulación, las variables y las restricciones del
RCPP base. Su única diferencia algebraica es el objetivo.

Sea $P$ el conjunto de vehículos y sea $A_{io}$ el conjunto de arcos físicos
`in/out` declarado en el archivo auxiliar. Cada arco pertenece a un cluster
positivo, aunque el objetivo actual asigna el mismo peso a todos los clusters.
La variable $Y_{ap}$ es la cantidad de recorridos sin servicio del arco $a$ por
el vehículo $p$.

$$
\min \sum_{p\in P}\sum_{a\in A_{io}} Y_{ap}.
$$

Las variables de servicio $X$ no forman parte del objetivo. Tampoco se cuentan
los conectores de giro, los conectores del depósito ni las transiciones
sintéticas creadas para cambiar entre copias de clusters. Todos ellos siguen
participando en las restricciones de continuidad y flujo del RCPP base.

## Archivo de aristas in/out

El tuner genera por defecto `graph_N.tuned.in-out.dat` con el formato:

```text
cluster in_node out_node
1 1 2
1 3 4
2 5 6
```

Los nodos virtuales se numeran desde uno en el archivo y se convierten a
índices desde cero al leerlos. Los clusters deben ser positivos y una arista
dirigida no puede aparecer más de una vez. Para una arista no dirigida se
escriben sus dos orientaciones. Las transiciones sintéticas no se escriben.
