#include "port16bit.hpp"


using namespace kos::arch::x86::hardware::port;


// Constructor
Port16Bit::Port16Bit(uint16_t portnumber): Port(portnumber)
{
}

// Destructor
Port16Bit::~Port16Bit()
{
}

//  Write a 16-bit value to the port
void Port16Bit::Write(uint16_t data)
{
    Write16(portnumber, data);
}

// Read a 16-bit value from the port
uint16_t Port16Bit::Read()
{
    return Read16(portnumber);
}