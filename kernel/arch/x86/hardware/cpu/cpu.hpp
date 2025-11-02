// cpu.hpp - x86 CPU info header for KOS
#pragma once
#ifndef KOS_ARCH_X86_HARDWARE__CPU__CPU_HPP
#define KOS_ARCH_X86_HARDWARE__CPU__CPU_HPP
#include <common/types.hpp>

using namespace kos::common;

namespace kos {
    namespace arch {
        namespace x86 {
            namespace hardware {
                namespace cpu{

                    class cpu{
                        public:
                            struct CpuInfo {
                                int8_t vendor[13];
                                int8_t brand[49];
                                uint32_t family;
                                uint32_t model;
                                uint32_t stepping;
                                uint32_t cores;
                                uint32_t logical;
                                uint32_t mhz;
                            };

                            static void GetCpuInfo(CpuInfo& info);
                            static void PrintCpuInfo();

                    };
                } // namespace cpu
            } // namespace hardware
        } // namespace x86
    } // namespace arch
} // namespace kos

#endif // KOS_ARCH_X86_CPU_HPP
