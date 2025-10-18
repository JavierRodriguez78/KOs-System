#include <services/time_service.hpp>
#include <console/logger.hpp>
#include <hardware/rtc.hpp>

using namespace kos::console;

namespace kos { 
    namespace services {

void TimeService::Tick() {
    if (!Logger::IsDebugEnabled()) return;
    kos::hardware::DateTime dt; kos::hardware::RTC::Read(dt);
    Logger::Log("[time service] tick");
}

}} // namespace services