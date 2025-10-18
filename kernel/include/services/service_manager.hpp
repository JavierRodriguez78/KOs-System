#ifndef KOS_SERVICES_SERVICE_MANAGER_HPP
#define KOS_SERVICES_SERVICE_MANAGER_HPP

#include <common/types.hpp>
#include <services/service.hpp>
#include <process/thread_manager.hpp>
#include <console/logger.hpp>

using namespace kos::common;

namespace kos {
    namespace services {

        struct ServiceNode {
            IService* svc;
            bool enabled;
            uint32_t last_tick_ms;
            ServiceNode* next;
        };

        // Minimal, static Service Manager
        class ServiceManager {
        public:
            // Register a service instance. Typically called from a static factory.
            static void Register(IService* service);

            // Load configuration and start enabled services.
            // Config format (simple lines):
            //   service.NAME=on|off
            //   debug=on|off
            // Lines starting with '#' are comments.
            static void InitAndStart();

            // Run service ticks periodically. Should be called from a system thread.
            static void TickAll();

            // Returns true if service exists and is enabled.
            static bool IsEnabled(const char* name);

            // Expose config flag for debug for convenience.
            static bool DebugFromConfig();

        private:
            static ServiceNode* s_head;
            static bool s_debugCfg;
            static uint32_t s_boot_ms;

            static void ApplyConfig();
            static uint32_t UptimeMs();
        };

        // API to run the ServiceManager loop as a system service thread.
        namespace ServiceAPI {
            bool StartManagerThread();
        }

    } // namespace services
} // namespace kos

#endif // KOS_SERVICES_SERVICE_MANAGER_HPP
