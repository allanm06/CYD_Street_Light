/*
 * rtps/config.h - configuracao do embeddedRTPS para ESP32 (Arduino-ESP32 3.x).
 *
 * Arquivo do projeto (nao faz parte do embeddedRTPS original), baseado em
 * config_stm.h. Diferencas:
 *  - IP_ADDRESS nao e constante: o IP vem do DHCP e deve ser preenchido com
 *    rtps_esp32_set_ip() ANTES de criar o rtps::Domain.
 *  - GUID aleatorio (GUID_RANDOM), para que cada placa tenha um GUID unico.
 *    O rand() deve ser semeado (srand(esp_random())) antes do Domain.
 *  - Pilhas em bytes (FreeRTOS da ESP32) e prioridades abaixo da task do lwIP.
 */
#ifndef RTPS_CONFIG_H
#define RTPS_CONFIG_H

#include "rtps/common/types.h"

namespace rtps {

#define IS_LITTLE_ENDIAN 1
#define OS_IS_FREERTOS

namespace Config {
const VendorId_t VENDOR_ID = {13, 37};

/* Preenchido em tempo de execucao (ver rtps_esp32_set_ip). */
extern std::array<uint8_t, 4> IP_ADDRESS;

const GuidPrefix_t BASE_GUID_PREFIX = GUID_RANDOM;

const uint8_t DOMAIN_ID = 0;
const uint8_t MAX_NUM_PARTICIPANTS = 1;
const uint8_t NUM_STATELESS_WRITERS = 2;   /* SPDP + reserva */
const uint8_t NUM_STATELESS_READERS = 2;
const uint8_t NUM_STATEFUL_READERS = 4;    /* SEDP (2) + usuario */
const uint8_t NUM_STATEFUL_WRITERS = 4;
const uint8_t NUM_WRITERS_PER_PARTICIPANT = 4;
const uint8_t NUM_READERS_PER_PARTICIPANT = 4;
const uint8_t NUM_WRITER_PROXIES_PER_READER = 4;
const uint8_t NUM_READER_PROXIES_PER_WRITER = 4;

const uint8_t MAX_NUM_UNMATCHED_REMOTE_WRITERS = 20;
const uint8_t MAX_NUM_UNMATCHED_REMOTE_READERS = 20;

const uint8_t MAX_NUM_READER_CALLBACKS = 2;

const uint8_t HISTORY_SIZE_STATELESS = 2;
const uint8_t HISTORY_SIZE_STATEFUL = 4;

const uint8_t MAX_TYPENAME_LENGTH = 32;
const uint8_t MAX_TOPICNAME_LENGTH = 32;

/* FreeRTOS da ESP32: tamanho de pilha em BYTES. */
const int HEARTBEAT_STACKSIZE = 3072;
const int THREAD_POOL_WRITER_STACKSIZE = 4096;
const int THREAD_POOL_READER_STACKSIZE = 6144;
const uint16_t SPDP_WRITER_STACKSIZE = 4096;

const uint16_t SF_WRITER_HB_PERIOD_MS = 4000;
const uint16_t SPDP_RESEND_PERIOD_MS = 1000;
const uint8_t SPDP_CYCLECOUNT_HEARTBEAT = 2;
const uint8_t SPDP_WRITER_PRIO = 5;        /* tcpip_thread do IDF roda em 18 */
const uint8_t SPDP_MAX_NUMBER_FOUND_PARTICIPANTS = 8;
const uint8_t SPDP_MAX_NUM_LOCATORS = 1;
const Duration_t SPDP_DEFAULT_REMOTE_LEASE_DURATION = {5, 0};
const Duration_t SPDP_MAX_REMOTE_LEASE_DURATION = {180, 0};
const Duration_t SPDP_LEASE_DURATION = {5, 0};

const int MAX_NUM_UDP_CONNECTIONS = 10;

const int THREAD_POOL_NUM_WRITERS = 1;
const int THREAD_POOL_NUM_READERS = 1;
const int THREAD_POOL_WRITER_PRIO = 5;
const int THREAD_POOL_READER_PRIO = 5;
const int THREAD_POOL_WORKLOAD_QUEUE_LENGTH_USERTRAFFIC = 20;
const int THREAD_POOL_WORKLOAD_QUEUE_LENGTH_METATRAFFIC = 20;

constexpr int OVERALL_HEAP_SIZE =
    THREAD_POOL_NUM_WRITERS * THREAD_POOL_WRITER_STACKSIZE +
    THREAD_POOL_NUM_READERS * THREAD_POOL_READER_STACKSIZE +
    MAX_NUM_PARTICIPANTS * SPDP_WRITER_STACKSIZE +
    NUM_STATEFUL_WRITERS * HEARTBEAT_STACKSIZE;
} // namespace Config
} // namespace rtps

/* Define o IP local usado nos locators anunciados na descoberta. */
void rtps_esp32_set_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d);

#endif // RTPS_CONFIG_H
