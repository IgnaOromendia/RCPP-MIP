# Experimentacion

MAPAS: acassusoA, acassusoB, martinezA, martinezB

REGLAS: 
- las variantes bestClusterPath siempre pasan por tune_cluster_crossing.py
- siempre mapas OSM
- siempre 20% de aristas para el cluster

REPETICIONES: 1

Generación de clusters:
- BFS
- Streets

Estas variantes se aplican únicamente a `bestClusterPath`. MIP y F&O se
ejecutan una sola vez sobre el mapa original.

Modelos de distancia:
- mip
- f&o random
- f&o maxDeadheadCost
- f&o topKDeadheadCost
- bestClusterPath BFS
- bestClusterPath Streets

Modelos de ordenamiento
- mip + mipPathSort-FirstPass
- mip + mipPathSort-Clusters
- mip con aristas de cruce pesadas + mipPathSort-FirstPass (tener en cuenta restar el peso extra al objetivo)
- mip con aristas de cruce pesadas + mipPathSort-Clusters (tener en cuenta restar el peso extra al objetivo)
- bestClusterPath

### Experimentos 
- eje x con mapas ordenados por #aristas

1 y 2- Una misma corrida de los modelos de distancia guarda tiempo y función
objetivo. Se generan dos plots con los mismos seis modelos: eje y con tiempo o costo total del viaje.
3- Tiempos de modelos de ordenamiento: eje y con tiempo
4- Cantidad de cruces en modelos de ordenamiento: eje y con #cruces
