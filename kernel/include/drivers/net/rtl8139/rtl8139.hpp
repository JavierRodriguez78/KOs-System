#pragma once

#include <drivers/driver.hpp>
#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace drivers { 
        namespace net {
            namespace rtl8139 {

                /*
                *@brief RTL8139 network driver class.
                *This class implements the RTL8139 network driver for Realtek
                *Ethernet controllers.
                */  
                class Rtl8139Driver : public Driver {
                    public:
                        /*
                        * Constructor for the RTL8139 driver.
                        */
                        Rtl8139Driver();
                        
                        /*
                        * @brief Activate the RTL8139 network driver.
                        * This method initializes the driver and prepares it for operation.
                        */
                        virtual void Activate() override;
    
                        /*
                        * @brief Reset the RTL8139 network driver.
                        * @return int   0 on success, negative error code on failure.
                        * This method resets the driver and prepares it for a new operation.
                        */
                        virtual int Reset() override { return 0; }

                        /*
                        * @brief Deactivate the RTL8139 network driver.
                        * This method stops the driver and releases any resources it holds.
                        */
                        virtual void Deactivate() override {}

                    private:

                        /*
                         * @brief Probe the RTL8139 network device.
                         * This method checks if the device is present and initializes it.
                         * @return true if the device is found and initialized, false otherwise.
                         */
                        bool probe_once();

                        /*
                         * @brief Log the RTL8139 network device information.
                         * This method logs the device's vendor and device IDs.
                         */
                        void log_device();

                        // Minimal stored info
                        /*
                         * @brief Vendor ID of the RTL8139 network device.
                         */
                        uint16_t vendor_{0};

                        /*
                         * @brief Device ID of the RTL8139 network device.
                         */
                        uint16_t device_{0};
                        /*
                         * @brief I/O base address of the RTL8139 network device.
                         */
                        uint32_t io_base_{0};

                        /*
                         * @brief IRQ number of the RTL8139 network device.
                         */
                        uint8_t  irq_{0};

                        /*
                         * @brief Flag indicating if the RTL8139 network device was found.
                         */
                        bool found_{false};
                };

            }
        }
    }
}

