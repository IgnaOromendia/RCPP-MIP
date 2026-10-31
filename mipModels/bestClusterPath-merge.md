# Fusión de los subproblemas de `bestClusterPath`

Este documento describe cómo `bestClusterPathExec` descompone la solución
global en visitas a clusters y cómo vuelve a juntar las soluciones locales en
una única `Solution` del RCPP original.

La idea central es que el primer solver decide la **estructura global** del
recorrido: cuántas veces cruza entre clusters y qué circuito se usa para
descomponerlo en visitas. Los solvers locales pueden cambiar el camino dentro
de cada cluster, pero no pueden cambiar la entrada y la salida correspondientes
a cada visita. La fusión reemplaza los tramos internos globales por esos caminos
locales y conserva los cruces entre clusters elegidos originalmente.

## Flujo completo

```text
solución global con cambios mínimos
               |
               v
expandir multiplicidades y construir un circuito de Euler ordenado
               |
               v
separar el circuito en visitas (entrada, interior, salida)
               |
               v
crear y resolver un RCPP local por cluster
               |
               v
traducir IDs locales -> IDs globales
               |
               v
reconstruir portales y reinsertar transiciones entre clusters
               |
               v
acumular todo sobre el vehículo global 1 y validar
```

La implementación principal está repartida entre:

- `SolutionEulerCircuit`: ordena la solución global agregada.
- `ClusterSegmentProcessor`: obtiene las visitas a cada cluster.
- `ClusterSubProblemBuilder`: construye los grafos y los vehículos locales.
- `ClusterRCPPSolver`: optimiza cada cluster manteniendo cada pareja
  entrada/salida.
- `ClusterSolutionMerger`: traduce y fusiona las soluciones locales.

## 1. Convertir la solución global en un circuito

La primera solución contiene multiplicidades de:

- `X`: arcos usados para prestar servicio;
- `Y`: arcos recorridos sin prestar servicio;
- conectores entre el depósito y la red.

Estas multiplicidades no expresan un orden. `SolutionEulerCircuit::circuit()`
expande cada valor positivo en copias individuales y aplica Hierholzer desde el
depósito, representado por el nodo `-1`. El resultado es un vector continuo de
`ClusterSolutionArc` que empieza y termina en el depósito.

El extractor valida las multiplicidades, los extremos y la continuidad. Toda
ocurrencia positiva debe pertenecer al circuito conectado al depósito. Cualquier
componente desconectada, incluidos los subtours formados sólo por recorridos
`Y`, produce un error y se conserva para el diagnóstico; no se descarta.

El orden es necesario porque una frontera agregada sólo indica cuántas entradas
y salidas hay. El circuito permite saber **qué salida corresponde a cada
entrada**.

## 2. Formar las visitas

`ClusterSegmentProcessor` recorre el circuito y mantiene a lo sumo una visita
abierta. Una visita tiene:

```text
ClusterVisit
  id global de visita
  portal de entrada
  portal de salida
  servicios asignados a la visita
  arcos internos, si es un passthrough
```

Los eventos que delimitan una visita son:

- un conector `depósito -> red` abre la primera visita;
- una transición sintética `cluster i -> cluster j` cierra la visita de `i` y
  abre una visita de `j`;
- un conector `red -> depósito` cierra la última visita.

Cada portal guarda el tipo (`NeutralInOut` o `GlobalDeposit`), el arco global,
el nodo físico de frontera y si actúa como entrada o salida. Los IDs de visita
son globales y crecientes a lo largo del circuito, aunque después las visitas
se agrupen por cluster.

Mientras una visita está abierta también se registran los `edge_id` de los
servicios encontrados. Esto fija no sólo la pareja entrada/salida, sino también
qué servicios debe conservar esa visita durante la optimización local.

### Visitas `passthrough`

Si entre la entrada y la salida no aparece ningún registro físico del cluster,
la visita sólo contiene conectores de giro. Se marca como `passthrough` y no se
crea un vehículo local para ella. Sus IDs de arco se guardan para copiarlos
literalmente durante la fusión.

## 3. Construir cada subproblema

Se crea un `ClusterProblem` por cada cluster que tenga una frontera activa en
el circuito global. Un cluster formado sólo por registros opcionales y nunca
visitado no genera un subproblema.

Para un cluster `c`, el subproblema contiene:

- todos los registros físicos asignados a `c`;
- los giros internos que genera su `SuperGraph`;
- como nodos de depósito local, la unión de los nodos de entrada y salida de
  sus visitas;
- un vehículo lógico por visita no `passthrough`.

Las transiciones sintéticas de cluster `0` nunca se agregan a un subproblema.
Permanecen bajo control de la solución global.

Al construir cada `SuperGraph` local se guarda el mapeo:

$$
\text{local\_to\_global\_arc}[a_l] = a_g,
\qquad
\text{local\_to\_global\_node}[v_l] = v_g.
$$

Para los arcos físicos, la correspondencia usa el ID del registro original y
la orientación de sus extremos. Esto distingue las dos orientaciones de una
arista no dirigida. Los conectores de giro se mapean después por sus extremos
virtuales globales.

### Por qué hay varios vehículos locales

Los vehículos locales no representan camiones distintos. El vehículo local
`r` es una etiqueta para la visita `r` del cluster.

Cada servicio que la solución global colocó dentro de una visita se asigna a la
zona de su vehículo local. Además, `ClusterRCPPSolver` impone para ese vehículo:

$$
\sum_{a\in\delta^+(D)}Y^{DK}_{a,r}=1,
\qquad
\sum_{a\in\delta^-(D)}Y^{KD}_{a,r}=1,
$$

y restringe ambas sumas al nodo físico de entrada y de salida de la visita.
Así, si un cluster se visita dos veces, el solver no puede intercambiar las
salidas de ambas visitas ni mover un servicio de una visita a la otra.

El solver local sí puede elegir entre los nodos virtuales compatibles con ese
nodo físico y puede invertir la orientación de una arista no dirigida. Por eso
el merge no puede limitarse a reutilizar los conectores internos de la solución
global: debe observar qué extremo virtual eligió cada solución local.

### Primera y segunda visita al mismo cluster

Dos apariciones del mismo cluster en el circuito no se tratan como visitas
intercambiables. Por ejemplo, para el circuito ordenado:

```text
D -> A₁ -> N -> B₁ -> N -> C -> N -> B₂ -> N -> A₂ -> D
```

se crean cuatro visitas distintas para los clusters repetidos:

- `A₁`: entrada desde el depósito y salida hacia `B`;
- `A₂`: entrada desde `B` y salida hacia el depósito;
- `B₁`: entrada desde `A` y salida hacia `C`;
- `B₂`: entrada desde `C` y salida hacia `A`.

En el subproblema de `A`, `A₁` y `A₂` son vehículos lógicos diferentes;
lo mismo ocurre con `B₁` y `B₂` en el subproblema de `B`. El orden de las
visitas no se infiere nuevamente al resolver el cluster: cada vehículo queda
asociado, por su posición, con el ID global de la visita que lo originó.

Por lo tanto, el solver local no puede asignar indiferentemente un subcamino
con servicio a la primera o a la segunda pasada. Para cada visita quedan
fijados:

1. el nodo físico de entrada;
2. el nodo físico de salida;
3. los registros que deben recibir servicio en esa visita.

Si las dos visitas tienen exactamente los mismos extremos y la misma
asignación de servicios, pueden ser simétricas desde el punto de vista del
modelo. También puede variar el recorrido opcional o `deadhead` usado dentro de
cada una. Esto no permite intercambiar un servicio asignado a `B₁` con uno
asignado a `B₂`.

Hay además una diferencia entre la decisión MIP y su ordenamiento: el primer
solver produce multiplicidades de arcos, no la lista anterior de forma
explícita. `SolutionEulerCircuit` elige un circuito de Euler compatible con
esas multiplicidades. Las visitas, sus parejas entrada/salida y sus servicios
se fijan a partir de **ese circuito ordenado**, y son las que luego respetan los
subproblemas y el merge.

## 4. Traducir las soluciones locales

`ClusterSolutionMerger::add_values_from_local_solution()` procesa las
soluciones en el mismo orden que el vector de subproblemas.

Para cada valor local positivo:

$$
X_g[a_g] \mathrel{+}= X_l[a_l],
\qquad
Y_g[a_g] \mathrel{+}= Y_l[a_l].
$$

El arco global `a_g` se obtiene del mapeo creado al construir el subproblema.
Los mapas globales acumulan multiplicidades, de modo que un mismo arco usado
por varias visitas o clusters aparece una sola vez en la salida con el valor
total.

Los conectores al depósito artificial local no se copian directamente. De cada
uno se extraen:

- el vehículo local;
- si es la entrada o la salida;
- el nodo virtual local elegido;
- el nodo virtual global correspondiente.

El número de vehículo local se convierte nuevamente en el ID global de visita,
saltando las visitas `passthrough`. Para cada visita quedan registradas dos
elecciones:

```text
choices[visit_id][entrada] = nodo_virtual_global
choices[visit_id][salida]  = nodo_virtual_global
```

Cada extremo debe aparecer exactamente una vez.

## 5. Reconstruir las fronteras

Una vez traducido el interior, el merge conecta cada extremo local con su
portal global.

### Portal entre clusters

Sea `t: u -> v` la transición sintética global entre dos clusters.

- Para la salida del cluster origen se agrega el conector
  `nodo_virtual_elegido -> u`.
- Para la entrada al cluster destino se agrega el conector
  `v -> nodo_virtual_elegido`.

La transición `u -> v` no se agrega en este paso. Se reinserta después desde la
solución global, con su multiplicidad original.

### Portal del depósito

- Una entrada agrega `depósito_global -> nodo_virtual_elegido`.
- Una salida agrega `nodo_virtual_elegido -> depósito_global`.

Estos arcos se guardan como `deposit_traversals`, usando `-1` para representar
el extremo depósito en la `Solution` exportada.

### Visita `passthrough`

Como no existe solución local, se reinsertan directamente todos sus conectores
internos guardados. Si alguno de sus portales es el depósito, también se copia
el conector global correspondiente.

## 6. Reinsertar los cruces globales

Finalmente, el merger reinserta cada cruce a partir de las salidas de las
visitas del circuito global. Así conserva exactamente las multiplicidades
seleccionadas para cada arco in/out:

$$
Y_{final}[a] \mathrel{+}=Y_{global}[a]
\quad\forall a\in A_{io}.
$$

Este paso es el que conserva la cantidad y dirección de los cambios entre
clusters obtenidos por `BestClusterPathSolver`. Los solvers locales nunca
pueden crear, eliminar ni cambiar esas transiciones.

## 7. Materializar la solución final

Los mapas acumulados se convierten en una única `Solution`:

- todos los servicios y recorridos internos usan IDs del `SuperGraph` global;
- todos los vehículos lógicos locales colapsan en el vehículo global `1`;
- los conectores al depósito vuelven al formato global;
- la salida sigue siendo agregada por arco, no un recorrido ordenado.

El objetivo exportado se recalcula como distancia física:

$$
z_{final}=
\sum_a c_a X_a+
\sum_{a:\,edge\_id(a)\geq0,\ cluster(a)>0}c_aY_a.
$$

No se suman al objetivo final los giros, los conectores del depósito ni las
transiciones sintéticas. El número óptimo de cruces se informa por separado en
`BEST_CLUSTER_PATH_RESULT`.

## 8. Validaciones posteriores al merge

Antes de escribir el archivo se comprueban tres invariantes.

### Continuidad

Para cada nodo interno se acumulan salidas con signo positivo y entradas con
signo negativo, incluyendo servicio, recorridos y el extremo interno de los
conectores al depósito. Todo balance debe ser cero.

### Servicio

Los servicios se agrupan por `edge_id`, de modo que las dos orientaciones de
una arista no dirigida cuentan juntas. Cada registro requerido de la instancia
global debe quedar servido exactamente una vez.

### Conexión

Se construye la adyacencia no dirigida de todos los arcos con multiplicidad
positiva y se recorre desde el depósito. Todo nodo utilizado debe ser
alcanzable.

Si falla cualquiera de estas condiciones, la solución no se exporta.

## Ejemplo conceptual

Supongamos que el circuito global es:

```text
D -> [visita 1 de C1] -> T12 -> [visita 2 de C2]
  -> T21 -> [visita 3 de C1] -> D
```

La descomposición crea:

- para `C1`, un subproblema con dos vehículos lógicos, uno para la visita 1 y
  otro para la visita 3;
- para `C2`, un subproblema con un vehículo lógico para la visita 2.

Cada solver local reemplaza sólo el contenido entre los corchetes. El merge:

1. traduce esos tres caminos a IDs globales;
2. conecta la salida elegida de la visita 1 con `T12`;
3. conecta `T12` con la entrada elegida de la visita 2;
4. hace lo mismo alrededor de `T21`;
5. reconstruye los conectores inicial y final al depósito;
6. acumula los tres vehículos lógicos sobre el vehículo global 1.

Por lo tanto, puede cambiar la distancia recorrida dentro de `C1` y `C2`, pero
se conservan `T12`, `T21` y las parejas de frontera derivadas del circuito
`C1 -> C2 -> C1`. La salida agregada no codifica esa secuencia como una lista;
otro circuito de Euler compatible con las mismas multiplicidades también puede
existir.

## Qué preserva y qué puede cambiar

El merge preserva:

- la cantidad, dirección y multiplicidad de los cruces entre clusters;
- los IDs y las parejas de frontera de las visitas obtenidas del circuito;
- la pareja entrada/salida y los servicios de cada visita;
- el servicio único de todos los registros requeridos;
- la continuidad y la conexión con el depósito.

La optimización local puede cambiar:

- el camino interno de cada visita;
- las multiplicidades de recorrido interno;
- la orientación usada para una arista no dirigida;
- el nodo virtual concreto, entre los compatibles con el mismo nodo físico de
  frontera.

Como `Solution` vuelve a almacenar multiplicidades por arco, el archivo final
no conserva un orden explícito ni el vehículo lógico que produjo cada uso. Esa
información se utiliza durante la construcción y la fusión, pero la pertenencia
de los recorridos opcionales a `A₁`/`A₂` o `B₁`/`B₂` deja de estar
explícita después de acumularlos. Por eso un ordenamiento posterior puede
encontrar otro circuito de Euler compatible con las mismas multiplicidades. Si
se necesita un recorrido secuencial, debe ordenarse posteriormente con el path
sorter.

La solución fusionada todavía usa el supergrafo expandido. Antes de escribir,
`NeutralExpandedGraph::contract_solution` la contrae una sola vez al dominio
original. Conserva todas las ocurrencias físicas, suma las que se contraen a la
misma orientación y recompone los giros adyacentes a cada arco in/out eliminado.
Un giro prohibido o inexistente, un servicio perdido o una componente
inconexa produce un error. No se construye un supergrafo original ni se reparan
recorridos durante esta operación.
