#pragma once

#include <drivers/driver.hpp>
#include <common/types.hpp>

namespace kos {
    namespace drivers {
        namespace net {
            namespace rtl8822be {

                class Rtl8822beDriver : public Driver {
                public:
                    Rtl8822beDriver();
                    virtual void Activate() override;
                    virtual int Reset() override { return 0; }
                    virtual void Deactivate() override {}

                private:
                    bool probe_once();
                    void log_device();

                    kos::common::uint16_t vendor_{0};
                    kos::common::uint16_t device_{0};
                    bool found_{false};
                };

            }
        }
    }
}
