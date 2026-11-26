/*
 * Leitura RMS do ZMCT103C (ver sensor_corrente.h para a ligacao).
 *
 * O TC entrega uma corrente AC = I_primario / 1000. No resistor de carga isso
 * vira uma tensao AC (datasheet: U = I/1000 * R), somada ao bias de ~1,65 V
 * para caber no ADC (0-3,3 V). A cada leitura amostramos um numero inteiro
 * de ciclos da rede, tiramos a media (o bias) e calculamos o RMS do resto.
 */
#include <Arduino.h>
#include <math.h>

#include "sensor_corrente.h"

/* ---- Ajustes do hardware ---------------------------------------------------
 * R de carga: com 330 R o pico de 3 A RMS da ~1,4 V, o maximo que cabe
 * em volta do bias. Para a lampada (~0,5 A) isso da ~150 mV RMS no ADC.
 * Corrente pequena demais? Passe o fio N vezes pelo furo (CORRENTE_VOLTAS = N).
 *   I_max(A RMS) ~= 1000 * 1,0 V / (R * N)                                   */
#define CORRENTE_R_CARGA   330.0f
#define CORRENTE_VOLTAS    1

/* Fator teorico A por V RMS no ADC. Para o modulo com LM358 (ou para
 * calibrar): ligue uma carga conhecida, compare com um alicate amperimetro
 * e multiplique este valor por (I_alicate / I_lido). */
#define CORRENTE_FATOR_A_POR_V  (1000.0f / (CORRENTE_R_CARGA * CORRENTE_VOLTAS))

/* Ruido do ADC sem corrente nenhuma (mV RMS). Meca com SENSOR_CORRENTE_DEBUG 1
 * e o circuito desligado, e coloque aqui; e descontado em quadratura. */
#define CORRENTE_RUIDO_MV  0.0f
#define CORRENTE_MINIMA_A  0.02f   /* abaixo disso reporta 0 */

#define CICLOS             10
#define AMOSTRA_US         200     /* 5 kHz = ~83 amostras por ciclo */

#define SENSOR_CORRENTE_DEBUG 0
/* ------------------------------------------------------------------------- */

void sensor_corrente_iniciar(void)
{
    analogReadResolution(12);
    analogSetPinAttenuation(CORRENTE_PINO, ADC_11db);  /* faixa ~0,15-3,1 V */
}

bool sensor_corrente_ler(float *corrente_a)
{
    const uint32_t janela_us = CICLOS * 1000000UL / REDE_HZ;
    int64_t soma = 0, soma_q = 0;
    uint32_t n = 0, mn = UINT32_MAX, mx = 0;

    uint32_t t0 = micros(), proxima = t0;
    while ((uint32_t)(micros() - t0) < janela_us) {
        while ((int32_t)(micros() - proxima) < 0) { }
        proxima += AMOSTRA_US;

        uint32_t mv = analogReadMilliVolts(CORRENTE_PINO);  /* ja calibrado (eFuse) */
        soma += mv;
        soma_q += (int64_t)mv * mv;
        if (mv < mn) mn = mv;
        if (mv > mx) mx = mv;
        n++;
    }
    if (n == 0) return false;

    double media_d = (double)soma / n;
    float media = (float)media_d;
    float var = (float)((double)soma_q / n - media_d * media_d);  /* mV^2, so a parte AC */
    float rms_mv = sqrtf(fmaxf(0.0f, var - CORRENTE_RUIDO_MV * CORRENTE_RUIDO_MV));

#if SENSOR_CORRENTE_DEBUG
    Serial.printf("[corrente] n=%lu media=%.0f mV min=%lu max=%lu rms=%.1f mV\n",
                  (unsigned long)n, media, (unsigned long)mn, (unsigned long)mx, sqrtf(fmaxf(0, var)));
#endif

    /* Bias longe do meio = divisor/TC desconectado; encostou nos extremos = saturou */
    if (media < 1000.0f || media > 2300.0f || mn < 80 || mx > 3150) return false;

    float i = rms_mv / 1000.0f * CORRENTE_FATOR_A_POR_V;
    *corrente_a = i < CORRENTE_MINIMA_A ? 0.0f : i;
    return true;
}
