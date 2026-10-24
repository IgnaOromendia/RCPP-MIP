# Algoritmo de clustering por calles divisorias

Entrada: un grafo con un conjunto `E` de aristas obligatorias y un porcentaje
`p`.

1. Marcar como zona neutra los nodos de avenidas y calles primarias.
2. Clusterizar las aristas obligatorias sin atravesar nodos ni calles neutras.
3. Calcular el tamaño máximo objetivo en cantidad de aristas:
   `L = floor(|E| * p / 100)`. Si `L < 1`, el porcentaje es inválido. Elegir
   el cluster más grande, `C`. Si
   `|C| <= L`, terminar; en caso contrario, continuar con `C`.
4. Entre las calles neutras limítrofes a `C`, elegir la de mayor longitud
   geográfica. En un empate, usar el orden de las aristas del grafo.
5. Calcular el centro geográfico de `C` (v) y elegir el nodo no neutro más cercano
   que pertenezca a una calle con nombre incidente a la frontera elegida.
6. Si esa misma calle llega, sin cambiar de nombre ni atravesar una tercera
   frontera, hasta otro límite de `C`, convertir todo ese tramo en neutro y
   volver al paso 2.
7. Si no existe ese tramo, ordenar los nodos candidatos de izquierda a derecha,
   considerar `v` la posición elegida en el paso 5 y probar `v + 1`.
8. Si tampoco existe, probar `v - 1`.
9. Repetir los pasos 7 y 8 aumentando el desplazamiento:
   `v + 2`, `v - 2`, `v + 3`, `v - 3`, hasta encontrar una calle válida. Si
   se agotan los candidatos, terminar con error sin generar una partición.

Un extremo de la calle también cuenta como límite de `C` cuando convertir el
tramo hasta ese extremo en neutro separa efectivamente el cluster.

Sólo `avenida` y `primary` forman la red neutra inicial. Los tramos creados por
el algoritmo se conservan como `divider_N`. La proporción indicada con
`--percentage` es `p`: el límite crece con la cantidad de aristas obligatorias
del input y no altera la selección de la calle divisoria. Las aristas opcionales
heredan después el cluster obligatorio más cercano y no cuentan para el límite.
