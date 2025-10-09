#ifndef __KOS__HARDWARE__RTC_H
#define __KOS__HARDWARE__RTC_H

#include <common/types.hpp>

namespace kos {
namespace hardware {

    struct DateTime {
        kos::common::uint16_t year;   // full year e.g., 2025
        kos::common::uint8_t  month;  // 1-12
        kos::common::uint8_t  day;    // 1-31
        kos::common::uint8_t  hour;   // 0-23
        kos::common::uint8_t  minute; // 0-59
        kos::common::uint8_t  second; // 0-59
    };

    class RTC {
    public:
        // Read current date/time from CMOS RTC into out parameter
        static void Read(DateTime& out);
    };

}
}

#endif
