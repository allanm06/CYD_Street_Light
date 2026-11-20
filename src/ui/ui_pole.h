/*
 * ui_pole.h - Tela principal: tensao e corrente de um poste (Smart Street Light).
 *
 * As leituras sao feitas pela ESP medidora do poste e chegam via Wi-Fi/DDS.
 * O firmware converte a mensagem recebida em ui_pole_data_t e chama
 * ui_pole_update(). Se nenhuma leitura chegar em UI_POLE_STALE_MS, a tela
 * indica "Sem dados".
 *
 * Todas as funcoes devem ser chamadas da thread que roda lv_timer_handler().
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_POLE_STALE_MS 5000

typedef struct {
    float voltage_v;        /* tensao RMS (V) */
    float current_a;        /* corrente RMS (A) */
    uint32_t seq;           /* numero da amostra enviada pelo medidor */
} ui_pole_data_t;

/* Tocou no icone de Wi-Fi do cabecalho. */
typedef void (*ui_pole_wifi_cb_t)(void);

/* Cria a tela do poste e a carrega como tela ativa. */
void ui_pole_create(const char *pole_name);

void ui_pole_set_wifi_cb(ui_pole_wifi_cb_t cb);

/* Mostra a tela do poste (ex.: ao voltar da tela de Wi-Fi). */
void ui_pole_show(void);

/* Nova leitura do poste. */
void ui_pole_update(const ui_pole_data_t *d);

/* Estado do Wi-Fi para o icone do cabecalho (ssid NULL = desconectado). */
void ui_pole_set_wifi(bool connected, const char *ssid);

#ifdef __cplusplus
}
#endif
