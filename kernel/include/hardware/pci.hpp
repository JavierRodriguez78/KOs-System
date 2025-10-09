#ifndef __KOS__HARDWARE__PCI_H
#define __KOS__HARDWARE__PCI_H

#include <hardware/interrupts.hpp>
#include <hardware/port.hpp>
#include <drivers/driver.hpp>
#include <common/types.hpp>
#include <console/tty.hpp>

using namespace kos::common;
using namespace kos::drivers;
using namespace kos::hardware;
using namespace kos::console;

namespace kos
{
    namespace hardware
    {
        class PeripheralComponentInterConnectDeviceDescriptor
        {
            public:
                uint32_t portBase;
                uint32_t interrupt;

                uint16_t bus;
                uint16_t device;
                uint16_t function;

                uint16_t vendor_id;
                uint16_t device_id;
                
                uint8_t class_id;
                uint8_t subclass_id;
                uint8_t interface_id;
                
                uint8_t revision;

                PeripheralComponentInterConnectDeviceDescriptor();
                ~PeripheralComponentInterConnectDeviceDescriptor();
        };

        class PeripheralComponentIntercontroller
        {
            Port32Bit dataPort;
            Port32Bit commandPort;

            public:
                PeripheralComponentIntercontroller();
                ~PeripheralComponentIntercontroller();
                uint32_t Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset);
                void Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset, uint32_t value);
                bool DeviceHasFunctions(uint16_t bus, uint16_t device);

                void SelectDrivers(DriverManager* driveManager);
                PeripheralComponentInterConnectDeviceDescriptor GetDeviceDescriptor(uint16_t bus, uint16_t device, uint16_t function);
            private:
                static TTY tty;
        };
    }
}
#endif
