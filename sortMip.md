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
    \text{sujeto a} \quad & X_{ek} + 1 \leq X_{ek+1} & \forall e \text{, } k=1\dots m_e -1 \\ 
    & D_{ef} \geq X_{e1} - X_{f1}  & \forall e,f \in E(G) / e = (x,v) \land f = (v,y)\\ 
    & D_{ef} \geq X_{f1} - X_{e1}  & \forall e,f \in E(G) / e = (x,v) \land f = (v,y)\\ 
    & \sum_{t=1}^{K} Z_{ek}^t = 1 & \forall e \text{, } k=1\dots m_e \\ 
    & \sum_{e\in E} \sum_{k=1}^{m_e} Z_{ek}^t = 1 &  t=1\dots K \\ 
    & X_{ek} = \sum^K_{t=1} t Z_{ek}^t & \forall e,k \\
    & \sum_{e\in \delta^-(v)}\sum^{m_e}_{k=1} Z_{ek}^t = \sum_{f\in \delta^+(v)}\sum^{m_f}_{k=1} Z_{fk}^{t+1} & \forall v\in V(G), t=1\dots K-1 \\
    & \sum_{e\in \delta^-(v)}\sum^{m_e}_{k=1} Z_{ek}^K = \sum_{f\in \delta^+(v)}\sum^{m_f}_{k=1} Z_{fk}^1 & \forall v\in V(G) \\
    & \sum_{e\in \delta^+(0)} \sum_{k=1}^{m_e} Z_{ek}^1 = 1 &  t=1\dots K 
\end{align}
$$

- (2): Orden de pasadas
- (3) y (4): Módudlo
- (5): Cada pasada ocupa una posición
- (6): Cada posición contiene una pasada
- (7): Definimos la posición
- (8): Continuidad
- (9): Circuito cerrado
- (10): Deposito
