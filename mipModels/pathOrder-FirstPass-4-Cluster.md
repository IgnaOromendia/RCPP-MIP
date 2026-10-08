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
    w_{\{c,d\}} &>0
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

- (1): minimiza la cantidad ponderada de aristas adelantadas según el orden
  que el propio modelo elige para cada par.
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
