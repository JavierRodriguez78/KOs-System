#ifndef KOS_SERVICES_FILESYSTEM_SERVICE_HPP
#define KOS_SERVICES_FILESYSTEM_SERVICE_HPP

#include <services/service.hpp>

namespace kos {
    namespace services {

        // Filesystem service: mounts the root filesystem (FAT32 preferred)
        // and ensures standard directories exist at boot (like /BIN, /HOME, /ETC).
        class FilesystemService : public IService {
        public:
            virtual const char* Name() const override { return "FS"; }
            virtual bool Start() override;
            virtual uint32_t TickIntervalMs() const override { return 0; } // no periodic tick
            virtual bool DefaultEnabled() const override { return true; }
        };

    } // namespace services
} // namespace kos

#endif // KOS_SERVICES_FILESYSTEM_SERVICE_HPP
