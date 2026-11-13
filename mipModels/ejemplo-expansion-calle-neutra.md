# Ejemplo de expansión de una calle neutra

Este ejemplo muestra el comportamiento propuesto en el spec.

El grafo tiene cuatro nodos y tres calles. La calle **A–B** es la frontera compartida por los clusters 1 y 2. En el archivo de entrada, A–B es un único tramo físico con etiqueta `-1`.

```text
X ── cluster 1 ── A ── calle neutra ── B ── cluster 2 ── Y
```

## Grafo expandido

El solver crea dos copias de A y dos de B. La misma calle A–B queda disponible en ambos clusters. Las flechas verticales son conectores entre copias de la misma esquina.

```text
Cluster 1    X ───── A₁ ────── B₁
                     ↕         ↕
Cluster 2           A₂ ────── B₂ ───── Y
```

| Movimiento | Cruces |
| --- | ---: |
| Recorrer X → A₁ o B₂ → Y | 0 |
| Recorrer A₁ → B₁ o A₂ → B₂ por la calle neutra | 0 |
| Cambiar de copia en A o B con un conector, si el giro está permitido | 1 por uso |

El camión puede recorrer **X → A₁ → B₁** con **0 cruces**. Si desde B₁ sigue por **B₁ → B₂ → Y**, suma **1 cruce**, justo en el conector B₁ → B₂. También puede cambiar en A y hacer **X → A₁ → A₂ → B₂ → Y**, con **1 cruce** en A₁ → A₂.

Las dos copias de A–B representan la misma calle física. Cada pasada suma su distancia y, si el tramo es obligatorio, se presta servicio una sola vez entre las dos copias.

[Spec de calles de frontera compartidas](spec-calles-de-frontera-compartidas.md)
