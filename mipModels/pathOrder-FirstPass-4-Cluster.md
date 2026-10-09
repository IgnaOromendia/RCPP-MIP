# Orden de las pasadas por cluster

El modelo considera un único vehículo. Cada arista física pertenece a un
cluster. Para cada par de clusters, el modelo elige cuál debería terminar
primero y penaliza las primeras pasadas que se adelantan a ese orden elegido.

## Constantes

$$
\begin{align*}
    m_e &= X^*_e + Y^*_e \\
    K &= \sum_{e\in E}m_e \\
    R &= \{(e,k):e\in E \land k=1\dots m_e\},
         & |R|=K \\
    S &= \{(e,f)\in E\times E:
           e=(x,v)\land f=(v,y)\}.
\end{align*}
$$

Sea $E^C\subseteq E$ el conjunto de aristas físicas que reciben un cluster.
Los conectores de giro y del depósito no pertenecen a $E^C$. Definimos:

$$
\begin{align*}
    \mathcal C &= \text{conjunto de clusters}, \\
    c(e) &\in \mathcal C
        && \forall e\in E^C, \\
    E_c &= \{e\in E^C:c(e)=c\}, \\
    H &\subseteq \{\{c,d\}:c,d\in\mathcal C,\ c\neq d\}, \\
    w_{\{c,d\}} &= \frac{1}{|E_c|+|E_d|}
        && \forall\{c,d\}\in H.
\end{align*}
$$

Cada elemento $\{c,d\}\in H$ es un par no ordenado: no determina cuál cluster
debe terminar primero. Para el caso particular mencionado,
$H=\{\{A,B\}\}$.

Los conectores del depósito ya forman parte de $E$. La solución de entrada
contiene exactamente una pasada que sale del depósito y una que regresa a él.
Definimos esas pasadas reales como:

$$
\begin{align*}
    \alpha &\in R,
        & e_\alpha&=(d,v), \\
    \beta &\in R,
        & e_\beta&=(v,d).
\end{align*}
$$

Por lo tanto, $\alpha$ y $\beta$ no son nodos ni aristas ficticias. Son alias
de las pasadas de los conectores del depósito seleccionados para el recorrido.

Las transiciones posibles se definen sin permitir una transición hacia el
inicio ni desde el final:

$$
T=\{(r,s):r,s\in R\land(e_r,e_s)\in S
    \land r\neq\beta\land s\neq\alpha\}.
$$

Sea $r=(e,k)\in R$. Definimos:

- $r'=(e,k+1)$ cuando $k<m_e$.
- $r^1=(e,1)$ como la primera pasada de $e$.
- $P=\{(e,1):e\in E\}$ como el conjunto de primeras pasadas.

## Variables

$$
\begin{align*}
    Z_{rs} &=
    \begin{cases}
        1 & \text{si }s\text{ sigue inmediatamente a }r,\\
        0 & \text{en caso contrario,}
    \end{cases}
    && \forall (r,s)\in T, \\
    X_r &= \text{posición de la pasada }r\text{ en el recorrido}
    && \forall r\in R, \\
    L_c &= \text{última posición entre las primeras pasadas del cluster }c
    && \forall c\in\mathcal C, \\
    O_{cd} &=
    \begin{cases}
        1 & \text{si }c\text{ termina antes que }d,\\
        0 & \text{si }d\text{ termina antes que }c,
    \end{cases}
    && \forall\{c,d\}\in H, \\
    Q_{ce} &=
    \begin{cases}
        1 & \text{si la primera pasada de }e\text{ ocurre antes de que}\\
          & \text{termine el cluster }c,\\
        0 & \text{en caso contrario,}
    \end{cases}
    && \forall c\in\mathcal C,\ e\in E^C:
       \{c,c(e)\}\in H.
\end{align*}
$$

La posición de la primera pasada de una arista $e$ ya está representada por
$X_{e^1}$; no hace falta crear otra variable para ella. La variable auxiliar
$L_c$ acota superiormente todas esas primeras posiciones dentro de $E_c$ y
puede elegirse igual a su máximo. Un cluster $c$ terminó su primera cobertura
cuando todas las aristas de $E_c$ ya aparecieron. La familia $Q$ tiene siempre
el mismo significado. La variable $O_{cd}$ determina cuál de las dos
direcciones se activa mediante las restricciones; las variables $Q$ de la
dirección no elegida quedan en cero por el objetivo.

## Modelo

$$
\begin{align}
    \min\quad
        & \sum_{\{c,d\}\in H}w_{\{c,d\}}
          \left(\sum_{f\in E_d}Q_{cf}
          +\sum_{e\in E_c}Q_{de}\right) \\
    \text{sujeto a}\quad
        & L_c\geq X_{a^1}
        && \forall c\in\mathcal C,\ a\in E_c \\
        & L_c-X_{f^1}
          \leq K\left(Q_{cf}+1-O_{cd}\right)
        && \forall\{c,d\}\in H,\ f\in E_d \\
        & L_d-X_{e^1}
          \leq K\left(Q_{de}+O_{cd}\right)
        && \forall\{c,d\}\in H,\ e\in E_c \\
        & \sum_{s:(r,s)\in T}Z_{rs}=1
        && \forall r\in R\setminus\{\beta\} \\
        & \sum_{r:(r,s)\in T}Z_{rs}=1
        && \forall s\in R\setminus\{\alpha\} \\
        & X_\alpha=1 \\
        & X_\beta=K \\
        & X_s\geq X_r+1-K(1-Z_{rs})
        && \forall (r,s)\in T \\
        & X_s\leq X_r+1+K(1-Z_{rs})
        && \forall (r,s)\in T \\
        & X_r+1\leq X_{r'}
        && \forall r=(e,k)\in R:k<m_e \\
        & 1\leq X_r\leq K
        && \forall r\in R \\
        & 1\leq L_c\leq K
        && \forall c\in\mathcal C \\
        & O_{cd}\in\{0,1\}
        && \forall\{c,d\}\in H \\
        & Q_{ce}\in\{0,1\}
        && \forall c\in\mathcal C,\ e\in E^C:
           \{c,c(e)\}\in H \\
        & Z_{rs}\in\{0,1\}
        && \forall (r,s)\in T.
\end{align}
$$

- (1): minimiza la cantidad de aristas adelantadas según el orden que el
  propio modelo elige para cada par. El peso $1/(|E_c|+|E_d|)$ normaliza la
  contribución por la suma de los tamaños de ambos clusters.
- (2): hace que $L_c$ sea una cota superior de las primeras posiciones de
  todas las aristas del cluster $c$. Siempre puede elegirse como su máximo.
- (3): queda activa cuando $O_{cd}=1$. Penaliza una arista de $d$ si aparece
  antes de $L_c$, es decir, antes de que todas las aristas de $c$ hayan
  aparecido.
- (4): queda activa cuando $O_{cd}=0$ y aplica la misma definición $Q_{de}$
  a las aristas de $c$ usando $L_d$.
- (5) y (6): cada pasada, salvo los extremos del recorrido, tiene exactamente
  una sucesora y una predecesora.
- (7) y (8): fijan la salida y el regreso al depósito.
- (9) y (10): si $s$ sucede a $r$, su posición es exactamente la siguiente.
- (11): ordena las distintas pasadas de una misma arista; por eso $(e,1)$ es
  realmente su primera pasada.
- (12) y (13): acotan las posiciones de las pasadas y las últimas posiciones
  auxiliares de los clusters. $L$ puede ser continua porque cada $X_{e^1}$ es
  entera.
- (14), (15) y (16): definen los dominios binarios.

Con peso finito, la separación entre clusters es blanda: una mezcla sigue
siendo factible, pero paga penalización. Si se requiere que un cluster termine
obligatoriamente antes de comenzar el otro, se fijan todas las variables $Q$
en cero; $O_{cd}$ sigue permitiendo que el modelo elija cuál va primero.

El modelo construye el camino:

$$
\alpha\rightarrow r_2\rightarrow\dots\rightarrow r_{K-1}
\rightarrow\beta.
$$

Antes de generar el modelo se debe validar que exista exactamente una pasada
de salida y una de llegada al depósito, que cada arista física tenga un cluster
válido y que ambos clusters de cada par de $H$ existan. Los conectores
de giro (`original_edge_id == -1`) y del depósito
(`original_edge_id == -2`) quedan fuera de $E^C$.

Si las dos orientaciones de una arista no dirigida representan el mismo
segmento físico, deben compartir cluster. Si se quiere penalizar una sola vez
por segmento independientemente del sentido, primero se deben agrupar por
`original_edge_id` y definir la primera pasada del grupo como el mínimo de las
primeras pasadas de sus orientaciones.

## Tamaño del bloque de orden entre clusters

Las restricciones (2), (3) y (4) agregan en total:

$$
    |E^C|
    +\sum_{\{c,d\}\in H}\left(|E_c|+|E_d|\right),
$$

Si $H$ contiene todos los pares de $C$ clusters no vacíos, el conteo es:

$$
    |E^C|+(C-1)|E^C|=C|E^C|.
$$

En la instancia de Acassuso hay $|E^C|=2480$ aristas activas y $C=8$
clusters, por lo que este bloque contiene

$$
    2480+7\cdot2480=19\,840
$$

restricciones y ocho variables continuas $L_c$.


## Ejemplos para las restricciones (3) y (4)

Las dos restricciones son simétricas. La variable $O_{cd}$ elige cuál de
ellas queda activa:

| Valor de $O_{cd}$ | Orden elegido | Restricción activa | Variables penalizadas |
|---|---|---|---|
| $1$ | $c$ debe terminar antes que $d$ | (3) | $Q_{cf}$ para $f\in E_d$ |
| $0$ | $d$ debe terminar antes que $c$ | (4) | $Q_{de}$ para $e\in E_c$ |

Como $1\leq X_r,L_c\leq K$, la mayor diferencia posible entre una última
posición y la posición de una pasada es $K-1$. Por eso un término $K$ en el
lado derecho alcanza para relajar por completo una desigualdad.

### Ejemplo 1: se elige que $c$ termine primero y no hay penalización

Supongamos $K=8$, $O_{cd}=1$ y las siguientes primeras pasadas:

$$
X_{a_1^1}=2,\qquad X_{a_2^1}=4,\qquad X_{f^1}=6,
$$

donde $a_1,a_2\in E_c$ y $f\in E_d$. El cluster $c$ termina su primera
cobertura en la posición $L_c=4$, antes de que $f$ aparezca en la posición
$6$.

Con $O_{cd}=1$, la restricción (3) se reduce a

$$
L_c-X_{f^1}\leq 8Q_{cf}.
$$

Si $Q_{cf}=0$, se obtiene

$$
4-6=-2\leq0.
$$

La desigualdad se cumple, así que el objetivo puede dejar $Q_{cf}=0$. La
arista $f$ no paga penalización porque aparece después de $L_c$, cuando todas
las aristas de $c$ ya tuvieron su primera pasada.

### Ejemplo 2: una arista de $d$ se adelanta y debe pagar penalización

Conservemos $K=8$ y $O_{cd}=1$, pero coloquemos la primera pasada de $f$ en
la posición $3$:

$$
X_{a_1^1}=2,\qquad X_{f^1}=3,\qquad X_{a_2^1}=4.
$$

La restricción (2) mantiene $L_c\geq4$; puede elegirse $L_c=4$. Si se
intentara usar $Q_{cf}=0$, la restricción (3) sería

$$
L_c-X_{f^1}=4-3=1\leq0,
$$

lo cual es falso. Por lo tanto, el modelo debe fijar $Q_{cf}=1$. Entonces:

$$
4-3=1\leq8,
$$

y la solución vuelve a ser factible, pero suma una unidad ponderada por
$w_{\{c,d\}}$ al objetivo. Que alguna primera pasada de $c$ ocurra después de
$f$ eleva $L_c$ por encima de $X_{f^1}$ y fuerza $Q_{cf}=1$.

La restricción (4) queda relajada en este caso: como $O_{cd}=1$, su lado
derecho contiene al menos $K$, incluso si $Q_{de}=0$.

### Ejemplo 3: se elige el orden opuesto

Ahora sea $O_{cd}=0$, es decir, el modelo elige que $d$ termine antes que
$c$. La restricción (3) queda relajada y la (4) se reduce a

$$
L_d-X_{e^1}\leq 8Q_{de}\qquad\forall e\in E_c.
$$

Supongamos que $d$ tiene dos aristas con primeras pasadas en las posiciones
$2$ y $5$. La restricción (2) permite tomar $L_d=5$.

- Si una arista $e\in E_c$ aparece por primera vez en la posición $7$, puede
  usarse $Q_{de}=0$, porque $L_d-X_{e^1}=5-7=-2\leq0$. No hay penalización.
- Si $e$ aparece en la posición $4$, se obtiene
  $L_d-X_{e^1}=5-4=1\nleq0$. En consecuencia, debe usarse $Q_{de}=1$ y esa
  aparición adelantada de $e$ paga penalización.

### Ejemplo 4: cómo el modelo elige el orden menos mezclado

Consideremos la secuencia de primeras pasadas

$$
c_1(2),\quad d_1(3),\quad c_2(4),\quad d_2(6),
$$

donde el número entre paréntesis es la posición. Si se elige $O_{cd}=1$,
la restricción (2) permite tomar $L_c=4$. Como $X_{d_1^1}=3<L_c$, la
restricción (3) fuerza $Q_{c d_1}=1$. En cambio,
$X_{d_2^1}=6\geq L_c$ permite $Q_{c d_2}=0$. La penalización total del par
es $1$.

Si se elige $O_{cd}=0$, puede tomarse $L_d=6$. Tanto
$X_{c_1^1}=2<L_d$ como $X_{c_2^1}=4<L_d$, por lo que la restricción (4)
fuerza $Q_{d c_1}=Q_{d c_2}=1$. La penalización total del par es $2$.

Con el mismo peso para ambas alternativas, el objetivo elige $O_{cd}=1$:
no elimina la mezcla, sino que la interpreta como una sola arista adelantada
en vez de dos.

En resumen, $L_c$ concentra en una sola variable la última primera pasada del
cluster $c$. Cuando una dirección está activa, $Q=0$ exige que la arista del
segundo cluster aparezca después de esa posición. Si esto no ocurre, $Q=1$
relaja la restricción y registra exactamente la penalización que usa el
objetivo.
