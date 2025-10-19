#pragma once

#ifndef __KOS__ARCH__X86__HARDWARE__PCI__PERIPHERALCOMPONENTINTERCONNECTDEVICEDESCRIPTOR_H
#define __KOS__ARCH__X86__HARDWARE__PCI__PERIPHERALCOMPONENTINTERCONNECTDEVICEDESCRIPTOR_H

#include <common/types.hpp>

using namespace kos::common;

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
                     * @brief Represents a PCI device descriptor
                     *
                     * This class holds the configuration information for a PCI device,
                     * including its bus, device, and function numbers, as well as vendor
                     * and device IDs.
                     */
                    class PeripheralComponentInterConnectDeviceDescriptor
                    {
        
                        public:
                           // Configuration space fields
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

                            /*
                            * @brief Constructor for PeripheralComponentInterConnectDeviceDescriptor
                            * Initializes all member variables to default values.
                            */   
                            PeripheralComponentInterConnectDeviceDescriptor();
                            
                            /*
                            * @brief Destructor for PeripheralComponentInterConnectDeviceDescriptor
                            * Cleans up any resources if necessary.
                            */
                            ~PeripheralComponentInterConnectDeviceDescriptor();
                    };
                }
            }
        }
    }   
}
#endif