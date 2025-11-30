#pragma once

#include <common/types.hpp>
#include <console/logger.hpp>

using namespace kos::common;

namespace kos { 
    namespace kernel {
        
        /*
        @brief Enumeration of boot stages.  
        High-level boot stages modeled loosely after a modern Linux boot flow.
        These allow consistent ordering and logging of progress without enforcing
        a specific init system. Stages should advance strictly monotonically.
        */
        enum class BootStage : uint8_t {
            EarlyInit = 0,        // Multiboot parsing, basic console, descriptor tables, interrupts
            MemoryInit,           // Physical memory manager, paging, heap
            DriverInit,           // Core hardware drivers (PCI, input, storage, network, etc.)
            FilesystemInit,       // Device scan + mount root / auxiliary filesystems
            InputInit,            // Input subsystem prepared (mouse state / sensitivity) prior to services
            ServicesInit,         // Kernel service registration/startup (time, banner, window manager, initd)
            MultitaskingStart,    // Scheduler + thread/process subsystem enabled
            GraphicsMode,         // Graphical environment prepared (framebuffer/window manager active)
            ShellInit,            // User interaction shell (text or graphical) started
            Complete              // Boot sequence finished; system in steady state
        };

        class BootProgressor {
            public:
            
                // `timeSourceMs` is an optional callback that returns a monotonically
                // increasing millisecond counter since boot (or 0 if unsupported).
                // Passing nullptr keeps behavior identical to pre-timing versions.
                using TimeSourceMs = uint32_t (*)();

                /*
                @brief Constructor for BootProgressor.
                @param timeSourceMs Optional time source callback for capturing stage times.        
                @return BootProgressor instance.
                */
                BootProgressor(TimeSourceMs timeSourceMs = nullptr): current(BootStage::EarlyInit), timeSource(timeSourceMs)
                {
                    for (int i = 0; i < kStageCount; ++i) {
                        stageTimesMs[i] = 0;
                    }
                    RecordTime(current);
                }

                /*
                @brief Advances the boot stage to the next specified stage.
                @param next The next boot stage to advance to.  
                */
                void Advance(BootStage next) {
                    // Allow idempotent advance to same stage but prevent regression
                    if (static_cast<int>(next) < static_cast<int>(current)) {
                        return; // ignore backward moves
                    }
                    current = next;
                    RecordTime(current);
                    Logger::LogKV("BOOT STAGE", StageName(current));
                }
                /*
                @brief Returns the current boot stage.
                @return Current boot stage.
                */
                BootStage Current() const { return current; }

                /*
                @brief Get the recorded time for a specific boot stage.
                @param s The boot stage to query.
                @return Recorded time in milliseconds for the specified stage.
                Returns the raw timestamp in milliseconds captured for a stage, or 0
                if no time source was provided or the stage was never reached.
                */
                uint32_t StageTimeMs(BootStage s) const {
                    int idx = static_cast<int>(s);
                    if (idx < 0 || idx >= kStageCount) return 0;
                    return stageTimesMs[idx];
                }

                /*
                @brief Logs a summary of boot timing information.   
                Logs a compact summary of per-stage timing deltas relative to
                EarlyInit (T0) and the previous stage, if a time source is present.
                */
                void LogTimingSummary() const {
                    if (!timeSource) return; // timing disabled

                    uint32_t t0 = StageTimeMs(BootStage::EarlyInit);
                    if (t0 == 0) return;

                    Logger::Log("Boot timing (ms) summary:");
                    BootStage prev = BootStage::EarlyInit;
                    uint32_t tPrev = t0;
                    for (int i = 0; i < kStageCount; ++i) {
                        BootStage s = static_cast<BootStage>(i);
                        uint32_t ts = StageTimeMs(s);
                        if (ts == 0) continue; // stage not reached
                        uint32_t fromStart = ts - t0;
                        uint32_t fromPrev = ts - tPrev;
                        // Single-line summary per stage for easier parsing
                        // Format: BootTiming stage=<name> start_ms=<N> prev_ms=<M>
                        char line[96];
                        char n1[16], n2[16];
                        toDecStr(n1, fromStart);
                        toDecStr(n2, fromPrev);
                        // Build line
                        int pos = 0;
                        auto append = [&](const char* s) {
                            while (*s && pos < (int)sizeof(line) - 1) { line[pos++] = *s++; }
                        };
                        append("BootTiming stage=");
                        append(StageName(s));
                        append(" start_ms=");
                        append(n1);
                        append(" prev_ms=");
                        append(n2);
                        line[pos] = 0;
                        Logger::Log(line);
                        prev = s;
                        tPrev = ts;
                    }
                }

                /*
                @brief Get the string name of a boot stage.
                @param s The boot stage.
                @return String name of the specified boot stage.
                */
                static const char* StageName(BootStage s) {
                    switch (s) {
                        case BootStage::EarlyInit: return "EarlyInit"; 
                        case BootStage::MemoryInit: return "MemoryInit";
                        case BootStage::DriverInit: return "DriverInit";
                        case BootStage::FilesystemInit: return "FilesystemInit";
                        case BootStage::InputInit: return "InputInit";
                        case BootStage::ServicesInit: return "ServicesInit";
                        case BootStage::MultitaskingStart: return "MultitaskingStart";
                        case BootStage::GraphicsMode: return "GraphicsMode";
                        case BootStage::ShellInit: return "ShellInit";
                        case BootStage::Complete: return "Complete";
                    }
                    return "Unknown";
                }

            private:
                static constexpr int kStageCount = 10; // keep in sync with BootStage enum

                /*
                @brief Records the time for a specific boot stage.
                @param s The boot stage to record time for.
                */
                void RecordTime(BootStage s) {
                    if (!timeSource) return;
                    int idx = static_cast<int>(s);
                    if (idx < 0 || idx >= kStageCount) return;
                    uint32_t now = timeSource();
                    if (stageTimesMs[idx] == 0) {
                        stageTimesMs[idx] = now;
                    }
                }

                /*
                @brief Converts an unsigned integer to a decimal string without using the standard library.
                @param out The output buffer to store the decimal string.
                @param v The unsigned integer value to convert.
                */
                static void toDecStr(char* out, uint32_t v) {
                    // Convert unsigned int to decimal string without stdlib.
                    char tmp[16]; int i = 0;
                    if (v == 0) { out[0] = '0'; out[1] = 0; return; }
                    while (v > 0 && i < 15) { tmp[i++] = '0' + (v % 10); v /= 10; }
                    // reverse into out
                    int j = 0; while (i > 0) { out[j++] = tmp[--i]; }
                    out[j] = 0;
                }

                /*
                @brief Disabled copy constructor and assignment operator.
                */
                BootStage current;
    
                /*
                @brief Time source callback for capturing stage times.
                */
                TimeSourceMs timeSource;
    
                /*
                @brief Array to store the time in milliseconds for each boot stage.
                */
                uint32_t stageTimesMs[kStageCount];
        };

    }
} // namespace kos::kernel
