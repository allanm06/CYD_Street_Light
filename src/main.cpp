/*
 * CYD Street Light - tela de busca de redes Wi-Fi.
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
/* Wi-Fi scan: roda numa task propria (bloqueante); o loop() so consome     */
/* o resultado, entao todas as chamadas LVGL ficam na mesma thread.         */
/* ======================================================================== */
static TaskHandle_t s_scan_task;
static ui_wifi_ap_t s_aps[WIFI_MAX_APS];
static volatile int16_t s_scan_result = 0;   /* >=0 redes, <0 erro */
static volatile bool s_scan_ready = false;

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

static void scan_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

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
}

/* chamado pela UI quando o botao de busca e tocado */
static void on_scan_requested(void) { xTaskNotifyGive(s_scan_task); }

static void on_connect(const ui_wifi_ap_t *ap)
{
    Serial.printf("Conectar em \"%s\" (ainda nao implementado)\n", ap->ssid);
}

static void wifi_poll(void)
{
    if (!s_scan_ready) return;
    s_scan_ready = false;
    if (s_scan_result < 0) ui_wifi_show_error("Falha na busca");
    else                   ui_wifi_show_results(s_aps, s_scan_result);
}

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

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    xTaskCreatePinnedToCore(scan_task, "wifi_scan", 4096, NULL, 1, &s_scan_task, 0);

    ui_wifi_create();
    ui_wifi_set_scan_cb(on_scan_requested);
    ui_wifi_set_connect_cb(on_connect);

    /* primeira busca automatica */
    ui_wifi_scan_started();
    xTaskNotifyGive(s_scan_task);
}

void loop()
{
    lv_timer_handler();
    wifi_poll();
    delay(5);
}
