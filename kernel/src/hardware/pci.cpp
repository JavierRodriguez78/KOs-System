#include <hardware/pci.hpp>
#include <console/logger.hpp>

using namespace kos::common;
using namespace kos::hardware;
using namespace kos::drivers;
using namespace kos::console;



PeripheralComponentInterConnectDeviceDescriptor::PeripheralComponentInterConnectDeviceDescriptor()
{

};
PeripheralComponentInterConnectDeviceDescriptor::~PeripheralComponentInterConnectDeviceDescriptor()
{

};

PeripheralComponentIntercontroller::PeripheralComponentIntercontroller()
: dataPort(0xCFC),
  commandPort(0xCF8)
{

};

PeripheralComponentIntercontroller::~PeripheralComponentIntercontroller()
{

};


uint32_t PeripheralComponentIntercontroller::Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset)
{
    uint32_t id=
        0x1 << 31
        | ((bus & 0xFF) << 16)
        | ((device & 0x1F) << 11)
        | ((function & 0x07) << 8)
        | (registeroffset & 0xFC);
    commandPort.Write(id);
    uint32_t result = dataPort.Read();
    return result >> (8* (registeroffset % 4));


};
                
void PeripheralComponentIntercontroller::Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset, uint32_t value)
{
    uint32_t id=
        0x1 << 31
        | ((bus & 0xFF) << 16)
        | ((device & 0x1F) << 11)
        | ((function & 0x07) << 8)
        | (registeroffset & 0xFC);
    commandPort.Write(id);
    dataPort.Write(value);
};

bool PeripheralComponentIntercontroller::DeviceHasFunctions(uint16_t bus, uint16_t device)
{
    return Read(bus, device, 0, 0x0E) & (1<<7);
};

void printf(int8_t* str);
void printfHex(uint8_t);

void PeripheralComponentIntercontroller::SelectDrivers(DriverManager* driveManager)
{
    Logger::Log("Scanning PCI bus...");
    for(int32_t bus= 0 ; bus < 8; bus++)
    {
        for(int32_t device=0; device < 32; device ++)
        {
            int32_t numFunctions = DeviceHasFunctions(bus, device) ? 8 :1;

            for(int32_t function=0; function< numFunctions; function++)
            {
                PeripheralComponentInterConnectDeviceDescriptor dev = GetDeviceDescriptor(bus, device, function);
                if(dev.vendor_id==0x0000 ||dev.vendor_id==0xFFFF )
                    break;

                Logger::SetDebugEnabled(false);
                if (Logger::IsDebugEnabled()) {
                    tty.Write("PCI BUS ");
                    tty.WriteHex(bus & 0xFF);
    
                    tty.Write(", DEVICE ");
                    tty.WriteHex(device & 0xFF);
    
                    tty.Write(", FUNCTION ");
                    tty.WriteHex(function & 0xFF);
    
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

    result.vendor_id = Read(bus, device, function, 0x00);
    result.device_id = Read(bus, device, function, 0x02);

    result.class_id = Read(bus, device, function, 0x0b);
    result.subclass_id = Read(bus, device, function, 0x0a);
    result.interface_id = Read(bus, device, function, 0x09);

    result.revision = Read(bus, device, function, 0x08);
    result.interrupt = Read(bus, device, function, 0x3c);
    return result;
};