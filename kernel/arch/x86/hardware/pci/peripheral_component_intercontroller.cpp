#include <console/logger.hpp>
#include "peripheral_component_intercontroller.hpp"
#include <arch/x86/hardware/pci/peripheral_component_inter_constants.hpp>


using namespace kos::common;
using namespace kos::hardware;
using namespace kos::drivers;
using namespace kos::console;
using namespace kos::arch::x86::hardware::pci;



PeripheralComponentIntercontroller::PeripheralComponentIntercontroller()
: dataPort(PCI_DATA_PORT),
  commandPort(PCI_COMMAND_PORT)
{

};

PeripheralComponentIntercontroller::~PeripheralComponentIntercontroller()
{

};


uint32_t PeripheralComponentIntercontroller::Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset)
{
    uint32_t id=
        PCI_ENABLE_BIT
        | ((bus & PCI_BUS_MASK) << PCI_BUS_SHIFT)
        | ((device & PCI_DEVICE_MASK) << PCI_DEVICE_SHIFT)
        | ((function & PCI_FUNCTION_MASK) << PCI_FUNCTION_SHIFT)
        | (registeroffset & PCI_REGISTER_MASK);
    commandPort.Write(id);
    uint32_t result = dataPort.Read();
    return result >> (8* (registeroffset % 4));


};
                
void PeripheralComponentIntercontroller::Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset, uint32_t value)
{
    uint32_t id=
        PCI_ENABLE_BIT
        | ((bus & PCI_BUS_MASK) << PCI_BUS_SHIFT)
        | ((device & PCI_DEVICE_MASK) << PCI_DEVICE_SHIFT)
        | ((function & PCI_FUNCTION_MASK) << PCI_FUNCTION_SHIFT)
        | (registeroffset & PCI_REGISTER_MASK);
    commandPort.Write(id);
    dataPort.Write(value);
};

bool PeripheralComponentIntercontroller::DeviceHasFunctions(uint16_t bus, uint16_t device)
{
    return Read(bus, device, 0, PCI_HEADER_TYPE_OFFSET) & (1 << PCI_MULTI_FUNCTION_BIT);
};

void printf(int8_t* str);
void printfHex(uint8_t);

void PeripheralComponentIntercontroller::SelectDrivers(DriverManager* driveManager)
{
    Logger::Log("Scanning PCI bus...");
    for(int32_t bus= 0 ; bus < PCI_MAX_BUSES; bus++)
    {
        for(int32_t device=0; device < PCI_MAX_DEVICES; device ++)
        {
            int32_t numFunctions = DeviceHasFunctions(bus, device) ? PCI_MAX_FUNCTIONS : 1;

            for(int32_t function=0; function< numFunctions; function++)
            {
                PeripheralComponentInterConnectDeviceDescriptor dev = GetDeviceDescriptor(bus, device, function);
                if(dev.vendor_id==PCI_INVALID_VENDOR_1 || dev.vendor_id==PCI_INVALID_VENDOR_2 )
                    break;

                Logger::SetDebugEnabled(false);
                if (Logger::IsDebugEnabled()) {
                    tty.Write("PCI BUS ");
                    tty.WriteHex(bus & PCI_BUS_MASK);
    
                    tty.Write(", DEVICE ");
                    tty.WriteHex(device & PCI_DEVICE_MASK);
    
                    tty.Write(", FUNCTION ");
                    tty.WriteHex(function & PCI_FUNCTION_MASK);
    
                    tty.Write(" = VENDOR ");
                    tty.WriteHex((dev.vendor_id & 0xFF00) >>8);
                    tty.WriteHex(dev.vendor_id & 0xFF);
    
                    tty.Write(", DEVICE ");
                    tty.WriteHex((dev.device_id & 0xFF00)>>8);
                    tty.WriteHex(dev.device_id & 0xFF);
                    tty.Write("\n");
                }
            }
        }
    }
};

PeripheralComponentInterConnectDeviceDescriptor PeripheralComponentIntercontroller::GetDeviceDescriptor(uint16_t bus, uint16_t device, uint16_t function)
{
    PeripheralComponentInterConnectDeviceDescriptor result;
    result.bus = bus;
    result.device = device;
    result.function = function;

    result.vendor_id = Read(bus, device, function, PCI_VENDOR_ID_OFFSET);
    result.device_id = Read(bus, device, function, PCI_DEVICE_ID_OFFSET);

    result.class_id = Read(bus, device, function, PCI_CLASS_OFFSET);
    result.subclass_id = Read(bus, device, function, PCI_SUBCLASS_OFFSET);
    result.interface_id = Read(bus, device, function, PCI_PROG_IF_OFFSET);

    result.revision = Read(bus, device, function, PCI_REVISION_ID_OFFSET);
    result.interrupt = Read(bus, device, function, PCI_INTERRUPT_LINE_OFFSET);
    return result;
};