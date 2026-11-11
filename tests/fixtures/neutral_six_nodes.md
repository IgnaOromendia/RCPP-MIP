# Caso de seis nodos con una calle compartida entre neutros

Caso usado por `check_six_node_boundary_expansion()` en
`tests/neutral_expanded_graph_test.cpp`.
Los IDs de este documento empiezan en 1. Todas las calles son no dirigidas.

## Grafo original

- Nodos interiores de C1: **1 y 2**.
- Nodos neutros, incidentes a C1 y C2: **3 y 4**.
- Nodos interiores de C2: **5 y 6**.
- Un vehículo y acceso al depósito por el nodo 1. El depósito es sintético y
  no se cuenta entre los seis nodos físicos.

```text
         1 ─────── 2              interior de C1
        /          \
       3 ────────── 4             neutros; calle 3–4 original de C1
        \          /
         5 ─────── 6              interior de C2
```

| Registro | Extremos | Cluster | Zona | Costo | Demanda |
| --- | --- | --- | --- | --- | --- |
| 1 | 3–1 | C1 | 1 | 1 | 1 |
| 2 | 1–2 | C1 | 1 | 2 | 1 |
| 3 | 2–4 | C1 | 1 | 3 | 1 |
| 4 | 3–5 | C2 | 1 | 4 | 1 |
| 5 | 5–6 | C2 | 1 | 5 | 1 |
| 6 | 6–4 | C2 | 1 | 6 | 1 |
| 7 | 3–4 | C1 | 1 | 7 | 1 |

La zona 1 asigna el servicio al único vehículo; el cluster es una asignación
independiente. No hay giros prohibidos adicionales. Se mantiene la prohibición
habitual de vueltas en U.

Contenido de `graph.dat`, con cabecera `V N D E A`, nodos de acceso al depósito
y registros `origen destino zona costo demanda`:

```text
1 6 1 7 0
1
3 1 1 1 1
1 2 1 2 1
2 4 1 3 1
3 5 1 4 1
5 6 1 5 1
6 4 1 6 1
3 4 1 7 1
```

## Expansión esperada

Los nodos 3 y 4 conservan sus IDs en C1 y reciben los IDs 7 y 8 en C2.
Quedan **ocho nodos físicos: cuatro interiores y cuatro representaciones
neutrales**. El depósito sintético pasa al ID 9.

```text
         1 ─────── 2
        /          \
     3_C1 ──────── 4_C1          calle original requerida, costo 7
       ⇅            ⇅           transiciones entre clusters
     3_C2 ──────── 4_C2          copia opcional, costo 7
        \          /
         5 ─────── 6
```

Las calles físicas de C1 son `3–1`, `1–2`, `2–4` y `3–4`.
Las calles físicas de C2 son `7–5`, `5–6`, `6–8` y la copia **`7–8`**.
Sólo la copia tiene zona 0 y demanda 0; conserva costo 7 y ambos sentidos.
El servicio se exige sobre los siete registros originales, una vez cada uno.

Las únicas transiciones neutrales son `3 → 7`, `7 → 3`, `4 → 8` y `8 → 4`.
Cada una cuesta `13 + 15 = 28` en el grafo expandido, calculado con los
registros originales. El objetivo de cruces les asigna un uso por transición.

El circuito interno de C2 `5 → 6 → 8 → 7 → 5` usa la copia y tiene **cero
cruces**. En cambio, pasar de la calle `2 → 4` de C1 a la copia `8 → 7` de
C2 utiliza la transición `4 → 8` y cuenta **un cruce**.

Al contraer, dos recorridos de la original `3–4` y tres de su copia `7–8`
deben sumar cinco recorridos del registro original 7. Las transiciones entre
clusters y los accesos al depósito no aparecen en la salida por registros.

La regresión verifica estos nodos, calles, transiciones, servicio único y
multiplicidades contraídas sin resolver un MIP.
