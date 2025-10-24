#include "port8bit.hpp"


using namespace kos::arch::x86::hardware::port;

// Constructor
Port8Bit::Port8Bit(uint16_t portnumber): Port(portnumber)
{
}

// Destructor
Port8Bit::~Port8Bit()
{
}

// Write an 8-bit value to the port
void Port8Bit::Write(uint8_t data)
{
    Write8(portnumber, data);
}

// Read an 8-bit value from the port
uint8_t Port8Bit::Read()
{
    return Read8(portnumber);
}


