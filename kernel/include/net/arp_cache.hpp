#pragma once

#include "include/common/types.hpp"
#include "include/net/arp.hpp"

namespace kos { 
    namespace net {

        /*
        @brief Looks up the MAC address for a given IPv4 address in the ARP cache.
        @param ip The IPv4 address to look up.
        @param mac_out Output parameter to receive the MAC address if found.
        @return True if the MAC address was found in the cache; false otherwise.    
        */
        bool arp_cache_lookup(const IPv4Addr& ip, MacAddr& mac_out);
        
        /*
        @brief Updates or adds an entry in the ARP cache.
        @param ip The IPv4 address to associate with the MAC address.
        @param mac The MAC address to store in the cache.       
        */
        void arp_cache_update(const IPv4Addr& ip, const MacAddr& mac);
        
        
        /*
        @brief Sends an ARP request for the specified IPv4 address.
        @param ip The IPv4 address to request the MAC address for.
        */
        void arp_send_request(const IPv4Addr& ip);


    } 
}
