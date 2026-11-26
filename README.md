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

A **corrente** e lida de um transformador de corrente ZMCT103C (1000:1) no GPIO34
(`src/sensor_corrente.*`: ligacao, resistor de carga e calibracao no comentario).
A **tensao** ainda e simulada (`TENSAO_SIMULADA` em `src/main.cpp`).

## Estrutura

| Caminho | Conteudo |
|---|---|
| `src/main.cpp` | Rede Wi-Fi (AP), leitura e publicador DDS |
| `src/sensor_corrente.*` | Corrente RMS do TC ZMCT103C (ADC1) |
| `src/credenciais.h.example` | Modelo do nome/senha da rede criada |
| `lib/StreetLightDDS` | Topico DDS (igual na branch da CYD) |
| `lib/embeddedRTPS`, `lib/MicroCDR` | Copias das bibliotecas (ver "embeddedRTPS na ESP32") |

## Compilar e gravar

```sh
cp src/credenciais.h.example src/credenciais.h   # nome/senha da rede do poste
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
