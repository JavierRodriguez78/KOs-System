#ifndef __KOS__HARDWARE__PCI_H
#define __KOS__HARDWARE__PCI_H

#include <hardware/interrupts.hpp>
#include <drivers/driver.hpp>
#include <common/types.hpp>
#include <hardware/port.hpp>

namespace kos
{
    namespace hardware
    {
        class PeripheralComponentInterConnectDeviceDescriptor
        {
            public:
                kos::common::uint32_t portBase;
                kos::common::uint32_t interrupt;

                kos::common::uint16_t bus;
                kos::common::uint16_t device;
                kos::common::uint16_t function;

                kos::common::uint16_t vendor_id;
                kos::common::uint16_t device_id;
                
                kos::common::uint8_t class_id;
                kos::common::uint8_t subclass_id;
                kos::common::uint8_t interface_id;
                
                kos::common::uint8_t revision;

                PeripheralComponentInterConnectDeviceDescriptor();
                ~PeripheralComponentInterConnectDeviceDescriptor();
        };

        class PeripheralComponentIntercontroller
        {
            kos::hardware::Port32Bit dataPort;
            kos::hardware::Port32Bit commandPort;

            public:
                PeripheralComponentIntercontroller();
                ~PeripheralComponentIntercontroller();
                kos::common::uint32_t Read(kos::common::uint16_t bus, kos::common::uint16_t device, kos::common::uint16_t function, kos::common::uint32_t registeroffset);
                void Write(kos::common::uint16_t bus, kos::common::uint16_t device, kos::common::uint16_t function, kos::common::uint32_t registeroffset, kos::common::uint32_t value);
                bool DeviceHasFunctions(kos::common::uint16_t bus, kos::common::uint16_t device);

                void SelectDrivers(kos::drivers::DriverManager* driveManager);
                kos::hardware::PeripheralComponentInterConnectDeviceDescriptor GetDeviceDescriptor(kos::common::uint16_t bus, kos::common::uint16_t device, kos::common::uint16_t function);
        };
    }
}
#endif
