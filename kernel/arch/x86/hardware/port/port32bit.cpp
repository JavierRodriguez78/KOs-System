#include "port32bit.hpp"


using namespace kos::arch::x86::hardware::port;


// Constructor
Port32Bit::Port32Bit(uint16_t portnumber): Port(portnumber)
{
}

// Destructor
Port32Bit::~Port32Bit()
{
}

// Write a 32-bit value to the port
void Port32Bit::Write(uint32_t data)
{
    Write32(portnumber, data);
}

// Read a 32-bit value from the port
uint32_t Port32Bit::Read()
{
    return Read32(portnumber);
}