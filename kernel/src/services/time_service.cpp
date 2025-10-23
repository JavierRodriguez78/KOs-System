#include <services/time_service.hpp>
#include <console/logger.hpp>
#include <arch/x86/hardware/rtc/rtc.hpp>

using namespace kos::console;
using namespace kos::arch::x86::hardware::rtc;

namespace kos { 
    namespace services {

void TimeService::Tick() {
    if (!Logger::IsDebugEnabled()) return;
    DateTime dt; RTC::Read(dt);
    Logger::Log("[time service] tick");
}

}} // namespace services