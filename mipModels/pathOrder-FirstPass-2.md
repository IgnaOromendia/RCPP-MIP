Constantes
$$
\begin{align*}
    m_e &= X^*_e + Y^*_e\\
    K &= \sum_{e\in E} m_e \\
    S &= \{e,f \in E \times E / e = (x,v) \land f = (v,y) \} \\
    E^1 &= \{ e \in E / m_e = 1\} \\
    E^+ &= \{ e \in E / m_e > 1\} 
\end{align*}
$$
Variables
$$
\begin{align*}
    Z_e^t &= 
    \begin{cases}
        1 & \text{si la arista } e \text{ tiene la posición } t \\
        0 & \text{caso contrario}
    \end{cases} \\
    A^t_e &= 
    \begin{cases}
        1 & \text{si } e \text{ ya apareció para la posición } t \\
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
    & \sum_{e\in \delta^-(0)} Z_e^K = 1 \\
    & X_e = \sum_{t=1}^K tZ^t_{e} & \forall e \in E^1 \\
    & A^t_e \geq A^{t-1}_e & \forall e \in E^+ \\   
    & A^t_e \geq Z^t_e & \forall e \in E^+ \\
    & A^t_e \leq A^{t-1}_e + Z^t_e & \forall e \in E^+ \\
    & X_e = K + 1 - \sum_{t=0}^K A^t_e & \forall e \in E^+ \\
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
- (9): Definimos X para $E^1$
- (10): Continuidad de posiciones
- (11): Si aparece por primera vez se debe recorrer
- (12): Si se recorre en t o apreció entre 1 y t-1 aparece entre 1 y t
- (13): Definimos X para $E^+$
