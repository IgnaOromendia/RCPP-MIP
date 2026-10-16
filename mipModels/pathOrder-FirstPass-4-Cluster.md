# Orden de las pasadas por cluster

El modelo considera un único vehículo y minimiza la cantidad de cambios de
cluster entre pasadas consecutivas. Cada arista de la solución de entrada,
incluidos los conectores de giro y del depósito, tiene un cluster asignado.

## Conjuntos y constantes

Sea $E$ el conjunto de aristas de la solución y

$$
\begin{align*}
    m_e &= X^*_e + Y^*_e, \\
    K &= \sum_{e\in E}m_e, \\
    R &= \{(e,k):e\in E,\ k=1,\dots,m_e\},
         & |R|&=K.
\end{align*}
$$

Cada $r=(e,k)\in R$ representa la pasada número $k$ de la arista $e$ y
$c(e)$ denota su cluster. Dos pasadas pueden enlazarse cuando el nodo final de
la primera coincide con el inicial de la segunda. Las transiciones posibles no
permiten salir de la llegada al depósito ni entrar nuevamente en la salida:

$$
T=\{(r,s)\in R\times R:
    \operatorname{to}(e_r)=\operatorname{from}(e_s),\
    r\neq\beta,\ s\neq\alpha\},
$$

donde $\alpha$ es la única pasada que sale del depósito y $\beta$ la única que
regresa a él.

Para $r=(e,k)$ con $k<m_e$, definimos $r'=(e,k+1)$.

## Variables

$$
\begin{align*}
    Z_{rs} &=
    \begin{cases}
        1 & \text{si }s\text{ sigue inmediatamente a }r,\\
        0 & \text{en caso contrario,}
    \end{cases}
    && \forall(r,s)\in T, \\
    X_r &= \text{posición de la pasada }r\text{ en el recorrido}
    && \forall r\in R.
\end{align*}
$$

No se crean variables específicas por cluster. La asignación $c(e)$ es un dato
de entrada y el objetivo se expresa directamente con las variables de
transición $Z$.

## Modelo

$$
\begin{align}
    \min\quad
        & \sum_{(r,s)\in T:\ c(e_r)\neq c(e_s)} Z_{rs} \\
    \text{sujeto a}\quad
        & \sum_{s:(r,s)\in T}Z_{rs}=1
        && \forall r\in R\setminus\{\beta\} \\
        & \sum_{r:(r,s)\in T}Z_{rs}=1
        && \forall s\in R\setminus\{\alpha\} \\
        & X_\alpha=1 \\
        & X_\beta=K \\
        & X_s\geq X_r+1-K(1-Z_{rs})
        && \forall(r,s)\in T \\
        & X_s\leq X_r+1+K(1-Z_{rs})
        && \forall(r,s)\in T \\
        & X_r+1\leq X_{r'}
        && \forall r=(e,k)\in R:k<m_e \\
        & 1\leq X_r\leq K
        && \forall r\in R \\
        & Z_{rs}\in\{0,1\}
        && \forall(r,s)\in T.
\end{align}
$$

- (1) cuenta cada transición consecutiva cuyos extremos pertenecen a clusters
  distintos. Todas las transiciones entre clusters tienen peso uno.
- (2) y (3) asignan una única sucesora y una única predecesora a cada pasada,
  excepto a los extremos del recorrido.
- (4) y (5) fijan la salida y el regreso al depósito.
- (6) y (7) hacen consecutivas las posiciones cuando $Z_{rs}=1$.
- (8) conserva el orden relativo de las pasadas repetidas de una misma arista.
- (9) y (10) definen los dominios de las variables.

El recorrido resultante tiene la forma

$$
\alpha\rightarrow r_2\rightarrow\dots\rightarrow r_{K-1}
\rightarrow\beta.
$$

El valor objetivo es exactamente la cantidad de fronteras de cluster que
atraviesa ese recorrido. Por ejemplo, una secuencia de clusters
$A,A,B,B,A$ tiene valor $2$: un cambio de $A$ a $B$ y otro de $B$ a $A$.
La mezcla de clusters sigue siendo factible; el modelo elige, entre los
circuitos factibles, el que usa menos cambios.

## Warm start

Antes de resolver el MIP se construye un circuito euleriano con Hierholzer
guiado por cluster. Ese circuito proporciona valores iniciales para $X$ y para
las transiciones factibles $Z$. Una resolución restringida, con esos valores
fijados, valida el circuito y permite registrar un punto inicial completo antes
de ejecutar la optimización sin las fijaciones.

## Tamaño del bloque específico de clusters

La variante por clusters no agrega variables ni restricciones adicionales a la
formulación común de ordenamiento. Su única modificación algebraica es el
objetivo (1), que contiene a lo sumo $|T|$ términos y solo incluye las
transiciones cuyos clusters son distintos.

El archivo de clusters debe asignar un identificador positivo a cada arista de
la solución. Al leerlo, esos identificadores se convierten a índices internos
desde cero; pueden existir huecos entre identificadores sin alterar el
objetivo.
