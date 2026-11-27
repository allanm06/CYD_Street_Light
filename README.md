# CYD Street Light - ESP32 medidora

Firmware da **ESP32 do poste**: cria a propria rede Wi-Fi (Access Point), mede
tensao e corrente e publica 1x por segundo via DDS. O display que mostra os dados
(CYD, ESP32-2432S028) fica na branch `DDS_CYD_Implementation` e conecta nessa rede
pela tela de Wi-Fi. Nao precisa de roteador.

| Rede | Valor |
|---|---|
| Nome / senha | `WIFI_SSID` / `WIFI_PASS` em `src/credenciais.h` (senha com 8+ caracteres) |
| IP da medidora | `192.168.4.1` (a CYD recebe IP por DHCP, `192.168.4.x`) |
| Canal | 6, ate 4 clientes |

A comunicacao e DDS/RTPS direto entre as placas (sem broker/agent), usando
[embeddedRTPS](https://github.com/embedded-software-laboratory/embeddedRTPS).
As placas se descobrem por multicast dentro da rede da medidora.
Topico `poste_medicao`, tipo `streetlight::Medicao` (IDL em `lib/StreetLightDDS/sl_dds.h`).

A **corrente** e lida do modulo ZMCT103C de 3 pinos (HW-670: VCC, GND, OUT) no GPIO34
(`src/sensor_corrente.*`: ligacao e calibracao no comentario). Para ver o sinal do
sensor em tempo real no serial: `pio run -e teste_sensor -t upload`.

A **tensao** ainda nao tem sensor (fica para depois). Por enquanto e **calculada**
a partir da potencia nominal da carga, `V = CARGA_POTENCIA_W / I` (`src/main.cpp`),
e publicada no campo `tensao_v`. Nao e medicao: sem corrente publica 0 V, e como a
calibracao da corrente partiu de 25 W / 127 V, com o ferro de solda o valor fica
sempre perto de 127 V.

### Limites do sensor de corrente

| Limite | Detalhe |
|---|---|
| Faixa | Com o ganho atual do trimpot: ~3,3 V de pico por A, ou seja, ate **~0,9 A**. Acima disso o ADC satura e a leitura e descartada. Para mais corrente, baixe o ganho no trimpot e recalibre. |
| Correntes pequenas | Abaixo de ~150 mV o ADC da ESP32 nao distingue nada de 0 V: correntes abaixo de **~0,05 A** perdem precisao e abaixo de 0,02 A sao reportadas como 0. |
| Meio ciclo | O OUT do modulo oscila em volta de 0 V e a ESP32 so le o semiciclo positivo. O RMS e calculado assumindo onda simetrica (senoide); cargas com corrente distorcida (fontes chaveadas, LED com driver barato) podem dar erro. |
| Calibracao | `CORRENTE_FATOR_A_POR_V = 0.404`, feita com ferro de solda 25 W / 127 V nominal (~0,197 A), sem alicate amperimetro: erro de alguns %, conforme a tensao real da rede. Recalibre multiplicando o fator por `I_real / I_lido`. |
| Tensao no pino | Com o modulo em 5 V, corrente muito acima da faixa pode levar o OUT acima de 3,3 V e danificar o GPIO34. |
| Pino | Precisa ser ADC1 (GPIO32-39): o ADC2 nao funciona com o Wi-Fi ligado. |

## Estrutura

| Caminho | Conteudo |
|---|---|
| `src/main.cpp` | Rede Wi-Fi (AP), leitura e publicador DDS |
| `src/sensor_corrente.*` | Corrente RMS do modulo ZMCT103C (ADC1) |
| `src/teste/sensor_teste.cpp` | Firmware de teste: tensao do sensor no serial (`-e teste_sensor`) |
| `src/credenciais.h.example` | Modelo do nome/senha da rede criada |
| `lib/StreetLightDDS` | Topico DDS (igual na branch da CYD) |
| `lib/embeddedRTPS`, `lib/MicroCDR` | Copias das bibliotecas (ver "embeddedRTPS na ESP32") |

## Compilar e gravar

```sh
cp src/credenciais.h.example src/credenciais.h   # nome/senha da rede do poste
pio run -e medidor -t upload --upload-port /dev/ttyUSB0
pio device monitor
```

## embeddedRTPS na ESP32

Copia do commit `2932c64`, com as adaptacoes (marcadas com `[ESP32]` ou em arquivos proprios):

- `include/rtps/config.h` e `src/config_esp32.cpp`: IP definido em tempo de execucao (DHCP),
  GUID aleatorio, pilhas em bytes e prioridades abaixo da task do lwIP.
- `include/esp32_compat/`: redireciona `FreeRTOS.h`/`semphr.h` para `freertos/...` do ESP-IDF.
- `src/communication/UdpDriver.cpp`: compatibilidade com o lwIP dual-stack (IPv4+IPv6) do ESP-IDF.
- `src/rtps.cpp` removido (inicializacao de lwIP para Linux/Windows).
