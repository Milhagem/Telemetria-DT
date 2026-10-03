#pragma once

#include <stdint.h>

struct Amostra {
    uint64_t tempo_us;

    float corrente_A;
    float tensao_V;
    float potencia_W;
    float energia_J;
};