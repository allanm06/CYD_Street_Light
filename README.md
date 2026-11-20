# CYD Street Light

Monitoramento de postes. Esta branch contem o firmware da **CYD**
(ESP32-2432S028, "Cheap Yellow Display"): interface LVGL 9 que mostra tensao e
corrente do poste e permite escolher a rede Wi-Fi. A **ESP medidora** do poste,
que mede e publica via DDS, fica na branch `DDS_ESP32_Implementation`.

A comunicacao e DDS/RTPS direto entre as placas (sem broker/agent), usando
[embeddedRTPS](https://github.com/embedded-software-laboratory/embeddedRTPS).
As placas se descobrem por multicast, entao precisam estar na **mesma rede**.
Topico `poste_medicao`, tipo `streetlight::Medicao` (IDL em `lib/StreetLightDDS/sl_dds.h`).

## Estrutura

| Caminho | Conteúdo |
|---|---|
| `src/main.cpp` | CYD: display (TFT_eSPI), touch (XPT2046), LVGL, Wi-Fi e assinante DDS |
| `src/ui/ui_pole.c/.h` | Tela do poste (tensao e corrente) |
| `src/ui/ui_wifi.c/.h` | Tela de redes Wi-Fi |
| `src/ui/ui_preview.c` | Ponto de entrada do LVGL Live Preview com dados simulados |
| `lib/StreetLightDDS` | Topico DDS (igual na branch da medidora) |
| `lib/embeddedRTPS`, `lib/MicroCDR` | Copias das bibliotecas (ver "embeddedRTPS na ESP32") |

## Compilar e gravar

```sh
pio run -t upload --upload-port /dev/ttyUSB0
pio device monitor
```

## embeddedRTPS na ESP32

Copia do commit `2932c64`, com as adaptacoes (marcadas com `[ESP32]` ou em arquivos proprios):

- `include/rtps/config.h` e `src/config_esp32.cpp`: IP definido em tempo de execucao (DHCP),
  GUID aleatorio, pilhas em bytes e prioridades abaixo da task do lwIP.
- `include/esp32_compat/`: redireciona `FreeRTOS.h`/`semphr.h` para `freertos/...` do ESP-IDF.
- `src/communication/UdpDriver.cpp`: compatibilidade com o lwIP dual-stack (IPv4+IPv6) do ESP-IDF.
- `src/rtps.cpp` removido (inicializacao de lwIP para Linux/Windows).

Limitacao: o IP e fixado quando o DDS inicia; se a CYD mudar de IP, reinicie-a.

## Preview no VS Code

Com a extensão **LVGL Live Preview**, abra `src/ui/ui_wifi_preview.c` e pressione `Ctrl+Shift+L`.
Ajuste o tamanho do display para **320 × 240** nas configurações do preview.

## Hardware

| Função | Pinos |
|---|---|
| LCD ILI9341 (HSPI) | SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, BL 21 |
| Touch XPT2046 (VSPI) | SCLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 |
