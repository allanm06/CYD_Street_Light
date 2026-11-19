/*
 * ui_wifi.h - Tela LVGL de busca e conexao a redes Wi-Fi (CYD 320x240).
 *
 * Este modulo e apenas UI: nao depende do hardware. O firmware (ou o
 * LVGL Live Preview) faz o scan/conexao e devolve os resultados por aqui.
 *
 * Todas as funcoes devem ser chamadas da mesma thread que roda
 * lv_timer_handler().
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
    int8_t rssi;            /* dBm (usado so para ordenar e para as barras) */
    uint8_t channel;
    ui_wifi_auth_t auth;
} ui_wifi_ap_t;

/* Usuario tocou em "buscar". O firmware deve iniciar o scan e depois chamar
 * ui_wifi_show_results() ou ui_wifi_show_error(). */
typedef void (*ui_wifi_scan_cb_t)(void);

/* Usuario pediu para conectar. password e "" para redes abertas.
 * O firmware deve tentar conectar e depois chamar ui_wifi_connect_result(). */
typedef void (*ui_wifi_connect_cb_t)(const ui_wifi_ap_t *ap, const char *password);

/* Usuario tocou em voltar no cabecalho. */
typedef void (*ui_wifi_back_cb_t)(void);

/* Cria a tela (sem carrega-la). */
void ui_wifi_create(void);
/* Carrega a tela de Wi-Fi; na primeira vez dispara uma busca. */
void ui_wifi_open(void);

void ui_wifi_set_scan_cb(ui_wifi_scan_cb_t cb);
void ui_wifi_set_connect_cb(ui_wifi_connect_cb_t cb);
void ui_wifi_set_back_cb(ui_wifi_back_cb_t cb);

/* --- Scan --- */
void ui_wifi_scan_started(void);
void ui_wifi_show_results(const ui_wifi_ap_t *aps, uint16_t count);
void ui_wifi_show_error(const char *msg);

/* --- Conexao --- */
/* Resultado da tentativa iniciada pelo connect_cb. msg e opcional
 * (ex.: "IP 192.168.0.10" ou "Senha incorreta"). Use apenas ASCII. */
void ui_wifi_connect_result(bool ok, const char *msg);

/* Informa a rede conectada no momento (NULL ou "" = nenhuma). */
void ui_wifi_set_connected(const char *ssid);

#ifdef __cplusplus
}
#endif
