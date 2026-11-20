/*
 * CYD Street Light - painel de um poste (Smart Street Light).
 *
 * A ESP medidora do poste (branch DDS_ESP32_Implementation) mede tensao e corrente e publica
 * via DDS/RTPS (lib/StreetLightDDS); esta CYD assina o topico e mostra os
 * dados. O icone de Wi-Fi no cabecalho abre a tela de redes; o DDS inicia
 * sozinho na primeira vez que o Wi-Fi obtem IP.
 *
 * Placa: ESP32-2432S028 ("Cheap Yellow Display")
 * Display/touch inicializados igual ao projeto CYD_Ecg (TFT_eSPI + XPT2046).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include "ui/ui_wifi.h"
#include "ui/ui_pole.h"
#include "sl_dds.h"

#define POLE_NAME "Poste 01"

/* 1 = gera leituras falsas (1 por segundo) para ver a tela sem o medidor.
 * Com 0 a tela mostra os dados recebidos via DDS. */
#define POLE_DEMO_DATA 0

/* ---- Touch (barramento VSPI proprio) ----------------------------------- */
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

#define SCREEN_W 320
#define SCREEN_H 240
#define WIFI_MAX_APS 20

static XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
static TFT_eSPI tft = TFT_eSPI();
static SPIClass touchSpi = SPIClass(VSPI);

/* ======================================================================== */
/* LVGL <-> hardware                                                        */
/* ======================================================================== */
static uint32_t my_tick(void) { return millis(); }

static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)px_map, w * h, true);
    tft.endWrite();
    lv_display_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    if (ts.touched()) {
        TS_Point p = ts.getPoint();
        data->point.x = constrain(map(p.x, 200, 3800, 0, SCREEN_W), 0, SCREEN_W - 1);
        data->point.y = constrain(map(p.y, 200, 3800, 0, SCREEN_H), 0, SCREEN_H - 1);
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ======================================================================== */
/* Wi-Fi: um worker (task propria) executa scan e conexao, que bloqueiam.   */
/* O loop() so consome os resultados, entao o LVGL fica numa thread so.     */
/* ======================================================================== */
#define CONNECT_TIMEOUT_MS 15000

typedef enum { CMD_SCAN, CMD_CONNECT } wifi_cmd_type_t;
typedef struct {
    wifi_cmd_type_t type;
    ui_wifi_ap_t ap;
    char pass[65];
} wifi_cmd_t;

static QueueHandle_t s_cmd_q;

/* resultados (escritos pelo worker, lidos pelo loop) */
static ui_wifi_ap_t s_aps[WIFI_MAX_APS];
static volatile int16_t s_scan_result;
static volatile bool s_scan_ready;
static volatile bool s_conn_ready;
static volatile bool s_conn_ok;
static char s_conn_msg[48];

static volatile uint8_t s_disc_reason;     /* ultimo motivo de desconexao */

static ui_wifi_auth_t map_auth(wifi_auth_mode_t m)
{
    switch (m) {
    case WIFI_AUTH_OPEN:            return UI_WIFI_AUTH_OPEN;
    case WIFI_AUTH_WEP:             return UI_WIFI_AUTH_WEP;
    case WIFI_AUTH_WPA_PSK:         return UI_WIFI_AUTH_WPA;
    case WIFI_AUTH_WPA2_PSK:        return UI_WIFI_AUTH_WPA2;
    case WIFI_AUTH_WPA_WPA2_PSK:    return UI_WIFI_AUTH_WPA_WPA2;
    case WIFI_AUTH_WPA3_PSK:        return UI_WIFI_AUTH_WPA3;
    case WIFI_AUTH_WPA2_WPA3_PSK:   return UI_WIFI_AUTH_WPA2_WPA3;
    case WIFI_AUTH_WPA2_ENTERPRISE: return UI_WIFI_AUTH_ENTERPRISE;
    default:                        return UI_WIFI_AUTH_OTHER;
    }
}

static bool reason_is_auth_fail(uint8_t r)
{
    return r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_AUTH_EXPIRE ||
           r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_HANDSHAKE_TIMEOUT ||
           r == WIFI_REASON_MIC_FAILURE;
}

static void do_scan(void)
{
    int16_t n = WiFi.scanNetworks(false /*bloqueante*/, true /*ocultas*/);
    if (n > 0) {
        if (n > WIFI_MAX_APS) n = WIFI_MAX_APS;
        for (int16_t i = 0; i < n; i++) {
            strlcpy(s_aps[i].ssid, WiFi.SSID(i).c_str(), sizeof s_aps[i].ssid);
            memcpy(s_aps[i].bssid, WiFi.BSSID(i), 6);
            s_aps[i].rssi = WiFi.RSSI(i);
            s_aps[i].channel = WiFi.channel(i);
            s_aps[i].auth = map_auth(WiFi.encryptionType(i));
        }
    }
    WiFi.scanDelete();
    Serial.printf("scan: %d\n", n);
    s_scan_result = n;
    s_scan_ready = true;
}

static void do_connect(const wifi_cmd_t *c)
{
    Serial.printf("conectando em \"%s\"...\n", c->ap.ssid);

    if (WiFi.isConnected()) {
        WiFi.disconnect();
        delay(200);
    }
    s_disc_reason = 0;
    WiFi.begin(c->ap.ssid, c->pass[0] ? c->pass : NULL, c->ap.channel, c->ap.bssid);

    bool ok = false;
    const char *err = "Tempo esgotado";
    uint32_t t0 = millis();
    while (millis() - t0 < CONNECT_TIMEOUT_MS) {
        if (WiFi.status() == WL_CONNECTED) { ok = true; break; }
        uint8_t r = s_disc_reason;
        if (reason_is_auth_fail(r))          { err = "Senha incorreta"; break; }
        if (r == WIFI_REASON_NO_AP_FOUND)    { err = "Rede nao encontrada"; break; }
        delay(100);
    }

    if (ok) {
        snprintf(s_conn_msg, sizeof s_conn_msg, "IP %s", WiFi.localIP().toString().c_str());
    } else {
        WiFi.disconnect();      /* para as tentativas automaticas */
        if (s_disc_reason && strcmp(err, "Tempo esgotado") == 0)
            snprintf(s_conn_msg, sizeof s_conn_msg, "Falha (motivo %u)", s_disc_reason);
        else
            strlcpy(s_conn_msg, err, sizeof s_conn_msg);
    }
    Serial.printf("conexao: %s (%s)\n", ok ? "ok" : "falhou", s_conn_msg);
    s_conn_ok = ok;
    s_conn_ready = true;
}

static void wifi_worker(void *arg)
{
    (void)arg;
    static wifi_cmd_t cmd;
    for (;;) {
        if (xQueueReceive(s_cmd_q, &cmd, portMAX_DELAY) != pdTRUE) continue;
        if (cmd.type == CMD_SCAN) do_scan();
        else                      do_connect(&cmd);
        memset(cmd.pass, 0, sizeof cmd.pass);
    }
}

/* ---- callbacks da UI (thread do loop) ---------------------------------- */
static void on_scan_requested(void)
{
    static wifi_cmd_t c;
    c.type = CMD_SCAN;
    xQueueSend(s_cmd_q, &c, 0);
}

static void on_connect(const ui_wifi_ap_t *ap, const char *password)
{
    static wifi_cmd_t c;
    c.type = CMD_CONNECT;
    c.ap = *ap;
    strlcpy(c.pass, password, sizeof c.pass);
    if (xQueueSend(s_cmd_q, &c, 0) != pdTRUE)
        ui_wifi_connect_result(false, "Ocupado, tente de novo");
    memset(c.pass, 0, sizeof c.pass);
}

/* ---- entrega dos resultados para a UI (thread do loop) ----------------- */
static void wifi_poll(void)
{
    if (s_scan_ready) {
        s_scan_ready = false;
        if (s_scan_result < 0) ui_wifi_show_error("Falha na busca");
        else                   ui_wifi_show_results(s_aps, s_scan_result);
    }
    if (s_conn_ready) {
        s_conn_ready = false;
        ui_wifi_connect_result(s_conn_ok, s_conn_msg);
    }

    /* acompanha quedas/reconexoes depois de conectado */
    static uint32_t last;
    if (millis() - last > 1000) {
        last = millis();
        bool conn = WiFi.isConnected();
        String ssid = conn ? WiFi.SSID() : String();
        ui_wifi_set_connected(conn ? ssid.c_str() : NULL);
        ui_pole_set_wifi(conn, conn ? ssid.c_str() : NULL);
    }
}

/* ======================================================================== */
/* Dados do poste (DDS)                                                     */
/* A callback roda na task de recepcao do RTPS; ela so guarda a amostra     */
/* mais recente e o loop() a entrega ao LVGL.                               */
/* ======================================================================== */
static portMUX_TYPE s_sample_mux = portMUX_INITIALIZER_UNLOCKED;
static sl_medicao_t s_sample;
static volatile bool s_sample_ready;
static uint32_t s_dds_ip;          /* IP com que o DDS foi iniciado */

static void on_medicao(const sl_medicao_t *m)
{
    portENTER_CRITICAL(&s_sample_mux);
    s_sample = *m;
    s_sample_ready = true;
    portEXIT_CRITICAL(&s_sample_mux);
}

static void dds_poll(void)
{
    /* inicia na primeira vez que ha IP */
    if (!sl_dds_started()) {
        if (!WiFi.isConnected()) return;
        s_dds_ip = (uint32_t)WiFi.localIP();
        if (s_dds_ip == 0) return;
        Serial.printf("DDS: iniciando assinante em %s\n", WiFi.localIP().toString().c_str());
        if (!sl_dds_start_subscriber(s_dds_ip, on_medicao))
            Serial.println("DDS: falha ao iniciar");
        return;
    }

    if (!s_sample_ready) return;
    sl_medicao_t m;
    portENTER_CRITICAL(&s_sample_mux);
    m = s_sample;
    s_sample_ready = false;
    portEXIT_CRITICAL(&s_sample_mux);

    ui_pole_data_t d = {};
    d.voltage_v = m.tensao_v;
    d.current_a = m.corrente_a;
    d.seq = m.seq;
    ui_pole_update(&d);
}

/* O IP anunciado na descoberta e fixado ao iniciar o DDS. Se mudar
 * (outra rede/novo lease), o medidor nao alcanca mais a CYD. */
static void dds_check_ip(void)
{
    static uint32_t last;
    if (!sl_dds_started() || millis() - last < 5000) return;
    last = millis();
    uint32_t ip = WiFi.isConnected() ? (uint32_t)WiFi.localIP() : 0;
    if (ip && ip != s_dds_ip)
        Serial.println("DDS: IP mudou desde o inicio do DDS; reinicie a CYD");
}

#if POLE_DEMO_DATA
static void pole_demo_poll(void)
{
    static uint32_t last, n;
    if (millis() - last < 1000) return;
    last = millis();
    n++;

    float k = sinf(n * 0.3f);
    ui_pole_data_t d = {};
    d.voltage_v = 221.0f + 4.0f * k;
    d.current_a = 0.45f + 0.03f * k;
    d.seq = n;
    ui_pole_update(&d);
}
#endif

static void open_wifi_screen(void)  { ui_wifi_open(); }
static void close_wifi_screen(void) { ui_pole_show(); }

/* ======================================================================== */
void setup()
{
    Serial.begin(115200);

    pinMode(21, OUTPUT);
    analogWrite(21, 255);

    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(touchSpi);
    ts.setRotation(1);

    lv_init();
    lv_tick_set_cb(my_tick);

    static uint16_t buf[SCREEN_W * 20];
    lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
    lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, my_disp_flush);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    WiFi.persistent(false);     /* nao grava credenciais na flash */
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);       /* economia de energia derruba multicast (descoberta DDS) */
    WiFi.disconnect();
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        s_disc_reason = info.wifi_sta_disconnected.reason;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    delay(100);
    s_cmd_q = xQueueCreate(2, sizeof(wifi_cmd_t));
    xTaskCreatePinnedToCore(wifi_worker, "wifi", 4096, NULL, 1, NULL, 0);

    ui_wifi_create();
    ui_wifi_set_scan_cb(on_scan_requested);
    ui_wifi_set_connect_cb(on_connect);
    ui_wifi_set_back_cb(close_wifi_screen);

    ui_pole_create(POLE_NAME);          /* tela inicial */
    ui_pole_set_wifi_cb(open_wifi_screen);
    ui_pole_set_wifi(false, NULL);

    /* busca em segundo plano: a lista ja esta pronta ao abrir a tela de Wi-Fi */
    ui_wifi_scan_started();
    on_scan_requested();
}

void loop()
{
    lv_timer_handler();
    wifi_poll();
#if POLE_DEMO_DATA
    pole_demo_poll();
#else
    dds_poll();
    dds_check_ip();
#endif
    delay(5);
}
