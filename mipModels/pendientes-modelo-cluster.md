# Pendientes del modelo Cluster

El modelo `PathSorterCluster` todavía no está implementado. Para que coincida
con `pathOrder-FirstPass-4-Cluster.md` falta:

- Guardar el cluster de cada arista.
- Definir los pares de clusters a comparar y sus pesos.
- Validar clusters, pares, pesos y conectores del depósito.
- Crear las variables de posición y sucesión (`X` y `Z`).
- Crear las variables de orden y penalización (`O` y `Q`).
- Agregar las restricciones que deciden qué cluster termina primero.
- Reutilizar las restricciones comunes que construyen el recorrido.
- Implementar el objetivo que penaliza la mezcla entre clusters.
- Habilitar `solve()` y la extracción del orden resultante.
- Definir cómo tratar las dos orientaciones de una arista no dirigida.
- Agregar pruebas funcionales y un ejecutable para usar el modelo.

Actualmente las pruebas solamente comprueban que el modelo informe que no está
implementado.
