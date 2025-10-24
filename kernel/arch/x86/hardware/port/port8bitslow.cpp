#include "port8bitslow.hpp"


using namespace kos::arch::x86::hardware::port;



// Constructor
Port8BitSlow::Port8BitSlow(uint16_t portnumber): Port8Bit(portnumber)
{
}

// Destructor
Port8BitSlow::~Port8BitSlow()
{
}

// Write an 8-bit value to the port slowly
void Port8BitSlow::Write(uint8_t data)
{
    Write8Slow(portnumber, data);
}
