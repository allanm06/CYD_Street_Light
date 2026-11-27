/*
 * Leitura RMS do modulo ZMCT103C (ver sensor_corrente.h para a ligacao).
 *
 * O OUT oscila em volta de 0 V e a ESP32 nao le tensao negativa: o semiciclo
 * negativo chega como 0. Numa senoide os dois semiciclos sao iguais, entao
 *   V_rms(onda inteira) = sqrt(2 * media(v^2))   com v = 0 no semiciclo cortado
 * amostrando um numero inteiro de ciclos da rede.
 *
 * Abaixo de ~150 mV o ADC da ESP32 (atenuacao 11 dB) nao distingue nada de 0,
 * entao correntes muito pequenas somem; isso tambem serve de filtro de ruido.
 */
#include <Arduino.h>
#include <math.h>

#include "sensor_corrente.h"

/* ---- Ajustes ---------------------------------------------------------------
 * Fator A por V RMS no pino. Calibrado com ferro de solda 25 W / 127 V
 * (~0,197 A). Para recalibrar: com uma carga conhecida (ou alicate amperimetro)
 * multiplique este valor por (I_real / I_lido).                               */
#define CORRENTE_FATOR_A_POR_V  0.404f

#define CORRENTE_ZERO_MV   150     /* leituras ate aqui = 0 V (piso do ADC) */
#define CORRENTE_MINIMA_A  0.02f   /* abaixo disso reporta 0 */
#define CORRENTE_SATURA_MV 3100    /* acima disso o ADC satura */

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
    int64_t soma_q = 0;
    uint32_t n = 0, mx = 0;

    uint32_t t0 = micros(), proxima = t0;
    while ((uint32_t)(micros() - t0) < janela_us) {
        while ((int32_t)(micros() - proxima) < 0) { }
        proxima += AMOSTRA_US;

        uint32_t mv = analogReadMilliVolts(CORRENTE_PINO);  /* ja calibrado (eFuse) */
        if (mv > mx) mx = mv;
        if (mv > CORRENTE_ZERO_MV) soma_q += (int64_t)mv * mv;
        n++;
    }
    if (n == 0) return false;

    float rms_mv = sqrtf(2.0f * (float)((double)soma_q / n));

#if SENSOR_CORRENTE_DEBUG
    Serial.printf("[corrente] n=%lu pico=%lu mV rms=%.1f mV\n",
                  (unsigned long)n, (unsigned long)mx, rms_mv);
#endif

    if (mx > CORRENTE_SATURA_MV) return false;

    float i = rms_mv / 1000.0f * CORRENTE_FATOR_A_POR_V;
    *corrente_a = i < CORRENTE_MINIMA_A ? 0.0f : i;
    return true;
}
