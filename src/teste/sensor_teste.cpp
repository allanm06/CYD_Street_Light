/*
 * Teste do sensor de corrente: mostra a tensao no pino em tempo real.
 * Firmware separado (sem Wi-Fi/DDS), ambiente "teste_sensor":
 *
 *   pio run -e teste_sensor -t upload --upload-port /dev/ttyUSB0
 *   pio device monitor
 *
 * Comandos no monitor serial (digite a letra e Enter):
 *   r = modo bruto: uma amostra (mV) por linha, ~1 kHz (bom para Serial Plotter)
 *   s = modo resumo (padrao): 5x por segundo, instantaneo/media/min/max/RMS AC
 */
#include <Arduino.h>
#include <math.h>

#include "../sensor_corrente.h"   /* CORRENTE_PINO */

static bool modo_bruto = false;

void setup()
{
    Serial.begin(115200);
    delay(200);
    analogReadResolution(12);
    analogSetPinAttenuation(CORRENTE_PINO, ADC_11db);
    Serial.printf("\nTeste do sensor no GPIO%d. 'r' = bruto, 's' = resumo\n", CORRENTE_PINO);
}

static void comandos(void)
{
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 'r') modo_bruto = true;
        if (c == 's') { modo_bruto = false; Serial.println("media(mV)  min   max   RMS_AC(mV)  agora"); }
    }
}

void loop()
{
    comandos();

    if (modo_bruto) {
        Serial.println(analogReadMilliVolts(CORRENTE_PINO));
        delayMicroseconds(1000);
        return;
    }

    /* 200 ms = 12 ciclos de 60 Hz, amostrando a ~5 kHz */
    int64_t soma = 0, soma_q = 0;
    uint32_t n = 0, mn = UINT32_MAX, mx = 0, agora = 0;
    uint32_t t0 = micros(), prox = t0;
    while ((uint32_t)(micros() - t0) < 200000) {
        while ((int32_t)(micros() - prox) < 0) { }
        prox += 200;
        agora = analogReadMilliVolts(CORRENTE_PINO);
        soma += agora;
        soma_q += (int64_t)agora * agora;
        if (agora < mn) mn = agora;
        if (agora > mx) mx = agora;
        n++;
    }
    double media = (double)soma / n;
    double rms = sqrt(fmax(0.0, (double)soma_q / n - media * media));
    Serial.printf("%7.0f  %5lu %5lu  %9.1f  %5lu\n", media, (unsigned long)mn,
                  (unsigned long)mx, rms, (unsigned long)agora);
}
