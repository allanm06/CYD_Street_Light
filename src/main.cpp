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

static void wifi_conectar(void)
{
    Serial.printf("Wi-Fi: conectando em \"%s\"", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
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
