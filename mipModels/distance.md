$$
\begin{align*}
    X^p_{ij} &= 
    \begin{cases}
        1 & \text{si el eje } ij \text{ fue satisfecho en el viaje } p \\
        0 & \text{caso contrario}
    \end{cases} \\
    Y^p_{ij} &= \text{Cantidad de veces que el eje } ij \text{ fue recorrido sin recoger en el viaje } p \\
    F^p_{ij} &= \text{Flujo del eje } ij \text{ en el viaje } p \\
\end{align*}
$$

$$
\begin{align}
    \min \quad & \sum^P_{p=1}(\sum_{ij\in R}t_{ij}X^p_{ij} + \sum_{ij\in S}t_{ij}Y^p_{ij}) \\
    \text{sujeto a} \quad & \sum_{j/ij\in S} Y^p_{ij} + \sum_{j/ji\in R} X^p_{ij} = \sum_{j/ji\in S} Y^p_{ji} + \sum_{j/ij\in R} X^p_{ji} & \forall i \text{ } \forall p \\
    & \sum_{p=1} X_{ij}^p = 1 & \forall ij \in R \cap A \\
    & \sum_{p=1} (X_{ij}^p + X_{ji}^p) = 1 & \forall ij \in R \cap E \\
    & \sum_{j:(0j)\in S} Y^p_{0j} + \sum_{j:(0j)\in R} X^p_{0j} \leq 1 & \forall p \\
    & \sum_{j:(ji)\in S} F^p_{ji} - \sum_{j:(ij)\in R} F^p_{ij} = \sum_{j:(ji)\in R}q_{ij} X_{ji}^P & \forall i \text{ } \forall p \\
    & \sum_{j:(0j) \in S} F^p_{0j} = \sum_{(ij)\in R} q_{ij}X_{ij}^p & \forall i \forall p \\
    & \sum_{i:(i0) \in S} F^p_{i0} = \sum_{i:(i0)\in R} q_{i0}X_{i0}^p & \forall p \\
    & F^p_{ij} \leq W(y^p_{ij} + x^p_{ij}) & \forall(i,j) \in S, \forall p \\
    & X^p_{ij} \in \{0,1\} & \forall(i,j) \in R, \forall p \\
    & F^p_{ij} \geq 0 & \forall(i,j) \in S, \forall p \\
    & Y^p_{ij} \geq 0 \text{ entera} & \forall(i,j) \in S, \forall p
\end{align}
$$