# Tratamiento de subtours en BestClusterPath

## Problema

El modelo global de `BestClusterPath` minimiza solamente los recorridos `Y`
correspondientes a transiciones entre clusters. Los recorridos `Y` internos no
tienen costo en ese objetivo, por lo que una solución factible puede contener
circuitos balanceados desconectados del depósito.

Al reconstruir el recorrido, Hierholzer consume el circuito que contiene al
depósito. Si quedan arcos con multiplicidad positiva, esos arcos forman uno o
más subtours desconectados dentro del soporte de la solución.

En las instancias donde todo servicio `X` tiene demanda positiva, las
restricciones de flujo impiden que un subtour desconectado contenga servicio.
Antes de apoyarse en esta propiedad hay que validarla: una instancia con
servicio de demanda cero no queda cubierta por el mismo argumento.

## Propuesta de reconexión

1. Ejecutar Hierholzer desde el depósito para reconstruir el camino principal.
2. Mientras queden arcos sin consumir:
   1. Ejecutar Hierholzer desde uno de ellos para obtener un subtour.
   2. Elegir un arco dirigido `a -> b` del subtour y retirarlo.
   3. Elegir un punto `m` del camino principal.
   4. Agregar un camino legal desde `m` hasta `b`.
   5. Recorrer el subtour abierto desde `b` hasta `a`.
   6. Agregar un camino legal desde `a` hasta `m` y continuar el camino
      principal.
3. Repetir hasta incorporar todos los subtours.

No alcanza con retirar una arista y agregar una única conexión: al abrir un
circuito quedan dos extremos dirigidos. Se necesitan dos caminos de conexión,
o una operación equivalente que corte también el circuito principal y haga un
empalme dirigido de ambos circuitos.

Los caminos agregados deben buscarse en el `SuperGraph`, no en el grafo físico
sin expandir, para respetar retornos en U y giros prohibidos. La selección no
debería ser realmente arbitraria: conviene elegir conjuntamente el arco que se
abre, el punto de inserción y los caminos que minimicen el costo de reconexión.

## Condiciones que deben preservarse

- No retirar un arco de servicio `X`.
- Mantener el balance de entrada y salida en todos los nodos virtuales.
- Usar solamente conectores de giro existentes en el `SuperGraph`.
- No alterar inadvertidamente la cantidad óptima de transiciones entre
  clusters.
- Actualizar las multiplicidades de `Y` de todos los arcos retirados y
  agregados antes de extraer las visitas de clusters.
- Volver a ejecutar Hierholzer y exigir que consuma todas las multiplicidades
  de la solución reparada.

Una reconexión que atraviese una transición `in/out` agrega un cruce que no
formaba parte del valor optimizado por el MIP. Por eso primero debe intentarse
una reconexión completamente interna al mismo cluster. Si no existe, hay que
decidir explícitamente si se acepta modificar el número de cruces.

## Alternativa recomendada para subtours irrelevantes

Para el caso observado, un subtour compuesto exclusivamente por `Y` internos
no contiene servicio ni transiciones `in/out`. Es recorrido gratuito generado
por la degeneración del objetivo y no aporta información a la optimización
posterior de los clusters.

En ese caso es preferible eliminar el subtour completo en lugar de conectarlo:

1. Reconstruir el componente del depósito con Hierholzer.
2. Clasificar todos los arcos sobrantes.
3. Fallar si aparece un `X`.
4. Descartar las componentes formadas únicamente por `Y`, incluidos los
   cruces `in/out` desconectados.
5. Continuar la extracción de visitas usando sólo el circuito del depósito.

Esta alternativa no agrega distancia, no cambia el número de cruces y evita
resolver un problema de caminos que no afecta el resultado buscado. La
reconexión queda reservada para el caso futuro en que exista una razón para
conservar las multiplicidades del subtour.

Esta es la alternativa implementada actualmente por `SolutionEulerCircuit`.
`ClusterBoundaryExtractor` forma visitas sólo con el circuito conectado y
`ClusterSolutionMerger` reinserta únicamente sus cruces `in/out`. Un arco de
servicio fuera del circuito principal continúa considerándose inválido.

## Pruebas necesarias

- Un circuito conectado al depósito sin subtours se conserva sin cambios.
- Un subtour formado sólo por `Y` internos se descarta.
- Varios subtours `Y` internos se descartan en la misma solución.
- Un sobrante que contiene `X` produce un error.
- Un subtour `Y` con transiciones `in/out` se descarta y no se reinserta.
- Una instancia con demanda requerida cero no asume conectividad por flujo.
- El circuito resultante respeta los giros prohibidos y no contiene retornos
  en U.
- El valor de cruces usado por el posprocesamiento coincide con el informado
  por el solver global.
