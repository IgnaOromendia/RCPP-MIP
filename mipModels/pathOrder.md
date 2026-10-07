Constantes
$$
\begin{align*}
    m_e &= X^*_e + Y^*_e\\
    K &= \sum_{e\in E} m_e \\
    S &= \{e,f \in E \times E / e = (x,v) \land f = (v,y) \}
\end{align*}
$$
Variables
$$
\begin{align*}
    Z_{e}^t &= 
    \begin{cases}
        1 & \text{si la arista } e \text{ tiene la posición } t \\
        0 & \text{caso contrario}
    \end{cases} \\
    A_{et} &= 
    \begin{cases}
        1 & \text{si } t \text{ es la primer aparición de la arista } e \\
        0 & \text{caso contrario}
    \end{cases} \\
    X_e &= \text{primer posición en la que aparece e}
\end{align*}
$$

Modelo:
$$
\begin{align}
    \min \quad & \sum  D_{ef} \\ 
    \text{sujeto a} \quad & D_{ef} \geq X_e - X_f  & \forall e,f \in S\\ 
    & D_{ef} \geq X_f - X_e  & \forall e,f \in S\\ 
    & \sum_{t=1}^{K} Z_e^t = m_e & \forall e \in E \\ 
    & \sum_{e\in E} Z_e^t = 1 &  t=1\dots K \\ 
    & \sum_{e\in \delta^-(v)} Z_e^t = \sum_{f\in \delta^+(v)} Z_f^{t+1} & \forall v\in V(G), t=1\dots K-1 \\
    & \sum_{e\in \delta^+(0)} Z_e^1 = 1 \\
    & \sum_{e\in \delta^-(0)} Z_e^1 = 1 \\
    & \sum_{t=1}^K A_{et} = 1 &  \forall e\in E \\ 
    & A_{et} \leq Z_e^t & \forall e \in E, \text{, } t=1\dots K \\ 
    & Z_e^t \leq \sum_{i=1}^t A_{ei} & \forall e, t = 1\dots K \\
    & X_e = \sum_{t=1}^K tA_{et} & \forall e \\
    & D_{ef} \geq 0 & \forall e, f \\
    & Z^t_e \in \{0,1\} & \forall e, k, t \\
\end{align}
$$

- (2) y (3): Módudlo
- (4): Cantidad de apariciones
- (5): Cada posición contiene una arista
- (6): Continuidad
- (7): Salida del depóisto
- (8): Llegada del Deposito
- (9): Cada arista tiene únicamente una única primer aparición
- (10): Si es primer aparición es porque paso por esa arista
- (11): No puede aparece e antes de la aparición marcada como primera
- (12): Definimos X
