/*
 * Medidor do poste (Smart Street Light) - ESP32 generica.
 *
 * Mede tensao e corrente e publica 1x por segundo no topico DDS
 * SL_DDS_TOPIC (lib/StreetLightDDS). A CYD (branch DDS_CYD_Implementation)
 * assina o mesmo topico.
 *
 * Wi-Fi: copie src/credenciais.h.example para src/credenciais.h e preencha.
 *
 * Compilar/gravar:  pio run -t upload --upload-port /dev/ttyUSB0
 */
#include <Arduino.h>
#include <WiFi.h>
#include <math.h>

#include "sl_dds.h"

#if __has_include("credenciais.h")
#include "credenciais.h"
#else
#error "Crie src/credenciais.h a partir de src/credenciais.h.example"
#endif

#define POSTE_ID        1
#define PERIODO_MS      1000

/* 1 = gera valores falsos (sem sensor ligado). Troque por 0 ao implementar
 * ler_medicao() com o sensor real (ex.: PZEM-004T, ZMPT101B + SCT-013). */
#define MEDICAO_SIMULADA 1

static bool ler_medicao(float *tensao_v, float *corrente_a)
{
#if MEDICAO_SIMULADA
    static uint32_t n;
    float k = sinf(n++ * 0.3f);
    *tensao_v   = 221.0f + 4.0f * k;
    *corrente_a = 0.45f + 0.03f * k;
    return true;
#else
    /* TODO: leitura do sensor real */
    return false;
#endif
}

static volatile uint8_t s_disc_reason;

/* Lista as redes visiveis e diz se WIFI_SSID esta entre elas. */
static void wifi_diagnostico(void)
{
    int n = WiFi.scanNetworks();
    bool achou = false;
    Serial.printf("Wi-Fi: %d redes visiveis:\n", n);
    for (int i = 0; i < n; i++) {
        Serial.printf("  %-32s %4d dBm  canal %2d\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
        if (WiFi.SSID(i) == WIFI_SSID) achou = true;
    }
    WiFi.scanDelete();
    if (!achou)
        Serial.printf("Wi-Fi: \"%s\" NAO encontrada (a ESP32 so enxerga 2,4 GHz)\n", WIFI_SSID);
}

static void wifi_conectar(void)
{
    WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info) {
        s_disc_reason = info.wifi_sta_disconnected.reason;
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    for (;;) {
        Serial.printf("Wi-Fi: conectando em \"%s\"", WIFI_SSID);
        s_disc_reason = 0;
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        uint32_t t0 = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
            delay(500);
            Serial.print('.');
        }
        if (WiFi.status() == WL_CONNECTED) break;

        uint8_t r = s_disc_reason;
        const char *txt = r == WIFI_REASON_NO_AP_FOUND ? "rede nao encontrada" :
                          (r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
                           r == WIFI_REASON_HANDSHAKE_TIMEOUT || r == WIFI_REASON_AUTH_EXPIRE)
                              ? "senha incorreta?" : "";
        Serial.printf("\nWi-Fi: falhou (motivo %u %s)\n", r, txt);
        WiFi.disconnect();
        wifi_diagnostico();
        delay(2000);
    }
    Serial.printf("\nWi-Fi: IP %s\n", WiFi.localIP().toString().c_str());
}

void setup()
{
    Serial.begin(115200);
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);   /* economia de energia derruba multicast (descoberta DDS) */
    wifi_conectar();

    if (!sl_dds_start_publisher((uint32_t)WiFi.localIP()))
        Serial.println("DDS: falha ao iniciar");
}

void loop()
{
    static uint32_t seq, last;
    if (millis() - last < PERIODO_MS) { delay(10); return; }
    last = millis();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi: caiu, aguardando reconexao");
        return;     /* o driver reconecta sozinho */
    }

    sl_medicao_t m = {};
    m.poste_id = POSTE_ID;
    m.seq = ++seq;
    if (!ler_medicao(&m.tensao_v, &m.corrente_a)) return;

    bool ok = sl_dds_publish(&m);
    Serial.printf("#%lu  %.1f V  %.3f A  %s\n", (unsigned long)m.seq,
                  m.tensao_v, m.corrente_a, ok ? "" : "(falha ao publicar)");
}
