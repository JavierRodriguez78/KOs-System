#include <drivers/driver.hpp>
#include <common/types.hpp>

using namespace kos::drivers;
using namespace kos::common;


 Driver::Driver()
 {

 };
   
   
Driver::~Driver()
{
};

void Driver::Activate()
{

};
int Driver::Reset()
{
    return 0;
};
void Driver::Deactivate()
{

};
  
DriverManager::DriverManager()
{
    numDrivers = 0;
};


void DriverManager::AddDriver(Driver* drv)
{
    drivers[numDrivers] = drv;
    numDrivers++;
};

void DriverManager::ActivateAll()
{
    for (int32_t i=0; i<numDrivers; i++)
        drivers[i]->Activate();

}