/* Definicoes do rtps/config.h especificas da ESP32 (arquivo do projeto). */
#include "rtps/config.h"

namespace rtps {
namespace Config {
std::array<uint8_t, 4> IP_ADDRESS = {0, 0, 0, 0};
} // namespace Config
} // namespace rtps

void rtps_esp32_set_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    rtps::Config::IP_ADDRESS = {a, b, c, d};
}
