#pragma once

#include <common/types.hpp>

using namespace kos::common;



namespace kos{
    namespace arch {
    
        namespace x86 {
        
            namespace hardware {
            
                namespace rtc {
                
                    // CMOS ports
                    static constexpr uint16_t CMOS_ADDRESS_PORT = 0x70;
                    static constexpr uint16_t CMOS_DATA_PORT    = 0x71;

                    // CMOS register indices
                    static constexpr uint8_t CMOS_REG_SECONDS  = 0x00;
                    static constexpr uint8_t CMOS_REG_MINUTES  = 0x02;
                    static constexpr uint8_t CMOS_REG_HOURS    = 0x04;
                    static constexpr uint8_t CMOS_REG_DAY      = 0x07;
                    static constexpr uint8_t CMOS_REG_MONTH    = 0x08;
                    static constexpr uint8_t CMOS_REG_YEAR     = 0x09;
                    static constexpr uint8_t CMOS_REG_STATUS_A = 0x0A;
                    static constexpr uint8_t CMOS_REG_STATUS_B = 0x0B;

                    // Bit masks
                    static constexpr uint8_t NMI_DISABLE_MASK           = 0x80;
                    static constexpr uint8_t RTC_UPDATE_IN_PROGRESS_BIT = 0x80;
                    static constexpr uint8_t RTC_FORMAT_BINARY_BIT      = 0x04;
                    static constexpr uint8_t RTC_FORMAT_24H_BIT         = 0x02;
                    static constexpr uint8_t RTC_PM_BIT                 = 0x80;
                    static constexpr uint8_t RTC_HOUR_MASK              = 0x7F;

                    // Base century assumption
                    static constexpr uint16_t RTC_BASE_CENTURY = 2000;
                
                } // namespace rtc

            } // namespace hardware
            
        } // namespace x86
        
    } // namespace arch
}