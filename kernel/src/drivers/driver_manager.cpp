#include <drivers/driver_manager.hpp>
#include <common/types.hpp>

using namespace kos::drivers;
using namespace kos::common;

 
DriverManager::DriverManager()
{
    numDrivers = 0;
};


bool DriverManager::AddDriver(Driver* drv)
{
    if (drv == 0)
        return false;

    const int maxDrivers = static_cast<int>(sizeof(drivers) / sizeof(drivers[0]));
    if (numDrivers >= maxDrivers)
        return false;

    drivers[numDrivers] = drv;
    numDrivers++;
    return true;
};

void DriverManager::ActivateAll()
{
    for (int32_t i=0; i<numDrivers; i++)
        drivers[i]->Activate();

}