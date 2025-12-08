#pragma once

#include "include/common/types.hpp"

using namespace kos::common;

// Feature flag to indicate raw ICMP ABI presence
#define KOS_NET_HAVE_RAW_ICMP 1

namespace kos {
    namespace net {

        /*
        @brief Opaque handle for raw ICMP operations.
        */
        struct RawIcmpHandle {
            int opaque; // placeholder
        };

        /*
        @brief Raw ICMP socket API for sending/receiving ICMP Echo Requests/Replies.    
        @returns true if the MAC address was successfully resolved; false otherwise.
        */
        RawIcmpHandle rawicmp_open();


        /*
        @brief Closes a raw ICMP handle.
        @param h The raw ICMP handle to close.
        */  
        void rawicmp_close(RawIcmpHandle h);

        /*
        @brief Sends an ICMP Echo Request to dst.
        @param h The raw ICMP handle.
        @param dst_ip_be Destination IPv4 address in big-endian format.
        @param id Identifier for the ICMP Echo Request.     
        @param seq Sequence number for the ICMP Echo Request.
        @param payload Pointer to the payload data.
        @param payload_len Length of the payload data in bytes.     
        @param timeout_ms Timeout in milliseconds for sending (not implemented in stub).
        @returns true if send was queued.
        */
        bool rawicmp_send_echo(
            RawIcmpHandle h,
            uint32_t dst_ip_be,
            uint16_t id,
            uint16_t seq,
            const uint8_t* payload,
            uint32_t payload_len,
            uint32_t timeout_ms);

        /*  
        @brief Attempts to receive an ICMP Echo Reply matching id/seq.
        @param h The raw ICMP handle.
        @param id Identifier for the ICMP Echo Reply.     
        @param seq Sequence number for the ICMP Echo Reply.        
        @param buf Pointer to the buffer to receive the payload.
        @param buf_len Length of the buffer in bytes.
        @param timeout_ms Timeout in milliseconds to wait for a reply.
        @returns Number of bytes received into buf, or 0 on timeout/error.
        */
        uint32_t rawicmp_recv_echo(
            RawIcmpHandle h,
            uint16_t id,
            uint16_t seq,
            uint8_t* buf,
            uint32_t buf_len,
            uint32_t timeout_ms);

    }   
}
