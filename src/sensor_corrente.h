/*
 * Sensor de corrente AC com o transformador de corrente ZMCT103C (1000:1, 0-10 A).
 *
 * Ligacao (TC "puro", figura II do datasheet, com bias para o ADC da ESP32):
 *
 *   3V3 --[10k]--+--[10k]-- GND        divisor = meio da escala (~1,65 V)
 *                |  (+ 10uF ao GND)
 *                +---- TC pino 2
 *   TC pino 1 ---+---- CORRENTE_PINO (ADC1)
 *                |
 *   TC pino 2 --[R carga = CORRENTE_R_CARGA]-- TC pino 1   (resistor em paralelo com o TC)
 *
 * A fase (ou o neutro) da lampada passa por dentro do furo do TC.
 *
 * Se for o modulo pronto "ZMCT103C" (com LM358 e trimpot), ligue OUT no
 * CORRENTE_PINO e acerte CORRENTE_FATOR_A_POR_V pela calibracao (ver .cpp).
 */
#pragma once

#include <stdbool.h>

/* Tem que ser ADC1 (GPIO32-39): o ADC2 nao funciona com o Wi-Fi ligado. */
#define CORRENTE_PINO      34

#define REDE_HZ            60      /* rede eletrica no Brasil */

void  sensor_corrente_iniciar(void);

/* Mede ~10 ciclos da rede (~167 ms, bloqueante) e devolve a corrente RMS em A.
 * Retorna false se o sinal estiver fora da faixa do ADC (fio solto / saturado). */
bool  sensor_corrente_ler(float *corrente_a);
