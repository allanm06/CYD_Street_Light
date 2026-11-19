/*
 * ui_preview.c - Ponto de entrada do LVGL Live Preview (VS Code).
 *
 * Simula as leituras do poste e o Wi-Fi (scan e conexao) com dados falsos.
 * No firmware o arquivo compila vazio (LVGL_LIVE_PREVIEW so existe no preview).
 * Ajuste o preview para 320x240.
 *   - Senha que "conecta": 12345678
 *   - A cada 40 s o poste "some" por 10 s para mostrar o estado "Sem dados"
 */
#ifdef LVGL_LIVE_PREVIEW

#include "ui_wifi.h"
#include "ui_pole.h"
#include <math.h>
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
    ui_pole_set_wifi(conn_ok, NULL);
}

static void on_connect(const ui_wifi_ap_t *ap, const char *password)
{
    printf("Conectar em \"%s\" com senha \"%s\"\n", ap->ssid, password);
    conn_ok = ap->auth == UI_WIFI_AUTH_OPEN || strcmp(password, "12345678") == 0;
    lv_timer_t *t = lv_timer_create(connect_done_cb, 2000, NULL);
    lv_timer_set_repeat_count(t, 1);
}

/* ---- Leituras simuladas do poste ---------------------------------------- */
static void pole_mock_cb(lv_timer_t *t)
{
    (void)t;
    static uint32_t n;
    static float energy = 12.30f;
    n++;
    if (n % 40 >= 30) return;           /* simula queda do link */

    float k = sinf(n * 0.3f);
    ui_pole_data_t d = {
        .voltage_v     = 221.0f + 4.0f * k,
        .lamp_on       = true,
        .dimming_pct   = 80,
        .power_factor  = 0.96f + 0.02f * k,
        .temperature_c = 41.0f + 2.0f * k,
        .ambient_lux   = 8.0f + 3.0f * k,
    };
    d.power_w   = 96.0f + 3.0f * k;
    d.current_a = d.power_w / (d.voltage_v * d.power_factor);
    energy     += d.power_w / 3600000.0f;
    d.energy_kwh = energy;
    ui_pole_update(&d);
}

static void open_wifi(void) { ui_wifi_open(); }
static void close_wifi(void) { ui_pole_show(); }

void lvgl_live_preview_init(void)
{
    ui_wifi_create();
    ui_wifi_set_scan_cb(on_scan);
    ui_wifi_set_connect_cb(on_connect);
    ui_wifi_set_back_cb(close_wifi);

    ui_pole_create("Poste 01");
    ui_pole_set_wifi_cb(open_wifi);
    ui_pole_set_wifi(false, NULL);

    lv_timer_create(pole_mock_cb, 1000, NULL);
}

#endif /* LVGL_LIVE_PREVIEW */
