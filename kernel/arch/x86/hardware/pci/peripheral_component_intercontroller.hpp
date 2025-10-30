#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__PCI__PERIPHERALCOMPONENTINTERCONTROLLER_H
#define __KOS__ARCH__X86__HARDWARE__PCI__PERIPHERALCOMPONENTINTERCONTROLLER_H


#include <arch/x86/hardware/port/port32bit.hpp>
#include <drivers/driver_manager.hpp>
#include <common/types.hpp>
#include <console/tty.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.hpp>

using namespace kos::common;
using namespace kos::drivers;
using namespace kos::arch::x86::hardware::port;
using namespace kos::console;

namespace kos
{
   namespace arch
   {

       namespace x86
       {
           namespace hardware
           {
               namespace pci
               {

                    /**
                     * @brief Controller for Peripheral Component Interconnect (PCI) devices
                     *
                     * This class provides methods to read from and write to PCI configuration
                     * space, as well as to detect and manage PCI devices on the system.
                     */
                    class PeripheralComponentIntercontroller
                    {
                        // Ports for PCI configuration space access
                        Port32Bit dataPort;
                        Port32Bit commandPort;

                        public:
                            /*
                            * @brief Constructor for PeripheralComponentIntercontroller
                            * Initializes the PCI controller with the appropriate I/O ports.
                            */
                            PeripheralComponentIntercontroller();
                            
                            /*
                            * @brief Destructor for PeripheralComponentIntercontroller
                            * Cleans up any resources if necessary.
                            */
                            ~PeripheralComponentIntercontroller();
                            
                            /*
                            * @brief Reads a 32-bit value from PCI configuration space
                            * @return The 32-bit value read from the specified PCI configuration register.
                            * @param bus The PCI bus number.
                            * @param device The PCI device number.
                            * @param function The PCI function number.
                            * @param registeroffset The offset of the register to read.
                            */
                            uint32_t Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset);
                          
                            /*
                            * @brief Writes a 32-bit value to PCI configuration space
                            * @return void
                            * @param bus The PCI bus number.
                            * @param device The PCI device number.
                            * @param function The PCI function number.
                            * @param registeroffset The offset of the register to write.
                            * @param value The 32-bit value to write to the specified PCI configuration register.
                            */
                            void Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset, uint32_t value);
                            
                            /*
                            * @brief Checks if a PCI device has multiple functions
                            * @return true if the device has multiple functions, false otherwise.
                            * @param bus The PCI bus number.
                            * @param device The PCI device number.
                            */ 
                            bool DeviceHasFunctions(uint16_t bus, uint16_t device);

                            /*
                            * @brief Selects the appropriate drivers for the detected PCI devices
                            * @return void
                            * @param driveManager Pointer to the DriverManager instance.
                            */
                            void SelectDrivers(DriverManager* driveManager);
                           
                            /*
                            * @brief Retrieves the device descriptor for a specific PCI device
                            * @return The device descriptor for the specified PCI device.
                            * @param bus The PCI bus number.
                            * @param device The PCI device number.
                            * @param function The PCI function number.
                            */
                            PeripheralComponentInterConnectDeviceDescriptor GetDeviceDescriptor(uint16_t bus, uint16_t device, uint16_t function);
                        private:

                            // TTY instance for logging
                            static TTY tty;
                    };
                }
            }
        }   
    }
}
#endif
