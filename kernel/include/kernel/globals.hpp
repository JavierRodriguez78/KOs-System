#pragma once
#include <drivers/mouse/mouse_driver.hpp>
#include <console/shell.hpp>
#include <fs/filesystem.hpp>
#include <kernel/boot_options.hpp>

namespace kos {
namespace fs { extern Filesystem* g_fs_ptr; }
extern kos::drivers::mouse::MouseDriver* g_mouse_driver_ptr;
// Pointer to a MouseEventHandler used by MouseDriver; defined in kernel_globals.cpp
extern kos::drivers::mouse::MouseEventHandler* g_mouse_ui_handler_ptr;
extern kos::console::Shell* g_shell;
extern kos::console::Shell g_shell_instance;
// Global mouse poll mode (set during multiboot/boot options parsing)
extern kos::common::uint8_t g_mouse_poll_mode;
// Global display mode selection
extern kos::kernel::DisplayMode g_display_mode;
}
