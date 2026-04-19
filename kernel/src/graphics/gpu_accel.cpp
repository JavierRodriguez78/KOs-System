#include <graphics/gpu_accel.hpp>

#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_constants.hpp>
#include <console/logger.hpp>
#include <drivers/gpu/vmsvga.hpp>

namespace kos {
namespace gfx {
namespace gpu {

namespace {

GpuDeviceInfo g_info = {false, false, false, false, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
bool g_backend_ready = false;
const char* g_backend_name = "cpu-fallback";

static inline bool IsDisplayClass(uint8_t class_id, uint8_t subclass_id) {
    if (class_id != 0x03u) return false;
    return (subclass_id == 0x00u || subclass_id == 0x02u || subclass_id == 0x80u);
}

static inline bool IsVmwareSvga(const GpuDeviceInfo& d) {
    return d.vendor_id == 0x15ADu && d.device_id == 0x0405u && d.bar0_is_io;
}

static inline int ScoreDevice(const GpuDeviceInfo& d) {
    int score = d.compatible ? 100 : 0;
    if (d.subclass_id == 0x00u) score += 10; // VGA
    if (d.subclass_id == 0x02u) score += 9;  // 3D
    if (d.mmio_bar0 != 0) score += 3;
    if (d.bar0_is_io) score += 2;
    return score;
}

static uint32_t ReadBar0(kos::arch::x86::hardware::pci::PeripheralComponentIntercontroller& pci,
                         uint16_t bus, uint16_t device, uint16_t function) {
    uint32_t bar0 = pci.Read(bus, device, function, 0x10u);
    if (bar0 == 0xFFFFFFFFu) return 0;
    return bar0;
}

static uint32_t ReadMMIOBar0(kos::arch::x86::hardware::pci::PeripheralComponentIntercontroller& pci,
                             uint16_t bus, uint16_t device, uint16_t function) {
    uint32_t bar0 = ReadBar0(pci, bus, device, function);
    if (bar0 == 0) return 0;
    // BAR bit 0 set means I/O BAR, not MMIO.
    if ((bar0 & 0x1u) != 0) return 0;
    return (bar0 & 0xFFFFFFF0u);
}

} // namespace

void ProbePCI() {
    if (g_info.probed) return;
    g_info.probed = true;
    g_backend_ready = false;
    g_backend_name = "cpu-fallback";

    using namespace kos::arch::x86::hardware::pci;
    PeripheralComponentIntercontroller pci;

    GpuDeviceInfo best = g_info;
    int bestScore = -1;

    for (uint16_t bus = 0; bus < 256; ++bus) {
        for (uint16_t dev = 0; dev < 32; ++dev) {
            const uint16_t fnCount = pci.DeviceHasFunctions(bus, dev) ? 8 : 1;
            for (uint16_t fn = 0; fn < fnCount; ++fn) {
                auto d = pci.GetDeviceDescriptor(bus, dev, fn);
                if (d.vendor_id == PCI_INVALID_VENDOR_1 || d.vendor_id == PCI_INVALID_VENDOR_2) {
                    if (fn == 0) break;
                    continue;
                }
                if (!IsDisplayClass(d.class_id, d.subclass_id)) continue;

                GpuDeviceInfo cand = {};
                cand.probed = true;
                cand.present = true;
                cand.vendor_id = d.vendor_id;
                cand.device_id = d.device_id;
                cand.class_id = d.class_id;
                cand.subclass_id = d.subclass_id;
                cand.interface_id = d.interface_id;
                cand.bus = bus;
                cand.device = dev;
                cand.function = fn;
                cand.bar0 = ReadBar0(pci, bus, dev, fn);
                cand.bar0_is_io = (cand.bar0 & 0x1u) != 0;
                cand.mmio_bar0 = ReadMMIOBar0(pci, bus, dev, fn);
                cand.compatible = IsVmwareSvga(cand);

                const int score = ScoreDevice(cand);
                if (score > bestScore) {
                    best = cand;
                    bestScore = score;
                }
            }
        }
    }

    if (bestScore >= 0) {
        g_info = best;
        if (g_info.compatible) {
            kos::console::Logger::Log("GPU: VMSVGA candidate detected");
            kos::console::Logger::LogKV("GPU backend", "vmsvga(pending init)");
        } else {
            kos::console::Logger::Log("GPU: display controller detected (fallback compositor mode)");
            kos::console::Logger::LogKV("GPU backend", "cpu-fallback");
        }
    } else {
        kos::console::Logger::Log("GPU: no PCI display controller detected");
        kos::console::Logger::LogKV("GPU backend", "cpu-fallback");
    }
}

bool IsProbed() { return g_info.probed; }

bool HasDevice() { return g_info.present; }

bool IsAccelerationEnabled() { return g_backend_ready; }

bool InitializeBackend(uint32_t width, uint32_t height) {
    g_backend_ready = false;
    g_backend_name = "cpu-fallback";
    if (!g_info.present || !g_info.compatible) return false;

    if (IsVmwareSvga(g_info)) {
        if (kos::drivers::gpu::vmsvga::Initialize(g_info, width, height)) {
            g_backend_ready = true;
            g_backend_name = "vmsvga";
            kos::console::Logger::LogKV("GPU backend", "vmsvga");
            return true;
        }
        kos::console::Logger::Log("GPU: VMSVGA init failed, falling back to CPU compositor");
        return false;
    }
    return false;
}

void PresentFrame(uint32_t width, uint32_t height) {
    if (!g_backend_ready) return;
    if (g_backend_name == nullptr) return;
    if (g_backend_name[0] == 'v') {
        kos::drivers::gpu::vmsvga::PresentRect(0, 0, width, height);
    }
}

const GpuDeviceInfo& GetDeviceInfo() { return g_info; }

const char* ActiveBackendName() {
    return g_backend_name;
}

} // namespace gpu
} // namespace gfx
} // namespace kos
