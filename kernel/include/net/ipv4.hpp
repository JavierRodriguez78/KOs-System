#pragma once

#include <common/types.hpp>

namespace kos { 
    namespace net { 
        namespace ipv4 {

            /*
            *@brief IPv4 configuration structure
            */      
            struct Config {
                char ip[16];
                char mask[16];
                char gw[16];
                char dns[16];
            };

            /*
            * @brief Set the IPv4 configuration
            * @param cfg The configuration to set
            */
            void SetConfig(const Config& cfg);
            
            /*
            * @brief Get the current IPv4 configuration
            * @return The current configuration
            */  
            const Config& GetConfig();

        } // namespace ipv4
    } // namespace net
} // namespace kos
