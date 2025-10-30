#ifndef __KOS__DRIVERS__DRIVER_MANAGER_H
#define __KOS__DRIVERS__DRIVER_MANAGER_H

#include <drivers/driver.hpp>

namespace kos
{
    namespace drivers
    {

        class DriverManager
        {
            private:
                Driver* drivers[265];
                int numDrivers;
            
            public:
                DriverManager();
                void AddDriver(Driver*);
                void ActivateAll();
        };
    }
}
#endif