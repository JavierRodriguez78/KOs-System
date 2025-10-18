#ifndef KOS_SERVICES_TIME_SERVICE_HPP
#define KOS_SERVICES_TIME_SERVICE_HPP

#include <services/service.hpp>

namespace kos {
    namespace services {

        class TimeService : public IService {
        public:
            virtual const char* Name() const override { return "TIME"; }
            virtual bool Start() override { return true; }
            virtual void Tick() override;
            virtual uint32_t TickIntervalMs() const override { return 1000; }
            virtual bool DefaultEnabled() const override { return false; } // off by default
        };

    } // namespace services
} // namespace kos

#endif

