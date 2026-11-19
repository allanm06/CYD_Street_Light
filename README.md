# CYD Street Light

Firmware para a **ESP32-2432S028 ("Cheap Yellow Display")** com interface em LVGL 9.
Etapa atual: tela de busca de redes Wi-Fi.

## Estrutura

| Caminho | Conteúdo |
|---|---|
| `src/main.cpp` | Inicialização do display (TFT_eSPI), touch (XPT2046), LVGL e scan Wi-Fi |
| `src/ui/ui_wifi.c/.h` | Tela LVGL, sem dependência de hardware |
| `src/ui/ui_wifi_preview.c` | Ponto de entrada do LVGL Live Preview com redes simuladas |

## Compilar e gravar

```sh
pio run -t upload --upload-port /dev/ttyUSB0
pio device monitor
```

## Preview no VS Code

Com a extensão **LVGL Live Preview**, abra `src/ui/ui_wifi_preview.c` e pressione `Ctrl+Shift+L`.
Ajuste o tamanho do display para **320 × 240** nas configurações do preview.

## Hardware

| Função | Pinos |
|---|---|
| LCD ILI9341 (HSPI) | SCLK 14, MOSI 13, MISO 12, CS 15, DC 2, BL 21 |
| Touch XPT2046 (VSPI) | SCLK 25, MOSI 32, MISO 39, CS 33, IRQ 36 |
