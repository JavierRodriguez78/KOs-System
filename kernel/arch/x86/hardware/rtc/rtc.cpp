#include <arch/x86/hardware/rtc/rtc.hpp>
#include <arch/x86/hardware/rtc/rtc_constants.hpp>
#include <arch/x86/hardware/port/port8bit.hpp>

using namespace kos::common;
using namespace kos::arch::x86::hardware::rtc;
using namespace kos::arch::x86::hardware::port;

// CMOS ports
static Port8Bit cmosAddress(CMOS_ADDRESS_PORT);
static Port8Bit cmosData(CMOS_DATA_PORT);

// Read a byte from the specified CMOS register
static inline uint8_t cmos_read(uint8_t reg) {
    // Disable NMI (set bit 7) when selecting register
    cmosAddress.Write(reg | NMI_DISABLE_MASK);
    return cmosData.Read();
}

// Check if RTC is currently updating
static inline bool rtc_is_updating() {
    cmosAddress.Write(CMOS_REG_STATUS_A | NMI_DISABLE_MASK);
    return (cmosData.Read() & RTC_UPDATE_IN_PROGRESS_BIT) != 0; // Update-In-Progress bit
}

// Convert BCD to binary
static inline uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd / 16) * 10);
}

// Read current date/time from CMOS RTC into out parameter
void RTC::Read(DateTime& out) {
    // Wait until not updating to get a consistent snapshot
    while (rtc_is_updating()) { }

    uint8_t sec = cmos_read(CMOS_REG_SECONDS);
    uint8_t min = cmos_read(CMOS_REG_MINUTES);
    uint8_t hour = cmos_read(CMOS_REG_HOURS);
    uint8_t day = cmos_read(CMOS_REG_DAY);
    uint8_t mon = cmos_read(CMOS_REG_MONTH);
    uint8_t yr  = cmos_read(CMOS_REG_YEAR);
    uint8_t regB = cmos_read(CMOS_REG_STATUS_B);

    bool isBinary = (regB & RTC_FORMAT_BINARY_BIT) != 0;
    bool is24h    = (regB & RTC_FORMAT_24H_BIT) != 0;

    if (!isBinary) {
        sec = bcd_to_bin(sec);
        min = bcd_to_bin(min);
        hour = bcd_to_bin(hour);
        day = bcd_to_bin(day);
        mon = bcd_to_bin(mon);
        yr = bcd_to_bin(yr);
    }

    // Convert 12-hour to 24-hour if needed
    if (!is24h) {
        bool pm = (hour & RTC_PM_BIT) != 0;
        hour &= RTC_HOUR_MASK;
        if (pm && hour < 12) hour += 12;
        if (!pm && hour == 12) hour = 0;
    }

    // Basic century handling (assumes RTC_BASE_CENTURY..RTC_BASE_CENTURY+99)
    uint16_t year = RTC_BASE_CENTURY + yr;

    out.year = year;
    out.month = mon;
    out.day = day;
    out.hour = hour;
    out.minute = min;
    out.second = sec;
}
