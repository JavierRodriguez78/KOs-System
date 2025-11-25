#pragma once

#include <common/types.hpp>
#include <console/logger.hpp>

namespace kos { namespace kernel {

// High-level boot stages modeled loosely after a modern Linux boot flow.
// These allow consistent ordering and logging of progress without enforcing
// a specific init system. Stages should advance strictly monotonically.
enum class BootStage : kos::common::uint8_t {
    EarlyInit = 0,        // Multiboot parsing, basic console, descriptor tables, interrupts
    MemoryInit,           // Physical memory manager, paging, heap
    DriverInit,           // Core hardware drivers (PCI, input, storage, network, etc.)
    FilesystemInit,       // Device scan + mount root / auxiliary filesystems
    ServicesInit,         // Kernel service registration/startup (time, banner, window manager, initd)
    MultitaskingStart,    // Scheduler + thread/process subsystem enabled
    GraphicsMode,         // Graphical environment prepared (framebuffer/window manager active)
    ShellInit,            // User interaction shell (text or graphical) started
    Complete              // Boot sequence finished; system in steady state
};

class BootProgressor {
public:
    BootProgressor() : current(BootStage::EarlyInit) {}

    void Advance(BootStage next) {
        // Allow idempotent advance to same stage but prevent regression
        if (static_cast<int>(next) < static_cast<int>(current)) {
            return; // ignore backward moves
        }
        current = next;
        Logger::LogKV("BOOT STAGE", StageName(current));
    }

    BootStage Current() const { return current; }

    static const char* StageName(BootStage s) {
        switch (s) {
            case BootStage::EarlyInit: return "EarlyInit";
            case BootStage::MemoryInit: return "MemoryInit";
            case BootStage::DriverInit: return "DriverInit";
            case BootStage::FilesystemInit: return "FilesystemInit";
            case BootStage::ServicesInit: return "ServicesInit";
            case BootStage::MultitaskingStart: return "MultitaskingStart";
            case BootStage::GraphicsMode: return "GraphicsMode";
            case BootStage::ShellInit: return "ShellInit";
            case BootStage::Complete: return "Complete";
        }
        return "Unknown";
    }

private:
    BootStage current;
};

}} // namespace kos::kernel
