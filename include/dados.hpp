#pragma once

#include <stdint.h>

struct Amostra {
    uint64_t tempo_us;

    float corrente_A;
    float tensao_V;
    float potencia_W;
    float energia_J;
};

/*essa é a estrutura que os dados vão se organizar, tem os dados de corrente e tals, 
mais o momento q foi lido, para dps fazer o gráfico*/