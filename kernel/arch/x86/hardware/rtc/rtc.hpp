#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__RTC__RTC_H
#define __KOS__ARCH__X86__HARDWARE__RTC__RTC_H

#include <common/types.hpp>

namespace kos { 
    namespace arch { 
        namespace x86 { 
            namespace hardware { 
                namespace rtc {

                    using namespace kos::common;

                    /**
                     * @brief DateTime structure representing RTC date and time.
                     */
                    struct DateTime {
                        uint16_t year;   // full year e.g., 2025
                        uint8_t  month;  // 1-12
                        uint8_t  day;    // 1-31
                        uint8_t  hour;   // 0-23
                        uint8_t  minute; // 0-59
                        uint8_t  second; // 0-59
                    };

                    /**
                     * @brief Real-Time Clock (RTC) interface for reading date and time.
                     */
                    class RTC {
                    public:
                        // Read current date/time from CMOS RTC into out parameter
                        static void Read(DateTime& out);
                    };

                } // namespace rtc
            } // namespace hardware
        } // namespace x86
    } // namespace arch
} // namespace kos

#endif
