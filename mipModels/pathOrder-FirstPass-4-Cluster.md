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
$X_{e^1}$; no hace falta crear otra variable para ella. Un cluster $c$ terminó
su primera pasada cuando todas las aristas de $E_c$ ya aparecieron. La familia
$Q$ tiene siempre el mismo significado. La variable $O_{cd}$ determina cuál
de las dos direcciones se activa mediante las restricciones; las variables
$Q$ de la dirección no elegida quedan en cero por el objetivo.

## Modelo

$$
\begin{align}
    \min\quad
        & \sum_{\{c,d\}\in H}w_{\{c,d\}}
          \left(\sum_{f\in E_d}Q_{cf}
          +\sum_{e\in E_c}Q_{de}\right) \\
    \text{sujeto a}\quad
        & X_{a^1}-X_{f^1}
          \leq K\left(Q_{cf}+1-O_{cd}\right)
        && \forall\{c,d\}\in H,\ f\in E_d,\ a\in E_c \\
        & X_{b^1}-X_{e^1}
          \leq K\left(Q_{de}+O_{cd}\right)
        && \forall\{c,d\}\in H,\ e\in E_c,\ b\in E_d \\
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
- (2): queda activa cuando $O_{cd}=1$. Penaliza una arista de $d$ si aparece
  antes de que todas las aristas de $c$ hayan aparecido.
- (3): queda activa cuando $O_{cd}=0$ y aplica la misma definición $Q_{de}$
  a las aristas de $c$.
- (4) y (5): cada pasada, salvo los extremos del recorrido, tiene exactamente
  una sucesora y una predecesora.
- (6) y (7): fijan la salida y el regreso al depósito.
- (8) y (9): si $s$ sucede a $r$, su posición es exactamente la siguiente.
- (10): ordena las distintas pasadas de una misma arista; por eso $(e,1)$ es
  realmente su primera pasada.
- (11): acota las posiciones dentro del recorrido.
- (12), (13) y (14): definen los dominios binarios.

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


## Ejemplos para las restricciones (2) y (3)

Las dos restricciones son simétricas. La variable $O_{cd}$ elige cuál de
ellas queda activa:

| Valor de $O_{cd}$ | Orden elegido | Restricción activa | Variables penalizadas |
|---|---|---|---|
| $1$ | $c$ debe terminar antes que $d$ | (2) | $Q_{cf}$ para $f\in E_d$ |
| $0$ | $d$ debe terminar antes que $c$ | (3) | $Q_{de}$ para $e\in E_c$ |

Como $1\leq X_r\leq K$, la mayor diferencia posible entre dos posiciones es
$K-1$. Por eso un término $K$ en el lado derecho alcanza para relajar por
completo una desigualdad.

### Ejemplo 1: se elige que $c$ termine primero y no hay penalización

Supongamos $K=8$, $O_{cd}=1$ y las siguientes primeras pasadas:

$$
X_{a_1^1}=2,\qquad X_{a_2^1}=4,\qquad X_{f^1}=6,
$$

donde $a_1,a_2\in E_c$ y $f\in E_d$. El cluster $c$ termina su primera
pasada en la posición $4$, antes de que $f$ aparezca en la posición $6$.

Con $O_{cd}=1$, la restricción (2) se reduce a

$$
X_{a^1}-X_{f^1}\leq 8Q_{cf}\qquad\forall a\in E_c.
$$

Si $Q_{cf}=0$, para las dos aristas de $c$ se obtiene

$$
2-6=-4\leq0,\qquad 4-6=-2\leq0.
$$

Todas las desigualdades se cumplen, así que el objetivo puede dejar
$Q_{cf}=0$. La arista $f$ no paga penalización porque aparece después de que
todas las aristas de $c$ ya tuvieron su primera pasada.

### Ejemplo 2: una arista de $d$ se adelanta y debe pagar penalización

Conservemos $K=8$ y $O_{cd}=1$, pero coloquemos la primera pasada de $f$ en
la posición $3$:

$$
X_{a_1^1}=2,\qquad X_{f^1}=3,\qquad X_{a_2^1}=4.
$$

Si se intentara usar $Q_{cf}=0$, la desigualdad correspondiente a $a_2$
sería

$$
X_{a_2^1}-X_{f^1}=4-3=1\leq0,
$$

lo cual es falso. Por lo tanto, el modelo debe fijar $Q_{cf}=1$. Entonces:

$$
4-3=1\leq8,
$$

y la solución vuelve a ser factible, pero suma una unidad ponderada por
$w_{\{c,d\}}$ al objetivo. Basta con que exista una arista $a\in E_c$ cuya
primera pasada ocurra después de $f$ para forzar $Q_{cf}=1$.

La restricción (3) queda relajada en este caso: como $O_{cd}=1$, su lado
derecho contiene al menos $K$, incluso si $Q_{de}=0$.

### Ejemplo 3: se elige el orden opuesto

Ahora sea $O_{cd}=0$, es decir, el modelo elige que $d$ termine antes que
$c$. La restricción (2) queda relajada y la (3) se reduce a

$$
X_{b^1}-X_{e^1}\leq 8Q_{de}\qquad
\forall e\in E_c,\ b\in E_d.
$$

Supongamos que $d$ tiene dos aristas con primeras pasadas en las posiciones
$2$ y $5$.

- Si una arista $e\in E_c$ aparece por primera vez en la posición $7$, puede
  usarse $Q_{de}=0$, porque $2-7\leq0$ y $5-7\leq0$. No hay penalización.
- Si $e$ aparece en la posición $4$, la arista de $d$ ubicada en $5$ produce
  $5-4=1\nleq0$. En consecuencia, debe usarse $Q_{de}=1$ y esa aparición
  adelantada de $e$ paga penalización.

### Ejemplo 4: cómo el modelo elige el orden menos mezclado

Consideremos la secuencia de primeras pasadas

$$
c_1(2),\quad d_1(3),\quad c_2(4),\quad d_2(6),
$$

donde el número entre paréntesis es la posición. Si se elige $O_{cd}=1$,
el cluster $c$ termina en la posición $4$: $d_1$ se adelantó y fuerza
$Q_{c d_1}=1$, mientras que $d_2$ no se adelantó y permite
$Q_{c d_2}=0$. La penalización total del par es $1$.

Si se elige $O_{cd}=0$, el cluster $d$ termina en la posición $6$. Tanto
$c_1$ como $c_2$ aparecieron antes de ese final, por lo que se fuerzan
$Q_{d c_1}=Q_{d c_2}=1$. La penalización total del par es $2$.

Con el mismo peso para ambas alternativas, el objetivo elige $O_{cd}=1$:
no elimina la mezcla, sino que la interpreta como una sola arista adelantada
en vez de dos.

En resumen, cuando una dirección está activa, $Q=0$ exige que la arista del
segundo cluster aparezca después de todas las primeras pasadas del cluster
que debería terminar primero. Si esto no ocurre, $Q=1$ relaja la restricción
y registra exactamente la penalización que usa el objetivo.
