#pragma once

constexpr int TOTAL_AMOSTRAS_MEDICAO = 5;
constexpr int MIN_AMOSTRAS_VALIDAS = 3;

struct ResultadoMedicao {
    float distancia;
    int amostrasValidas;
    int ecosRecebidos;
};

// Descarta leituras fora da faixa antes de calcular a mediana.
inline ResultadoMedicao filtrarMediana(const float* leituras, float distanciaMin,
                                      float distanciaMax) {
    ResultadoMedicao resultado = {-1.0f, 0, 0};
    float validas[TOTAL_AMOSTRAS_MEDICAO];

    for (int i = 0; i < TOTAL_AMOSTRAS_MEDICAO; i++) {
        const float distancia = leituras[i];
        if (distancia > 0.0f) {
            resultado.ecosRecebidos++;
        }
        if (distancia >= distanciaMin && distancia <= distanciaMax) {
            validas[resultado.amostrasValidas++] = distancia;
        }
    }

    if (resultado.amostrasValidas < MIN_AMOSTRAS_VALIDAS) {
        return resultado;
    }

    // Ordenação simples: o grupo tem no máximo cinco valores.
    for (int i = 1; i < resultado.amostrasValidas; i++) {
        const float valor = validas[i];
        int j = i - 1;
        while (j >= 0 && validas[j] > valor) {
            validas[j + 1] = validas[j];
            j--;
        }
        validas[j + 1] = valor;
    }

    const int meio = resultado.amostrasValidas / 2;
    resultado.distancia = validas[meio];
    if (resultado.amostrasValidas % 2 == 0) {
        resultado.distancia = (validas[meio - 1] + validas[meio]) / 2.0f;
    }
    return resultado;
}
