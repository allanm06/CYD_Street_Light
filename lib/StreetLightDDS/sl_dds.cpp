/*
 * sl_dds.cpp - ver sl_dds.h.
 */
#include "sl_dds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_random.h>
#include <esp_system.h>

#include "rtps/rtps.h"
#include "ucdr/microcdr.h"

/* Mesma QoS nos dois lados, senao os endpoints nao casam na descoberta.
 * Best-effort: leitura periodica, a proxima amostra substitui a perdida. */
static const bool SL_DDS_RELIABLE = false;

/* cabecalho de encapsulamento + 4 campos de 4 bytes */
#define SL_PAYLOAD_SIZE (4 + 4 * 4)
static const uint8_t CDR_LE_HEADER[4] = {0x00, 0x01, 0x00, 0x00};

static rtps::Domain *s_domain;
static rtps::Writer *s_writer;
static sl_dds_rx_cb_t s_rx_cb;

/* ---- serializacao ------------------------------------------------------- */
static uint32_t serialize(const sl_medicao_t *m, uint8_t *buf, size_t len)
{
    memcpy(buf, CDR_LE_HEADER, 4);
    ucdrBuffer ub;
    ucdr_init_buffer_offset_endian(&ub, buf + 4, len - 4, 0, UCDR_LITTLE_ENDIANNESS);
    ucdr_serialize_uint32_t(&ub, m->poste_id);
    ucdr_serialize_uint32_t(&ub, m->seq);
    ucdr_serialize_float(&ub, m->tensao_v);
    ucdr_serialize_float(&ub, m->corrente_a);
    return ub.error ? 0 : 4 + (uint32_t)ucdr_buffer_length(&ub);
}

static bool deserialize(const uint8_t *buf, size_t len, sl_medicao_t *m)
{
    if (len < SL_PAYLOAD_SIZE || buf[0] != 0x00 || (buf[1] != 0x00 && buf[1] != 0x01))
        return false;
    ucdrEndianness e = buf[1] == 0x01 ? UCDR_LITTLE_ENDIANNESS : UCDR_BIG_ENDIANNESS;
    ucdrBuffer ub;
    ucdr_init_buffer_offset_endian(&ub, buf + 4, len - 4, 0, e);
    ucdr_deserialize_uint32_t(&ub, &m->poste_id);
    ucdr_deserialize_uint32_t(&ub, &m->seq);
    ucdr_deserialize_float(&ub, &m->tensao_v);
    ucdr_deserialize_float(&ub, &m->corrente_a);
    return !ub.error;
}

/* ---- dominio ------------------------------------------------------------ */
static rtps::Participant *domain_start(uint32_t ip)
{
    if (s_domain) return nullptr;

    /* IP vem do DHCP: precisa estar definido antes de criar os endpoints,
     * pois vai nos locators anunciados. GUID aleatorio exige rand() semeado. */
    rtps_esp32_set_ip(ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24);
    srand(esp_random());

    size_t heap0 = esp_get_free_heap_size();
    s_domain = new rtps::Domain();
    printf("[dds] Domain: %u bytes, heap livre %u -> %u\n", (unsigned)sizeof(rtps::Domain),
           (unsigned)heap0, (unsigned)esp_get_free_heap_size());
    rtps::Participant *part = s_domain->createParticipant();
    if (!part) printf("[dds] falha ao criar participante\n");
    return part;
}

static bool domain_finish(void)
{
    bool ok = s_domain->completeInit();
    printf("[dds] dominio %s (heap livre %u, minimo %u)\n", ok ? "iniciado" : "FALHOU ao iniciar threads",
           (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
    return ok;
}

static void reader_cb(void *arg, const rtps::ReaderCacheChange &change)
{
    (void)arg;
    if (change.kind != rtps::ChangeKind_t::ALIVE) return;
    uint8_t buf[64];
    if (change.size > sizeof buf || !change.copyInto(buf, sizeof buf)) return;
    sl_medicao_t m;
    if (deserialize(buf, change.size, &m) && s_rx_cb) s_rx_cb(&m);
}

/* ---- API ---------------------------------------------------------------- */
extern "C" bool sl_dds_start_publisher(uint32_t ip)
{
    rtps::Participant *part = domain_start(ip);
    if (!part) return false;
    s_writer = s_domain->createWriter(*part, SL_DDS_TOPIC, SL_DDS_TYPE, SL_DDS_RELIABLE);
    if (!s_writer) {
        printf("[dds] falha ao criar writer\n");
        return false;
    }
    return domain_finish();
}

extern "C" bool sl_dds_start_subscriber(uint32_t ip, sl_dds_rx_cb_t cb)
{
    rtps::Participant *part = domain_start(ip);
    if (!part) return false;
    s_rx_cb = cb;
    rtps::Reader *reader = s_domain->createReader(*part, SL_DDS_TOPIC, SL_DDS_TYPE, SL_DDS_RELIABLE);
    if (!reader) {
        printf("[dds] falha ao criar reader\n");
        return false;
    }
    reader->registerCallback(reader_cb, nullptr);
    return domain_finish();
}

extern "C" bool sl_dds_publish(const sl_medicao_t *m)
{
    if (!s_writer) return false;
    uint8_t buf[SL_PAYLOAD_SIZE];
    uint32_t n = serialize(m, buf, sizeof buf);
    if (!n) return false;
    return s_writer->newChange(rtps::ChangeKind_t::ALIVE, buf, n) != nullptr;
}

extern "C" bool sl_dds_started(void) { return s_domain != nullptr; }
