#include "port.hpp"
// Ensure consumers of this translation unit also have access to well-known
// I/O port constants for x86 by pulling in the local constants header.
#include "port_constants.hpp"
using namespace kos::common;
using namespace kos::arch::x86::hardware::port;

// Constructor
Port::Port(uint16_t portnumber)
{
    this->portnumber = portnumber;
}

// Destructor
Port::~Port()
{
}
