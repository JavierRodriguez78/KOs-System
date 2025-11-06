#include <net/ipv4.hpp>
#include <lib/string.hpp>

using namespace kos::lib;
namespace kos { 
    namespace net { 
        namespace ipv4 {

            static Config g_cfg;

            void SetConfig(const Config& cfg) {
                String::strncpy(reinterpret_cast<int8_t*>(g_cfg.ip),   reinterpret_cast<const int8_t*>(cfg.ip),   sizeof(g_cfg.ip)-1);
                String::strncpy(reinterpret_cast<int8_t*>(g_cfg.mask), reinterpret_cast<const int8_t*>(cfg.mask), sizeof(g_cfg.mask)-1);
                String::strncpy(reinterpret_cast<int8_t*>(g_cfg.gw),   reinterpret_cast<const int8_t*>(cfg.gw),   sizeof(g_cfg.gw)-1);
                String::strncpy(reinterpret_cast<int8_t*>(g_cfg.dns),  reinterpret_cast<const int8_t*>(cfg.dns),  sizeof(g_cfg.dns)-1);
                g_cfg.ip[15]=g_cfg.mask[15]=g_cfg.gw[15]=g_cfg.dns[15]='\0';
            }

            const Config& GetConfig() { return g_cfg; }

        }
    }
}
