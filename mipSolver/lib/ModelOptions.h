#ifndef MODEL_OPTIONS_H
#define MODEL_OPTIONS_H

#include <cmath>
#include <stdexcept>

struct ModelOptions {
    double capacity = 10000;
    int max_traversals = 10000;

    void validate() const {
        if (!std::isfinite(capacity) || capacity <= 0)
            throw std::invalid_argument("capacity debe ser positiva y finita");
        if (max_traversals < 0)
            throw std::invalid_argument("max-traversals debe ser un entero no negativo");
    }
};

#endif
