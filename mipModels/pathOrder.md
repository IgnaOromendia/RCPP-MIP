# Orden de las pasadas

El modelo considera un único vehículo.

## Constantes

$$
\begin{align*}
    m_e &= X^*_e + Y^*_e \\
    K &= \sum_{e\in E}m_e \\
    R &= \{(e,k):e\in E \land k=1\dots m_e\},
         & |R|=K \\
    S &= \{(e,f)\in E\times E:
           e=(x,v)\land f=(v,y)\}
\end{align*}
$$

Los conectores del depósito ya forman parte de $E$. La solución de entrada
contiene exactamente una pasada que sale del depósito y una que regresa a él.
Definimos esas pasadas reales como:

$$
\begin{align*}
    \alpha &\in R,
        & e_\alpha&=(d,v) \\
    \beta &\in R,
        & e_\beta&=(v,d)
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
    && \forall (r,s)\in T \\
    X_r &= \text{posición de la pasada }r\text{ en el recorrido},
    && \forall r\in R \\
    D_{rs} &= |X_r-X_s|,
    && \forall r,s\in P
\end{align*}
$$

## Modelo

$$
\begin{align}
    \min\quad
        & \sum_{r,s\in P}D_{rs} \\
    \text{sujeto a}\quad
        & D_{rs}\geq X_r-X_s
        && \forall r,s\in P \\
        & D_{rs}\geq X_s-X_r
        && \forall r,s\in P \\
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
        & D_{rs}\geq0
        && \forall r,s\in P \\
        & Z_{rs}\in\{0,1\}
        && \forall (r,s)\in T
\end{align}
$$

- (2) y (3): definen el módulo de la diferencia entre primeras pasadas.
- (4): cada pasada, salvo la llegada al depósito, tiene una sucesora.
- (5): cada pasada, salvo la salida del depósito, tiene una predecesora.
- (6): la pasada que sale del depósito ocupa la primera posición.
- (7): la pasada que vuelve al depósito ocupa la última posición.
- (8) y (9): si $s$ sucede a $r$, su posición es exactamente la siguiente.
- (10): ordena las distintas pasadas de una misma arista.
- (11): acota las posiciones dentro del recorrido.
- (12): impone la no negatividad de las distancias.
- (13): define el dominio binario de las transiciones.

El modelo construye el camino:

$$
\alpha\rightarrow r_2\rightarrow\dots\rightarrow r_{K-1}
\rightarrow\beta.
$$

Antes de generar el modelo se debe validar que exista exactamente una pasada
de salida y una de llegada al depósito. Los conectores del depósito se
reconocen en `PathSortInstance` por `original_edge_id == -2`.
