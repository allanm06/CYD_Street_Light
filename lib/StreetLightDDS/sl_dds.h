/*
 * sl_dds.h - Comunicacao DDS/RTPS do Smart Street Light (embeddedRTPS).
 *
 * Usado pelos dois firmwares:
 *   - medidor (ESP no poste): publica sl_medicao_t no topico SL_DDS_TOPIC;
 *   - CYD (display): assina o topico e mostra os valores.
 *
 * Nao ha broker nem agent: as placas se descobrem por multicast (SPDP/SEDP,
 * 239.255.0.1) e trocam dados por UDP unicast, como qualquer DDS. Por isso
 * as duas precisam estar na mesma rede/sub-rede, com multicast liberado.
 *
 * O payload segue o padrao DDS (cabecalho CDR_LE + struct em CDR), entao
 * o topico tambem pode ser lido por Fast DDS / ROS 2 com o IDL abaixo:
 *
 *   module streetlight {
 *     struct Medicao {
 *       unsigned long poste_id;
 *       unsigned long seq;
 *       float tensao_v;
 *       float corrente_a;
 *     };
 *   };
 *
 * Uso: chame sl_dds_start_*() UMA vez, depois que o Wi-Fi obteve IP.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SL_DDS_TOPIC "poste_medicao"
#define SL_DDS_TYPE  "streetlight::Medicao"

typedef struct {
    uint32_t poste_id;
    uint32_t seq;           /* contador de amostras do medidor */
    float tensao_v;         /* tensao RMS (V) */
    float corrente_a;       /* corrente RMS (A) */
} sl_medicao_t;

/* Chamado na task de recepcao do RTPS (NAO na thread do LVGL). */
typedef void (*sl_dds_rx_cb_t)(const sl_medicao_t *m);

/* ip: endereco IPv4 local (ordem de rede, como WiFi.localIP()).  */
bool sl_dds_start_publisher(uint32_t ip);
bool sl_dds_start_subscriber(uint32_t ip, sl_dds_rx_cb_t cb);

/* Publica uma amostra (somente apos sl_dds_start_publisher). */
bool sl_dds_publish(const sl_medicao_t *m);

bool sl_dds_started(void);

#ifdef __cplusplus
}
#endif
