#ifndef KOS_SERVICES_SERVICE_HPP
#define KOS_SERVICES_SERVICE_HPP

#include <common/types.hpp>
#include <process/thread.h>

using namespace kos::common;
using namespace kos::process;

namespace kos {
    namespace services {

        // Basic service interface for kernel/system services started at boot.
        class IService {
        public:
            virtual ~IService() {}

            // Unique short name (8.3 friendly) used in config, e.g. "BANNER" or "TIME"
            virtual const char* Name() const = 0;

            // Called once when service is enabled at startup. Return true on success.
            virtual bool Start() = 0;

            // Optional periodic work. Override Tick() if the service needs background work.
            virtual void Tick() {}

            // Called on shutdown (not currently invoked during poweroff).
            virtual void Stop() {}

            // Default enabled state if not configured.
            virtual bool DefaultEnabled() const { return true; }

            // Desired tick interval in milliseconds for periodic work (if Tick is overridden).
            virtual uint32_t TickIntervalMs() const { return 1000; }
        };

    } // namespace services
} // namespace kos

#endif // KOS_SERVICES_SERVICE_HPP
