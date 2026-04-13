#include <kernel/drivers.hpp>
#include <drivers/driver.hpp>
#include <drivers/keyboard/keyboard.hpp>
#include <drivers/keyboard/keyboard_driver.hpp>
#include <drivers/mouse/mouse_driver.hpp>
#include <drivers/net/rtl8139/rtl8139.hpp>
#include <drivers/net/e1000/e1000.hpp>
#include <drivers/net/rtl8169/rtl8169.hpp>
#include <drivers/net/rtl8822be/rtl8822be.hpp>
#include <drivers/ps2/ps2.hpp>
#include <drivers/usb/usb_core.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/interrupts/interrupt_manager.hpp>
#include <arch/x86/hardware/port/port8bit.hpp>
#include <console/logger.hpp>
#include <ui/input.hpp>
#include <process/scheduler.hpp>
#include <kernel/globals.hpp>
#include <lib/serial.hpp>

using namespace kos;
using namespace kos::process;
using namespace kos::arch::x86::hardware::port;
using namespace kos::drivers::keyboard;
using namespace kos::drivers::net;
using namespace kos::drivers::mouse;
using namespace kos::arch::x86::hardware::interrupts;
using namespace kos::arch::x86::hardware::pci;

namespace kos { 
    namespace kernel {

        void InitDrivers(InterruptManager* interrupts)
        {
            Logger::Log("Initializing Hardware, Stage 1");
            static DriverManager drvManager; // Driver manager instance

            // Initialize shell pointer for keyboard handler fallback
            g_shell = &g_shell_instance;

            // Initialize PS/2 controller once before creating keyboard/mouse drivers
            Logger::Log("Initializing PS/2 controller");
            kos::drivers::ps2::PS2Controller::Instance().Init();

            // Initialize USB subsystem for USB keyboard/mouse support
            Logger::Log("Probing USB controllers");
            kos::drivers::usb::UsbCore::Init();

            Logger::Log("Loading device drivers");
            auto registerDriver = [&](const char* name, kos::drivers::Driver* driver) {
                const bool ok = drvManager.AddDriver(driver);
                Logger::LogStatus(name, ok);
            };

            static ShellKeyboardHandler skbhandler;
            static KeyboardDriver keyboard(interrupts, &skbhandler);
            // Expose keyboard driver globally for optional polling fallback
            g_keyboard_driver_ptr = &keyboard;
            registerDriver("Keyboard driver registered", &keyboard);

            // In graphics mode, route mouse events to UI input
            static MouseDriver mouse(interrupts, g_mouse_ui_handler_ptr);
            
            // Expose pointer for fallback polling path invoked by WindowManager
            g_mouse_driver_ptr = &mouse;
            registerDriver("Mouse driver registered", &mouse);

            // Add scaffold NIC drivers; they will self-probe for matching PCI devices
            static rtl8139::Rtl8139Driver nic_rtl8139;
            static e1000::E1000Driver   nic_e1000;
            static rtl8169::Rtl8169Driver nic_rtl8169;
            static rtl8822be::Rtl8822beDriver nic_rtl8822be;
            registerDriver("RTL8139 driver registered", &nic_rtl8139);
            registerDriver("E1000 driver registered", &nic_e1000);
            registerDriver("RTL8169 driver registered", &nic_rtl8169);
            registerDriver("RTL8822BE driver registered", &nic_rtl8822be);
            PeripheralComponentIntercontroller PCIController;
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
            // Ensure PIC cascade (IRQ2) is unmasked so slave IRQs (8-15) propagate
            interrupts->EnableIRQ(2);
            Logger::LogStatus("PIC cascade IRQ2 enabled", true);
            // Enable mouse IRQ (PS/2 auxiliary device)
            interrupts->EnableIRQ(12);
            Logger::LogStatus("Mouse IRQ12 enabled", true);
    
            // Enable preemptive scheduling now that interrupts are active
            g_scheduler->EnablePreemption();
            Logger::LogStatus("Preemptive scheduling enabled", true);

            // Verify and fix keyboard after all initialization
            // First MASK IRQ1 in PIC to prevent any keyboard interrupts during our work
            Logger::Log("Verifying keyboard setup...");
            Port8Bit picMasterData(0x21);
            uint8_t picMask = picMasterData.Read();
            picMasterData.Write(picMask | 0x02);  // Mask IRQ1
            kos::lib::serial_write("[PIC] Masked IRQ1 for setup\n");
            
            auto& ps2 = kos::drivers::ps2::PS2Controller::Instance();
            const char* hex = "0123456789ABCDEF";
            
            // Flush any stale data first
            int flushed = 0;
            while (ps2.ReadStatus() & 0x01) { (void)ps2.ReadData(); flushed++; }
            if (flushed) {
                kos::lib::serial_write("[PS2] Flushed stale: ");
                kos::lib::serial_putc('0' + flushed);
                kos::lib::serial_write("\n");
            }
            
            // Check PS/2 controller config
            ps2.WaitWrite(); ps2.WriteCommand(0x20);
            ps2.WaitRead();
            uint8_t cfg = ps2.ReadData();
            kos::lib::serial_write("[PS2] Config: 0x");
            kos::lib::serial_putc(hex[(cfg >> 4) & 0xF]);
            kos::lib::serial_putc(hex[cfg & 0xF]);
            kos::lib::serial_write("\n");
            
            // Ensure keyboard IRQ enabled (bit 0) and port enabled (bit 4 clear)
            bool needFix = !(cfg & 0x01) || (cfg & 0x10);
            if (needFix) {
                cfg |= 0x01;   // Enable keyboard IRQ
                cfg &= ~0x10;  // Enable keyboard port clock
                ps2.WaitWrite(); ps2.WriteCommand(0x60);
                ps2.WaitWrite(); ps2.WriteData(cfg);
                kos::lib::serial_write("[PS2] Fixed config\n");
            }
            
            // Ensure first PS/2 port is enabled
            ps2.WaitWrite(); ps2.WriteCommand(0xAE);
            
            // Send keyboard reset (0xFF)
            kos::lib::serial_write("[KBD] Sending reset...\n");
            ps2.WaitWrite(); ps2.WriteData(0xFF);
            
            // Wait for 0xFA ACK then 0xAA self-test
            bool gotAA = false;
            for (int i = 0; i < 1000000 && !gotAA; ++i) {
                if (ps2.ReadStatus() & 0x01) {
                    uint8_t resp = ps2.ReadData();
                    kos::lib::serial_write("[KBD] resp: 0x");
                    kos::lib::serial_putc(hex[(resp >> 4) & 0xF]);
                    kos::lib::serial_putc(hex[resp & 0xF]);
                    kos::lib::serial_write("\n");
                    if (resp == 0xAA) gotAA = true;
                }
            }
            
            // Send enable scanning (0xF4)
            kos::lib::serial_write("[KBD] Enabling scanning...\n");
            ps2.WaitWrite(); ps2.WriteData(0xF4);
            
            // Wait for ACK
            for (int i = 0; i < 100000; ++i) {
                if (ps2.ReadStatus() & 0x01) {
                    uint8_t ack = ps2.ReadData();
                    kos::lib::serial_write("[KBD] Enable resp: 0x");
                    kos::lib::serial_putc(hex[(ack >> 4) & 0xF]);
                    kos::lib::serial_putc(hex[ack & 0xF]);
                    kos::lib::serial_write("\n");
                    break;
                }
            }
            
            // Flush any remaining data before unmasking
            while (ps2.ReadStatus() & 0x01) { (void)ps2.ReadData(); }
            
            // Now unmask IRQ1 - keyboard should be ready
            picMask = picMasterData.Read();
            picMask &= ~0x02;  // Unmask IRQ1
            picMasterData.Write(picMask);
            kos::lib::serial_write("[PIC] Unmasked IRQ1, mask=0x");
            kos::lib::serial_putc(hex[(picMask >> 4) & 0xF]);
            kos::lib::serial_putc(hex[picMask & 0xF]);
            kos::lib::serial_write("\n");
            
            // Enable keyboard polling now that initialization is complete
            g_kbd_poll_enabled = true;
            kos::lib::serial_write("[KBD] Polling enabled\n");
            
            Logger::LogStatus("Keyboard verification complete", true);
        }
    }
 }
