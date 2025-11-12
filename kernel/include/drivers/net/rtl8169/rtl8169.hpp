#pragma once

#include <drivers/driver.hpp>
#include <common/types.hpp>

using namespace kos::common;

namespace kos {
    namespace drivers {
        namespace net {
            namespace rtl8169 {

                /*
                 * @brief RTL8169-family network driver scaffold.
                 * Supports detecting Realtek 8169/8168/810x NICs and logging presence.
                 */
                class Rtl8169Driver : public Driver {
                public:
                    // Constructs the RTL8169 network driver
                    Rtl8169Driver();

                    // Activates the RTL8169 network driver (logs presence if found)
                    virtual void Activate() override;

                    // Resets the driver (stub)
                    virtual int  Reset() override { return 0; }

                    // Deactivates the driver (stub)
                    virtual void Deactivate() override {}

                private:
                    // Probe PCI bus for supported Realtek 8169-family devices
                    bool probe_once();

                    // Log device details once detected
                    void log_device();

                    // Minimal stored info
                    uint16_t vendor_{0};
                    uint16_t device_{0};
                    uint32_t io_base_{0};
                    uint8_t  irq_{0};
                    bool     found_{false};
                };

            } // namespace rtl8169
        } // namespace net
    } // namespace drivers
} // namespace kos