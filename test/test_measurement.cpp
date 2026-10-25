#include <cmath>
#include <iostream>
#include <limits>

#include "measurement_filter.h"

namespace {

int falhas = 0;
int casosExecutados = 0;

void verificarCaso(const char* nome,
                   const float (&leituras)[TOTAL_AMOSTRAS_MEDICAO],
                   float distanciaEsperada, int validasEsperadas,
                   int ecosEsperados) {
    ++casosExecutados;

    const ResultadoMedicao resultado = filtrarMediana(leituras, 25.0f, 50.0f);

    if (!std::isfinite(resultado.distancia) ||
        std::fabs(resultado.distancia - distanciaEsperada) > 0.0001f ||
        resultado.amostrasValidas != validasEsperadas ||
        resultado.ecosRecebidos != ecosEsperados) {
        ++falhas;
        std::cerr << "FALHA: " << nome << ": distancia=" << resultado.distancia
                  << " (esperada " << distanciaEsperada << "), validas="
                  << resultado.amostrasValidas << " (esperadas " << validasEsperadas
                  << "), ecos=" << resultado.ecosRecebidos << " (esperados "
                  << ecosEsperados << ")\n";
    }
}

}  // namespace

int main() {
    static_assert(TOTAL_AMOSTRAS_MEDICAO == 5, "Cada lote deve ter cinco amostras.");
    static_assert(MIN_AMOSTRAS_VALIDAS == 3,
                  "Uma medicao exige pelo menos tres amostras validas.");

    verificarCaso("mediana de cinco em ordem misturada",
                  {49.0f, 25.0f, 30.0f, 40.0f, 35.0f}, 35.0f, 5, 5);
    verificarCaso("mediana de quatro usa media dos valores centrais",
                  {40.0f, 30.0f, 45.0f, 25.0f, 0.0f}, 35.0f, 4, 4);
    verificarCaso("mediana de tres descarta timeout e distancia excessiva",
                  {42.0f, -1.0f, 28.0f, 60.0f, 35.0f}, 35.0f, 3, 4);
    verificarCaso("limites minimo e maximo sao inclusivos",
                  {25.0f, 50.0f, 40.0f, 24.9f, 50.1f}, 40.0f, 3, 5);
    verificarCaso("duas validas nao bastam",
                  {28.0f, 35.0f, 0.0f, -1.0f, 24.0f}, -1.0f, 2, 3);
    verificarCaso("valores pouco fora da faixa sao descartados",
                  {24.9f, 25.0f, 50.0f, 50.1f, -1.0f}, -1.0f, 2, 4);
    verificarCaso("todos os disparos sem eco",
                  {-1.0f, -1.0f, -1.0f, 0.0f, 0.0f}, -1.0f, 0, 0);
    verificarCaso("distancias muito curtas nao viram caixa cheia",
                  {1.0f, 2.0f, 3.0f, 4.0f, 5.0f}, -1.0f, 0, 5);
    verificarCaso("distancias excessivas nunca estimam caixa cheia",
                  {250.0f, 250.0f, 250.0f, 250.0f, 250.0f}, -1.0f, 0, 5);
    verificarCaso("pico isolado nao altera valor central",
                  {30.0f, 31.0f, 32.0f, 33.0f, 49.0f}, 32.0f, 5, 5);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinito = std::numeric_limits<float>::infinity();
    verificarCaso("valores nao finitos nao completam quorum",
                  {nan, infinito, -infinito, 35.0f, 45.0f}, -1.0f, 2, 3);
    verificarCaso("tres validas sobrevivem a NaN e infinito",
                  {nan, infinito, 25.0f, 30.0f, 50.0f}, 30.0f, 3, 4);

    if (falhas != 0) {
        std::cerr << falhas << " de " << casosExecutados << " casos falharam.\n";
        return 1;
    }

    std::cout << casosExecutados << " casos de medicao passaram.\n";
    return 0;
}
