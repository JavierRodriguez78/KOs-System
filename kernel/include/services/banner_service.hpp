#ifndef KOS_SERVICES_BANNER_SERVICE_HPP
#define KOS_SERVICES_BANNER_SERVICE_HPP

#include <services/service.hpp>

namespace kos {
    namespace services {

        class BannerService : public IService {
        public:
            virtual const char* Name() const override { return "BANNER"; }
            virtual bool Start() override;
            virtual uint32_t TickIntervalMs() const override { return 0; } // no periodic tick
        };

    } // namespace services
} // namespace kos

#endif
