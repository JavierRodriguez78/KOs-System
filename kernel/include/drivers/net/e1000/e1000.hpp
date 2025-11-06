#pragma once

#include <drivers/driver.hpp>
#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace drivers { 
        namespace net {
            namespace e1000 {

                /*
                *@brief E1000 network driver class.
                *This class implements the E1000 network driver for Intel 8254x/8255x
                *Ethernet controllers.
                */
                class E1000Driver : public Driver {
                    public:

                        /**
                         * @brief Construct a new E1000Driver object
                         */
                        E1000Driver();
                        
                        /*
                        * @brief Activate the E1000 network driver.
                        * This method initializes the driver and prepares it for operation.
                        */
                        virtual void Activate() override;
                        
                        /*
                        * @brief Reset the E1000 network driver.
                        * @return int   0 on success, negative error code on failure.
                        * This method resets the driver and prepares it for a new operation.
                        */
                        virtual int Reset() override { return 0; }

                        /*
                        * @brief Deactivate the E1000 network driver.
                        * This method deactivates the driver and releases any resources it holds.
                        */
                        virtual void Deactivate() override {}

                    private:
                        /*
                        * @brief Probe the PCI bus for the E1000 device.
                        * @return true if the device is found, false otherwise.
                        * This method scans the PCI bus to locate the E1000 network device.
                        */
                        bool probe_once();
                        
                        /*
                        * @brief Log the E1000 device information.
                        * This method logs relevant information about the detected E1000 device.
                        */  
                        void log_device();

                        /*
                        * @brief E1000 device vendor ID.
                        * @var vendor_id
                        */
                        uint16_t vendor_{0};
                        
                        /*
                        * @brief E1000 device ID.
                        * @var device_id
                        */
                        uint16_t device_{0};
                        
                        /*
                        * @brief E1000 device I/O base address.
                        * @var io_base
                        */
                        uint32_t io_base_{0};
                        
                        /*
                        * @brief E1000 device IRQ line.
                        * @var irq
                        */
                        uint8_t irq_{0};
                        
                        /*
                        * @brief E1000 device found status.
                        * @var found
                        */  
                        bool found_{false};
                    };
                }
            }
        }
    }
