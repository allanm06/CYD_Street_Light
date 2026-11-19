/*
 * ui_wifi_preview.c - Ponto de entrada do LVGL Live Preview (VS Code).
 *
 * Simula scan e conexao Wi-Fi com dados falsos. No firmware o arquivo
 * compila vazio (LVGL_LIVE_PREVIEW so existe no preview).
 * Ajuste o preview para 320x240. Senha que "conecta": 12345678.
 */
#ifdef LVGL_LIVE_PREVIEW

#include "ui_wifi.h"
#include <stdio.h>
#include <string.h>

static const ui_wifi_ap_t mock_aps[] = {
    {"Arkmeds_Corp",       {0x48,0xAF,0xF3,0x10,0x22,0x01}, -48,  6, UI_WIFI_AUTH_WPA2},
    {"CYD-StreetLight-AP", {0x48,0xAF,0xF3,0x6F,0xC7,0xBD}, -39,  1, UI_WIFI_AUTH_WPA2_WPA3},
    {"VIVOFIBRA-5G2A",     {0xA4,0x33,0xD7,0x5E,0x01,0x9C}, -63, 11, UI_WIFI_AUTH_WPA_WPA2},
    {"Visitantes",         {0x10,0x27,0xF5,0xAA,0x01,0x02}, -71,  6, UI_WIFI_AUTH_OPEN},
    {"",                   {0x10,0x27,0xF5,0xAA,0x01,0x03}, -74,  6, UI_WIFI_AUTH_WPA2},
    {"Laboratorio_IoT_Rede_Nome_Longo", {0xDC,0xA6,0x32,0x00,0x11,0x22}, -82, 3, UI_WIFI_AUTH_WPA3},
    {"eduroam",            {0x00,0x1A,0x2B,0x3C,0x4D,0x5E}, -86,  9, UI_WIFI_AUTH_ENTERPRISE},
    {"NET_2G_CASA",        {0xC8,0x3A,0x35,0x12,0x34,0x56}, -93, 13, UI_WIFI_AUTH_WEP},
};

static int scan_round;

static void scan_done_cb(lv_timer_t *t)
{
    (void)t;
    /* A cada busca alterna entre: lista cheia, lista cheia, vazio, erro */
    switch (scan_round++ % 4) {
    case 2:  ui_wifi_show_results(NULL, 0); break;
    case 3:  ui_wifi_show_error("Falha ao iniciar o scan (ESP_ERR_WIFI_STATE)"); break;
    default: ui_wifi_show_results(mock_aps, sizeof mock_aps / sizeof mock_aps[0]); break;
    }
}

static void on_scan(void)
{
    lv_timer_t *t = lv_timer_create(scan_done_cb, 1500, NULL);
    lv_timer_set_repeat_count(t, 1);
}

/* Conexao simulada: redes abertas e a senha "12345678" conectam;
 * qualquer outra senha falha com "Senha incorreta". */
static bool conn_ok;

static void connect_done_cb(lv_timer_t *t)
{
    (void)t;
    ui_wifi_connect_result(conn_ok, conn_ok ? "IP 192.168.0.42" : "Senha incorreta");
}

static void on_connect(const ui_wifi_ap_t *ap, const char *password)
{
    printf("Conectar em \"%s\" com senha \"%s\"\n", ap->ssid, password);
    conn_ok = ap->auth == UI_WIFI_AUTH_OPEN || strcmp(password, "12345678") == 0;
    lv_timer_t *t = lv_timer_create(connect_done_cb, 2000, NULL);
    lv_timer_set_repeat_count(t, 1);
}

void lvgl_live_preview_init(void)
{
    ui_wifi_create();
    ui_wifi_set_scan_cb(on_scan);
    ui_wifi_set_connect_cb(on_connect);

    /* faz a primeira busca automaticamente, como no firmware */
    ui_wifi_scan_started();
    on_scan();
}

#endif /* LVGL_LIVE_PREVIEW */
