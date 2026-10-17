# RCPP con objetivo de cambios entre clusters

Esta variante conserva la formulación, las variables y las restricciones del
RCPP base. Su única diferencia algebraica es el objetivo.

Sea $P$ el conjunto de vehículos y sea $A_{io}$ el conjunto de transiciones
sintéticas `in/out` entre copias de clusters declarado en el archivo auxiliar.
Cada transición se agrupa por su cluster de origen, aunque el objetivo actual
asigna el mismo peso a todos los cambios. La variable $Y_{ap}$ es la cantidad
de usos de la transición $a$ por el vehículo $p$.

$$
\min \sum_{p\in P}\sum_{a\in A_{io}} Y_{ap}.
$$

Las variables de servicio `X`, las pasadas físicas, los conectores de giro y
los conectores del depósito no forman parte del objetivo. Todos ellos siguen
participando en las restricciones de continuidad y flujo del RCPP base.

## Archivo de transiciones in/out

El tuner genera por defecto `graph_N.tuned.in-out.dat` con el formato compatible:

```text
cluster in_node out_node
1 1 2
1 3 4
2 5 6
```

La primera columna identifica el cluster de origen de la transición. Los nodos
virtuales se numeran desde uno en el archivo y se convierten a índices desde
cero al leerlos. Los clusters deben ser positivos y una transición dirigida no
puede aparecer más de una vez.
