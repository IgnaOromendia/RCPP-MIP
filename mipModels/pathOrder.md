Constantes
$$
\begin{align*}
    m_e &= X^*_e + Y^*_e\\
    K &= \sum_{e\in E} m_e \\
    S &= \{(e,f): e,f \in E \times E \land e = (x,v) \land f = (v,y) \} \\
    R &= \{(e,k): e \in E \land k = 1\dots m_e\} \rightarrow |R| = K \\
    \alpha &= ((d,k), 1) \in R \text{, salida del depósito} \\
    \beta &= ((k,d), 1) \in R \text{, llegada al depósito} \\
    T &= \{(r,s): r,s \in R \land (e_r, e_s) \in S\} 
\end{align*}
$$

Sea $r = (e,k) \in R$ \
Definimos:
- $r' \in R$ como $(e, k + 1)$
- $r^p \in R$ como $(e, 1)$
    - $P = \{r^p: r \in R\}$  

Variables
$$
\begin{align*}
    Z_{rs} &= 
    \begin{cases}
        1 & \text{si } s \text{ le sigue inmediatamente después a } r \\
        0 & \text{caso contrario}
    \end{cases} \\
    X_{r} &= \text{posición de la pasada } r \text{ en el recorrido}
\end{align*}
$$

Modelo:
$$
\begin{align}
    \min \quad & \sum_{r,s \in P}  D_{rs} \\ 
    \text{sujeto a} \quad & D_{rs} \geq X_r - X_s  & \forall r,s \in P \\ 
    & D_{rs} \geq X_s - X_r  & \forall r,s \in P \\ 
    & \sum_{s:(r,s)\in T} Z_{rs} = 1 & \forall r \in R \\
    & \sum_{r:(r,s)\in T} Z_{rs} = 1 & \forall s \in R \\
    & \sum_{s:(\alpha,s)\in T} Z_{\alpha s} = 1 & \forall s \in R \\
    & \sum_{r:(r,\beta)\in T} Z_{r\beta} = 1 & \forall r \in R \\
    & X_s \geq X_r + 1 - K(1 - Z_{rs}) & \forall r,s \in T \\
    & X_s \leq X_r + 1 + K(1 - Z_{rs}) & \forall r,s \in T \\
    & X_r + 1 \leq X_{r'} & \forall r \in R \\
    
    & D_{ef} \geq 0 & \forall e, f \in T \\
\end{align}
$$

- (2) y (3): Módudlo
- (4): Exactamente una sucesora
- (5): Exactamente una predecesora
- (6): Salida del depósito
- (7): Llegada al depósito
- (8): Si $s$ sucede a $r$ su posicion debe ser mayor a la de $s$
- (9): Si $s$ sucede a $r$ su posicion no debe superar a la de $s$ por más de 1
- (10): Orden de pasadas
- (11): 
- (12): 
- (13): 
