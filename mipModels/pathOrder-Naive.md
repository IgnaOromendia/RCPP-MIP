Constantes
$$
\begin{align*}
    m_e &= X^*_e + Y^*_e\\
    K &= \sum_{e\in E} m_e
\end{align*}
$$
Variables
$$
\begin{align*}
    Z^t_{ek} &= 
    \begin{cases}
        1 & \text{si la k-ésima pasada por } e \text{ fue está en la posición } t \\
        0 & \text{caso contrario}
    \end{cases} \\
    X_{ek} &= \text{Posición de la k-ésima pasada por } e \\
\end{align*}
$$

Modelo:
$$
\begin{align}
    \min \quad & \sum  D_{ef} \\ 
    \text{sujeto a} \quad & D_{ef} \geq X_{e1} - X_{f1}  & \forall e,f \in E(G) / e = (x,v) \land f = (v,y)\\ 
    & D_{ef} \geq X_{f1} - X_{e1}  & \forall e,f \in E(G) / e = (x,v) \land f = (v,y)\\ 
    & \sum_{t=1}^{K} Z_{ek}^t = 1 & \forall e \text{, } k=1\dots m_e \\ 
    & \sum_{e\in E} \sum_{k=1}^{m_e} Z_{ek}^t = 1 &  t=1\dots K \\ 
    & \sum_{e\in \delta^-(v)}\sum^{m_e}_{k=1} Z_{ek}^t = \sum_{f\in \delta^+(v)}\sum^{m_f}_{k=1} Z_{fk}^{t+1} & \forall v\in V(G), t=1\dots K-1 \\
    & \sum_{e\in \delta^-(0)}\sum^{m_e}_{k=1} Z_{ek}^K = \sum_{f\in \delta^+(0)}\sum^{m_f}_{k=1} Z_{fk}^1 \\
    & \sum_{e\in \delta^+(0)} \sum_{k=1}^{m_e} Z_{ek}^1 = 1 \\
    & X_{ek} = \sum^K_{t=1} t Z_{ek}^t & \forall e,k \\
    & X_{ek} + 1 \leq X_{ek+1} & \forall e \text{, } k=1\dots m_e -1 \\ 
    & X_{ek} \geq 0 & \forall e, k \\
    & D_{ef} \geq 0 & \forall e, f \\
    & Z^t_{ek} \in \{0,1\} & \forall e, k, t \\
\end{align}
$$

- (2) y (3): Módudlo
- (4): Cada pasada ocupa una posición
- (5): Cada posición contiene una pasada
- (6): Continuidad
- (7): Circuito cerrado
- (8): Deposito
- (9): Definimos la posición
- (10): Orden de pasadas
