#pragma once

#ifndef __KOS__DRIVERS__DRIVER_H
#define __KOS__DRIVERS__DRIVER_H

namespace kos
{
    namespace drivers
    {
        /*
        *@brief Abstract base class for all drivers.
        */
        class Driver
        {
            public:
                
                /*
                *@brief Constructor.
                */
                Driver();

                /*
                *@brief Destructor.
                */
                ~Driver();

                /*
                *@brief Activates the driver.
                */
                virtual void Activate();

                /*
                *@brief Resets the driver.
                */
                virtual int Reset();

                /*
                *@brief Deactivates the driver.
                */
                virtual void Deactivate();
        };
        
    }
}
#endif