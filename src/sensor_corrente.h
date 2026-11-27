/*
 * Sensor de corrente AC: modulo ZMCT103C de 3 pinos (HW-670: VCC, GND, OUT).
 *
 * Ligacao:  VCC -> alimentacao do modulo, GND -> GND, OUT -> CORRENTE_PINO.
 * A fase (ou o neutro) da carga passa por dentro do furo do TC.
 *
 * O OUT desse modulo oscila em volta de 0 V (sem bias no meio da faixa), entao
 * a ESP32 so enxerga o semiciclo positivo; a leitura usa isso (ver o .cpp).
 * Com o ganho atual do trimpot: ~3,3 V de pico por ampere, ou seja, cabe ate
 * ~0,9 A. Para mais corrente, baixe o ganho no trimpot e recalibre.
 */
#pragma once

#include <stdbool.h>

/* Tem que ser ADC1 (GPIO32-39): o ADC2 nao funciona com o Wi-Fi ligado. */
#define CORRENTE_PINO      34

#define REDE_HZ            60      /* rede eletrica no Brasil */

void  sensor_corrente_iniciar(void);

/* Mede ~10 ciclos da rede (~167 ms, bloqueante) e devolve a corrente RMS em A.
 * Retorna false se o sinal saturar o ADC (corrente alta demais para o ganho). */
bool  sensor_corrente_ler(float *corrente_a);
