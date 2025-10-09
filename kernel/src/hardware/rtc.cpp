#include <hardware/rtc.hpp>
#include <hardware/port.hpp>

using namespace kos::common;
using namespace kos::hardware;

// CMOS ports
static Port8Bit cmosAddress(0x70);
static Port8Bit cmosData(0x71);

static inline uint8_t cmos_read(uint8_t reg) {
    // Disable NMI (set bit 7) when selecting register
    cmosAddress.Write(reg | 0x80);
    return cmosData.Read();
}

static inline bool rtc_is_updating() {
    cmosAddress.Write(0x0A | 0x80);
    return (cmosData.Read() & 0x80) != 0; // Update-In-Progress bit
}

static inline uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd / 16) * 10);
}

void RTC::Read(DateTime& out) {
    // Wait until not updating to get a consistent snapshot
    while (rtc_is_updating()) { }

    uint8_t sec = cmos_read(0x00);
    uint8_t min = cmos_read(0x02);
    uint8_t hour = cmos_read(0x04);
    uint8_t day = cmos_read(0x07);
    uint8_t mon = cmos_read(0x08);
    uint8_t yr  = cmos_read(0x09);
    uint8_t regB = cmos_read(0x0B);

    bool isBinary = (regB & 0x04) != 0;
    bool is24h    = (regB & 0x02) != 0;

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
        bool pm = (hour & 0x80) != 0;
        hour &= 0x7F;
        if (pm && hour < 12) hour += 12;
        if (!pm && hour == 12) hour = 0;
    }

    // Basic century handling (assume 2000-2099)
    uint16_t year = 2000 + yr;

    out.year = year;
    out.month = mon;
    out.day = day;
    out.hour = hour;
    out.minute = min;
    out.second = sec;
}
