/*
 * ui_wifi.h - Tela LVGL de busca de redes Wi-Fi (CYD 320x240).
 *
 * Este modulo e apenas UI: nao depende do ESP-IDF. O firmware (ou o
 * LVGL Live Preview) fornece os resultados do scan via ui_wifi_show_results().
 *
 * No firmware, chame estas funcoes com o lock do LVGL adquirido
 * (ex.: lvgl_port_lock()/lvgl_port_unlock() do esp_lvgl_port).
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_WIFI_AUTH_OPEN = 0,
    UI_WIFI_AUTH_WEP,
    UI_WIFI_AUTH_WPA,
    UI_WIFI_AUTH_WPA2,
    UI_WIFI_AUTH_WPA_WPA2,
    UI_WIFI_AUTH_WPA3,
    UI_WIFI_AUTH_WPA2_WPA3,
    UI_WIFI_AUTH_ENTERPRISE,
    UI_WIFI_AUTH_OTHER,
} ui_wifi_auth_t;

typedef struct {
    char ssid[33];          /* string terminada em '\0'; vazia = rede oculta */
    uint8_t bssid[6];
    int8_t rssi;            /* dBm */
    uint8_t channel;
    ui_wifi_auth_t auth;
} ui_wifi_ap_t;

/* Chamado quando o usuario toca em "buscar". O firmware deve iniciar o scan. */
typedef void (*ui_wifi_scan_cb_t)(void);
/* Chamado quando o usuario toca em "Conectar" no detalhe de uma rede. */
typedef void (*ui_wifi_connect_cb_t)(const ui_wifi_ap_t *ap);

/* Cria a tela na tela ativa. */
void ui_wifi_create(void);

void ui_wifi_set_scan_cb(ui_wifi_scan_cb_t cb);
void ui_wifi_set_connect_cb(ui_wifi_connect_cb_t cb);

/* Mostra o estado "buscando..." (spinner, botao desabilitado). */
void ui_wifi_scan_started(void);

/* Entrega os resultados do scan. A lista e copiada e ordenada por RSSI. */
void ui_wifi_show_results(const ui_wifi_ap_t *aps, uint16_t count);

/* Mostra uma falha de scan no lugar da lista. */
void ui_wifi_show_error(const char *msg);

#ifdef __cplusplus
}
#endif
