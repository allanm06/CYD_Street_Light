# CYD Street Light - ESP32 medidora

Firmware da **ESP32 do poste**: mede tensao e corrente e publica 1x por segundo
via DDS. O display que mostra os dados (CYD, ESP32-2432S028) fica na branch
`DDS_CYD_Implementation`.

A comunicacao e DDS/RTPS direto entre as placas (sem broker/agent), usando
[embeddedRTPS](https://github.com/embedded-software-laboratory/embeddedRTPS).
As placas se descobrem por multicast, entao precisam estar na **mesma rede**.
Topico `poste_medicao`, tipo `streetlight::Medicao` (IDL em `lib/StreetLightDDS/sl_dds.h`).

Por enquanto a leitura e **simulada** (`MEDICAO_SIMULADA` em `src/main.cpp`);
o sensor real entra em `ler_medicao()`.

## Estrutura

| Caminho | Conteudo |
|---|---|
| `src/main.cpp` | Wi-Fi, leitura (simulada) e publicador DDS |
| `src/credenciais.h.example` | Modelo das credenciais Wi-Fi |
| `lib/StreetLightDDS` | Topico DDS (igual na branch da CYD) |
| `lib/embeddedRTPS`, `lib/MicroCDR` | Copias das bibliotecas (ver "embeddedRTPS na ESP32") |

## Compilar e gravar

```sh
cp src/credenciais.h.example src/credenciais.h   # e preencha SSID/senha
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
