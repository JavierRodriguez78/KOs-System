#include <kernel/drivers.hpp>
#include <drivers/driver.hpp>
#include <drivers/keyboard/keyboard.hpp>
#include <drivers/keyboard/keyboard_driver.hpp>
#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/net/rtl8139/rtl8139.hpp>
#include <drivers/net/e1000/e1000.hpp>
#include <drivers/net/rtl8169/rtl8169.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/interrupts/interrupt_manager.hpp>
#include <console/logger.hpp>
#include <ui/input.hpp>
#include <process/scheduler.hpp>
#include <kernel/globals.hpp>

using namespace kos;
using namespace kos::process;

namespace kos { namespace kernel {

void InitDrivers(arch::x86::hardware::interrupts::InterruptManager* interrupts)
{
    Logger::Log("Initializing Hardware, Stage 1");
    static DriverManager drvManager;

    // Initialize shell pointer for keyboard handler fallback
    g_shell = &g_shell_instance;

    Logger::Log("Loading device drivers");
    static ShellKeyboardHandler skbhandler;
    static kos::drivers::keyboard::KeyboardDriver keyboard(interrupts, &skbhandler);
    drvManager.AddDriver(&keyboard);

    // In graphics mode, route mouse events to UI input
    static kos::drivers::mouse::MouseDriver mouse(interrupts, g_mouse_ui_handler_ptr);
    // Expose pointer for fallback polling path invoked by WindowManager
    g_mouse_driver_ptr = &mouse;
    drvManager.AddDriver(&mouse);

    // Add scaffold NIC drivers; they will self-probe for matching PCI devices
    static kos::drivers::net::rtl8139::Rtl8139Driver nic_rtl8139;
    static kos::drivers::net::e1000::E1000Driver   nic_e1000;
    static kos::drivers::net::rtl8169::Rtl8169Driver nic_rtl8169;
    drvManager.AddDriver(&nic_rtl8139);
    drvManager.AddDriver(&nic_e1000);
    drvManager.AddDriver(&nic_rtl8169);

    kos::arch::x86::hardware::pci::PeripheralComponentIntercontroller PCIController;
    PCIController.SelectDrivers(&drvManager);
    Logger::Log("Initializing Hardware, Stage 2");
    drvManager.ActivateAll();
    Logger::LogStatus("Hardware initialized Stage 2", true);
    Logger::Log("Initializing Hardware, Stage 3");
    interrupts->Activate();
    Logger::LogStatus("Hardware initialized Stage 3", true);
    
    // Explicitly enable timer interrupt (IRQ0) and keyboard interrupt (IRQ1)
    interrupts->EnableIRQ(0);
    Logger::LogStatus("Timer IRQ0 enabled", true);
    interrupts->EnableIRQ(1);
    Logger::LogStatus("Keyboard IRQ1 enabled", true);
    // Enable mouse IRQ (PS/2 auxiliary device)
    interrupts->EnableIRQ(12);
    Logger::LogStatus("Mouse IRQ12 enabled", true);
    
    // Enable preemptive scheduling now that interrupts are active
    g_scheduler->EnablePreemption();
    Logger::LogStatus("Preemptive scheduling enabled", true);
}

} }
