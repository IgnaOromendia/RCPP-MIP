### Corte conectividad

Propuesta:
$$
\sum_{ij\in\delta^-(S)} X_{ij} + Y_{ij} \geq 1
$$

Motivación: Fortalecer el relajamiento

Tenemos:
- $F_{ij} \leq W (X_{ij} + Y_{ij})$ 
- $\sum_{j:(ji)\in E} F_{ji} - \sum_{j:(ij)\in R} F_{ij} = \sum_{j:(ji)\in R}q_{ij} X_{ji}  $

Luego:

$F_{ij} \geq 0$ entonces $\sum_{j:(ij)\in R} F_{ij} \geq 0$ 

Por lo tanto

$\sum_{j:(ji)\in E} F_{ji} \geq \sum_{j:(ji)\in R}q_{ij} X_{ji} $

En partiuclar para todo $S \subseteq R$ donde $q(S) = \sum_{j:(ji)\in S}q_{ij} X_{ji}$

Usando $F_{ij} \leq W (X_{ij} + Y_{ij})$ Tenemos que 

$$
W \sum_{ij\in\delta^-(S)} X_{ij} + Y_{ij} \geq q(s)
$$

Lo que equivale a

$$
\sum_{ij\in\delta^-(S)} X_{ij} + Y_{ij} \geq \frac{q(S)}{W}
$$

Si $\frac{q(S)}{W} < 1$ estamos relajando el modelo pero sabemos qu

$$
\sum_{ij\in\delta^-(S)} X_{ij} + Y_{ij} \geq 1
$$
