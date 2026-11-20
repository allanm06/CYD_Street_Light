/*
 * Medidor do poste (Smart Street Light) - ESP32 generica.
 *
 * Cria a propria rede Wi-Fi (Access Point) e publica 1x por segundo a tensao
 * e a corrente no topico DDS SL_DDS_TOPIC (lib/StreetLightDDS). A CYD
 * (branch DDS_CYD_Implementation) conecta nessa rede e assina o topico.
 * Nao precisa de roteador: a medidora e sempre AP_IP (192.168.4.1) e
 * entrega IP por DHCP para quem conectar.
 *
 * Rede: copie src/credenciais.h.example para src/credenciais.h e preencha
 *       (WIFI_SSID / WIFI_PASS = nome e senha da rede CRIADA pela medidora).
 *
 * Compilar/gravar:  pio run -t upload --upload-port /dev/ttyUSB0
 */
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <math.h>

#include "sl_dds.h"

#if __has_include("credenciais.h")
#include "credenciais.h"
#else
#error "Crie src/credenciais.h a partir de src/credenciais.h.example"
#endif

static_assert(sizeof(WIFI_PASS) - 1 >= 8 || sizeof(WIFI_PASS) == 1,
              "WIFI_PASS precisa ter 8+ caracteres (WPA2) ou ser vazia (rede aberta)");

#define POSTE_ID        1
#define PERIODO_MS      1000

#define AP_CANAL        6
#define AP_MAX_CLIENTES 4
static const IPAddress AP_IP(192, 168, 4, 1);
static const IPAddress AP_MASK(255, 255, 255, 0);

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

static void wifi_eventos(WiFiEvent_t ev, WiFiEventInfo_t info)
{
    switch (ev) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
        const uint8_t *m = info.wifi_ap_staconnected.mac;
        Serial.printf("Wi-Fi: cliente conectou %02x:%02x:%02x:%02x:%02x:%02x (%d na rede)\n",
                      m[0], m[1], m[2], m[3], m[4], m[5], WiFi.softAPgetStationNum());
        break;
    }
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        Serial.printf("Wi-Fi: cliente saiu (%d na rede)\n", WiFi.softAPgetStationNum());
        break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED: {
        IPAddress ip(info.wifi_ap_staipassigned.ip.addr);
        Serial.printf("Wi-Fi: cliente recebeu IP %s\n", ip.toString().c_str());
        break;
    }
    default:
        break;
    }
}

static bool wifi_criar_rede(void)
{
    WiFi.persistent(false);
    WiFi.onEvent(wifi_eventos);
    /* So AP (sem STA): a interface do AP vira a padrao do lwIP, que e a
     * usada pelo embeddedRTPS para multicast e para checar a sub-rede. */
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_IP, AP_MASK);
    const char *senha = sizeof(WIFI_PASS) > 1 ? WIFI_PASS : NULL;
    if (!WiFi.softAP(WIFI_SSID, senha, AP_CANAL, 0 /*visivel*/, AP_MAX_CLIENTES)) {
        Serial.println("Wi-Fi: falha ao criar a rede");
        return false;
    }
    esp_wifi_set_ps(WIFI_PS_NONE);  /* sem economia de energia: multicast da descoberta DDS */
    Serial.printf("Wi-Fi: rede \"%s\" criada (%s), canal %d, IP %s\n", WIFI_SSID,
                  senha ? "WPA2" : "aberta", AP_CANAL, WiFi.softAPIP().toString().c_str());
    return true;
}

void setup()
{
    Serial.begin(115200);
    delay(200);

    if (!wifi_criar_rede()) {
        delay(3000);
        ESP.restart();
    }

    if (!sl_dds_start_publisher((uint32_t)WiFi.softAPIP()))
        Serial.println("DDS: falha ao iniciar");
}

void loop()
{
    static uint32_t seq, last;
    if (millis() - last < PERIODO_MS) { delay(10); return; }
    last = millis();

    sl_medicao_t m = {};
    m.poste_id = POSTE_ID;
    m.seq = ++seq;
    if (!ler_medicao(&m.tensao_v, &m.corrente_a)) return;

    /* publica mesmo sem clientes: quem conectar recebe a partir dai */
    bool ok = sl_dds_publish(&m);
    Serial.printf("#%lu  %.1f V  %.3f A  [%d na rede]%s\n", (unsigned long)m.seq,
                  m.tensao_v, m.corrente_a, WiFi.softAPgetStationNum(),
                  ok ? "" : "  (falha ao publicar)");
}
