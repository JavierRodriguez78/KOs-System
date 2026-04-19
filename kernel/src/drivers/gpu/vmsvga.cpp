#include <drivers/gpu/vmsvga.hpp>

#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/port/port32bit.hpp>
#include <console/logger.hpp>
#include <lib/string.hpp>
#include <memory/heap.hpp>
#include <memory/paging.hpp>
#include <memory/pmm.hpp>

namespace kos {
namespace drivers {
namespace gpu {
namespace vmsvga {

namespace {

using kos::common::uint16_t;
using kos::common::uint32_t;
using kos::common::uint8_t;

static constexpr uint16_t kVendorVmware = 0x15ADu;
static constexpr uint16_t kDeviceSvga2 = 0x0405u;

static constexpr uint32_t kSvgaId0 = 0x90000000u;
static constexpr uint32_t kSvgaId1 = 0x90000001u;
static constexpr uint32_t kSvgaId2 = 0x90000002u;

enum SvgaReg : uint32_t {
    SVGA_REG_ID = 0,
    SVGA_REG_ENABLE = 1,
    SVGA_REG_WIDTH = 2,
    SVGA_REG_HEIGHT = 3,
    SVGA_REG_CAPABILITIES = 17,
    SVGA_REG_MEM_START = 18,
    SVGA_REG_MEM_SIZE = 19,
    SVGA_REG_CONFIG_DONE = 20,
    SVGA_REG_SYNC = 21,
    SVGA_REG_BUSY = 22,
    SVGA_REG_GUEST_ID = 23
};

enum SvgaFifoReg : uint32_t {
    FIFO_MIN = 0,
    FIFO_MAX = 1,
    FIFO_NEXT_CMD = 2,
    FIFO_STOP = 3,
    FIFO_CAPABILITIES = 4
};

static constexpr uint32_t SVGA_CMD_UPDATE = 1;
static constexpr uint32_t SVGA_CMD_RECT_COPY = 3;
static constexpr uint32_t SVGA_CMD_FRONT_ROP_FILL = 29;
static constexpr uint32_t SVGA_CMD_DEFINE_GMRFB = 36;
static constexpr uint32_t SVGA_CMD_BLIT_GMRFB_TO_SCREEN = 37;
static constexpr uint32_t SVGA_CMD_DEFINE_GMR2 = 41;
static constexpr uint32_t SVGA_CMD_REMAP_GMR2 = 42;
static constexpr uint32_t SVGA_CAP_RECT_COPY = 0x00000002u;
static constexpr uint32_t SVGA_CAP_EXTENDED_FIFO = 0x00008000u;
static constexpr uint32_t SVGA_CAP_GMR = 0x00100000u;
static constexpr uint32_t SVGA_CAP_GMR2 = 0x00400000u;
static constexpr uint32_t SVGA_FIFO_CAP_ACCELFRONT = (1u << 1);
static constexpr uint32_t SVGA_FIFO_CAP_GMR2 = (1u << 8);
static constexpr uint32_t SVGA_ROP_COPY = 0x000000CCu;
static constexpr uint32_t SVGA_ROP_PATCOPY = 0x000000F0u;
static constexpr uint32_t SVGA_GMR_NULL = 0xFFFFFFFFu;
static constexpr uint32_t kUploadGmrId = 1u;
static constexpr uint32_t kUploadVirtBase = 0x13000000u;
static constexpr uint32_t kDefaultColorDepth = 24u;

struct State {
    bool ready;
    uint16_t io_base;
    volatile uint32_t* fifo;
    uint32_t fifo_bytes;
    uint32_t caps;
    uint32_t fifo_caps;
    bool front_bound;
    uint8_t* frontbuffer_base;
    uint32_t width;
    uint32_t height;
    uint32_t pitch_bytes;
    uint8_t bpp;
    bool dirty;
    uint32_t dirty_x1;
    uint32_t dirty_y1;
    uint32_t dirty_x2;
    uint32_t dirty_y2;
    bool fifo_pending;
    bool upload_ready;
    uint8_t* upload_buffer;
    uint32_t upload_bytes;
    uint32_t upload_pages;
    phys_addr_t* upload_phys_pages;
    bool upload_path_logged;
    bool upload_fallback_logged;
};

State g_state = {false, 0, nullptr, 0, 0, 0, false, nullptr, 0, 0, 0, 0, false, 0, 0, 0, 0, false, false, nullptr, 0, 0, nullptr, false, false};

static uint32_t RegRead(uint16_t io_base, uint32_t reg) {
    kos::arch::x86::hardware::port::Port32Bit idx(io_base + 0u);
    kos::arch::x86::hardware::port::Port32Bit val(io_base + 1u);
    idx.Write(reg);
    return val.Read();
}

static void RegWrite(uint16_t io_base, uint32_t reg, uint32_t value) {
    kos::arch::x86::hardware::port::Port32Bit idx(io_base + 0u);
    kos::arch::x86::hardware::port::Port32Bit val(io_base + 1u);
    idx.Write(reg);
    val.Write(value);
}

static void WaitIdle(uint16_t io_base) {
    for (uint32_t i = 0; i < 1000000u; ++i) {
        if (RegRead(io_base, SVGA_REG_BUSY) == 0) return;
    }
}

static void SyncFifo(uint16_t io_base) {
    RegWrite(io_base, SVGA_REG_SYNC, 1);
    WaitIdle(io_base);
}

static uint32_t FifoGet(uint32_t index) {
    return g_state.fifo[index];
}

static void FifoSet(uint32_t index, uint32_t value) {
    g_state.fifo[index] = value;
}

static bool EnsureFifoSpace(uint32_t bytesNeeded) {
    if (!g_state.fifo) return false;

    for (uint32_t retry = 0; retry < 8; ++retry) {
        const uint32_t min = FifoGet(FIFO_MIN);
        const uint32_t max = FifoGet(FIFO_MAX);
        const uint32_t next = FifoGet(FIFO_NEXT_CMD);
        const uint32_t stop = FifoGet(FIFO_STOP);

        uint32_t freeBytes = 0;
        if (next >= stop) {
            freeBytes = (max - next) + (stop - min);
        } else {
            freeBytes = stop - next;
        }

        if (freeBytes > bytesNeeded) return true;
        SyncFifo(g_state.io_base);
    }
    return false;
}

static void FifoPush(uint32_t value) {
    uint32_t next = FifoGet(FIFO_NEXT_CMD);
    const uint32_t min = FifoGet(FIFO_MIN);
    const uint32_t max = FifoGet(FIFO_MAX);

    g_state.fifo[next / 4u] = value;
    next += 4u;
    if (next >= max) next = min;
    FifoSet(FIFO_NEXT_CMD, next);
}

static bool FifoSubmitUpdate(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    constexpr uint32_t kWords = 5u;
    constexpr uint32_t kBytes = kWords * 4u;
    if (!EnsureFifoSpace(kBytes)) return false;

    FifoPush(SVGA_CMD_UPDATE);
    FifoPush(x);
    FifoPush(y);
    FifoPush(w);
    FifoPush(h);
    g_state.fifo_pending = true;
    return true;
}

static bool FifoSubmitRectCopy(uint32_t srcX, uint32_t srcY, uint32_t dstX, uint32_t dstY, uint32_t w, uint32_t h) {
    constexpr uint32_t kWords = 7u;
    constexpr uint32_t kBytes = kWords * 4u;
    if (!EnsureFifoSpace(kBytes)) return false;

    FifoPush(SVGA_CMD_RECT_COPY);
    FifoPush(srcX);
    FifoPush(srcY);
    FifoPush(dstX);
    FifoPush(dstY);
    FifoPush(w);
    FifoPush(h);
    g_state.fifo_pending = true;
    return true;
}

static bool FifoSubmitFrontRopFill(uint32_t color, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    constexpr uint32_t kWords = 7u;
    constexpr uint32_t kBytes = kWords * 4u;
    if (!EnsureFifoSpace(kBytes)) return false;

    FifoPush(SVGA_CMD_FRONT_ROP_FILL);
    FifoPush(color);
    FifoPush(x);
    FifoPush(y);
    FifoPush(w);
    FifoPush(h);
    FifoPush(SVGA_ROP_PATCOPY);
    g_state.fifo_pending = true;
    return true;
}

static bool FifoSubmitDefineGMR2(uint32_t gmrId, uint32_t numPages) {
    constexpr uint32_t kWords = 3u;
    if (!EnsureFifoSpace(kWords * 4u)) return false;
    FifoPush(SVGA_CMD_DEFINE_GMR2);
    FifoPush(gmrId);
    FifoPush(numPages);
    g_state.fifo_pending = true;
    return true;
}

static bool FifoSubmitRemapGMR2(uint32_t gmrId, uint32_t numPages, const phys_addr_t* pages) {
    const uint32_t kWords = 1u + 4u + numPages;
    if (!EnsureFifoSpace(kWords * 4u)) return false;
    FifoPush(SVGA_CMD_REMAP_GMR2);
    FifoPush(gmrId);
    FifoPush(0u); // flags: PPN32 list inline
    FifoPush(0u); // offsetPages
    FifoPush(numPages);
    for (uint32_t i = 0; i < numPages; ++i) {
        FifoPush((uint32_t)(pages[i] >> 12));
    }
    g_state.fifo_pending = true;
    return true;
}

static bool FifoSubmitDefineGMRFB(uint32_t gmrId, uint32_t offset, uint32_t bytesPerLine, uint32_t bitsPerPixel, uint32_t colorDepth) {
    constexpr uint32_t kWords = 5u;
    if (!EnsureFifoSpace(kWords * 4u)) return false;
    FifoPush(SVGA_CMD_DEFINE_GMRFB);
    FifoPush(gmrId);
    FifoPush(offset);
    FifoPush(bytesPerLine);
    FifoPush((bitsPerPixel & 0xFFu) | ((colorDepth & 0xFFu) << 8));
    g_state.fifo_pending = true;
    return true;
}

static bool FifoSubmitBlitGMRFBToScreen(uint32_t srcX, uint32_t srcY, uint32_t dstX, uint32_t dstY, uint32_t w, uint32_t h) {
    constexpr uint32_t kWords = 8u;
    if (!EnsureFifoSpace(kWords * 4u)) return false;
    FifoPush(SVGA_CMD_BLIT_GMRFB_TO_SCREEN);
    FifoPush(srcX);
    FifoPush(srcY);
    FifoPush((int32_t)dstX);
    FifoPush((int32_t)dstY);
    FifoPush((int32_t)(dstX + w));
    FifoPush((int32_t)(dstY + h));
    FifoPush(0u); // destScreenId: primary legacy screen
    g_state.fifo_pending = true;
    return true;
}

static inline uint32_t MinU32(uint32_t a, uint32_t b) { return (a < b) ? a : b; }

static void ReleaseUploadSurface() {
    if (g_state.upload_ready && g_state.fifo) {
        if (FifoSubmitDefineGMRFB(SVGA_GMR_NULL, 0u, 0u, 0u, 0u) &&
            FifoSubmitDefineGMR2(kUploadGmrId, 0u)) {
            SyncFifo(g_state.io_base);
            g_state.fifo_pending = false;
        }
    }

    if (g_state.upload_buffer && g_state.upload_pages != 0u) {
        kos::memory::Paging::UnmapRange((virt_addr_t)kUploadVirtBase, g_state.upload_pages * 4096u);
    }

    if (g_state.upload_phys_pages) {
        for (uint32_t i = 0; i < g_state.upload_pages; ++i) {
            if (g_state.upload_phys_pages[i] != 0u) {
                kos::memory::PMM::FreeFrame(g_state.upload_phys_pages[i]);
            }
        }
        kos::memory::Heap::Free(g_state.upload_phys_pages);
    }

    g_state.upload_ready = false;
    g_state.upload_buffer = nullptr;
    g_state.upload_bytes = 0;
    g_state.upload_pages = 0;
    g_state.upload_phys_pages = nullptr;
    g_state.upload_path_logged = false;
    g_state.upload_fallback_logged = false;
}

static void MarkDirty(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return;
    if (!g_state.dirty) {
        g_state.dirty = true;
        g_state.dirty_x1 = x;
        g_state.dirty_y1 = y;
        g_state.dirty_x2 = x + w;
        g_state.dirty_y2 = y + h;
        return;
    }
    if (x < g_state.dirty_x1) g_state.dirty_x1 = x;
    if (y < g_state.dirty_y1) g_state.dirty_y1 = y;
    if (x + w > g_state.dirty_x2) g_state.dirty_x2 = x + w;
    if (y + h > g_state.dirty_y2) g_state.dirty_y2 = y + h;
}

static bool EnsureUploadSurface() {
    if (g_state.upload_ready) return true;
    if ((g_state.caps & SVGA_CAP_GMR2) == 0u) {
        if (!g_state.upload_fallback_logged) {
            kos::console::Logger::Log("VMSVGA: GMR2 not available, upload fallback to CPU frontbuffer");
            g_state.upload_fallback_logged = true;
        }
        return false;
    }
    if ((g_state.caps & SVGA_CAP_EXTENDED_FIFO) != 0u &&
        (g_state.fifo_caps & SVGA_FIFO_CAP_GMR2) == 0u) {
        if (!g_state.upload_fallback_logged) {
            kos::console::Logger::Log("VMSVGA: FIFO GMR2 not advertised, upload fallback to CPU frontbuffer");
            g_state.upload_fallback_logged = true;
        }
        return false;
    }
    if (g_state.pitch_bytes == 0 || g_state.height == 0) return false;

    const uint32_t bytes = g_state.pitch_bytes * g_state.height;
    const uint32_t pages = (bytes + 4095u) / 4096u;
    if (pages == 0) return false;

    if (!g_state.upload_buffer || g_state.upload_pages != pages || g_state.upload_bytes != bytes) {
        if (g_state.upload_buffer) {
            ReleaseUploadSurface();
        }

        phys_addr_t* physPages = (phys_addr_t*)kos::memory::Heap::Alloc(sizeof(phys_addr_t) * pages, 16);
        if (!physPages) return false;

        for (uint32_t i = 0; i < pages; ++i) {
            phys_addr_t frame = kos::memory::PMM::AllocFrame();
            if (!frame) return false;
            physPages[i] = frame;
            kos::memory::Paging::MapPage((virt_addr_t)(kUploadVirtBase + i * 4096u), frame,
                                         kos::memory::Paging::Present | kos::memory::Paging::RW);
        }

        g_state.upload_phys_pages = physPages;
        g_state.upload_buffer = (uint8_t*)kUploadVirtBase;
        g_state.upload_bytes = bytes;
        g_state.upload_pages = pages;
        g_state.upload_ready = false;

        for (uint32_t i = 0; i < bytes; ++i) g_state.upload_buffer[i] = 0;
    }

    if (!FifoSubmitDefineGMR2(kUploadGmrId, pages)) return false;
    if (!FifoSubmitRemapGMR2(kUploadGmrId, pages, g_state.upload_phys_pages)) return false;
    if (!FifoSubmitDefineGMRFB(kUploadGmrId, 0u, g_state.pitch_bytes, g_state.bpp ? g_state.bpp : 32u, kDefaultColorDepth)) return false;
    SyncFifo(g_state.io_base);
    g_state.fifo_pending = false;
    g_state.upload_ready = true;
    kos::console::Logger::Log("VMSVGA: GMRFB upload surface ready");
    return true;
}

static void FrontFillFallback(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_state.front_bound || !g_state.frontbuffer_base) return;
    if (x >= g_state.width || y >= g_state.height) return;
    w = MinU32(w, g_state.width - x);
    h = MinU32(h, g_state.height - y);

    if (g_state.bpp == 32) {
        for (uint32_t row = 0; row < h; ++row) {
            uint32_t* dst = (uint32_t*)(g_state.frontbuffer_base + (y + row) * g_state.pitch_bytes) + x;
            for (uint32_t col = 0; col < w; ++col) dst[col] = color;
        }
    } else {
        const uint8_t b = (uint8_t)(color & 0xFFu);
        const uint8_t g = (uint8_t)((color >> 8) & 0xFFu);
        const uint8_t r = (uint8_t)((color >> 16) & 0xFFu);
        for (uint32_t row = 0; row < h; ++row) {
            uint8_t* dst = g_state.frontbuffer_base + (y + row) * g_state.pitch_bytes + x * 3u;
            for (uint32_t col = 0; col < w; ++col) {
                dst[col * 3u + 0u] = b;
                dst[col * 3u + 1u] = g;
                dst[col * 3u + 2u] = r;
            }
        }
    }
}

static void FrontRectCopyFallback(uint32_t srcX, uint32_t srcY, uint32_t dstX, uint32_t dstY, uint32_t w, uint32_t h) {
    if (!g_state.front_bound || !g_state.frontbuffer_base) return;
    if (srcX >= g_state.width || srcY >= g_state.height) return;
    if (dstX >= g_state.width || dstY >= g_state.height) return;
    w = MinU32(w, MinU32(g_state.width - srcX, g_state.width - dstX));
    h = MinU32(h, MinU32(g_state.height - srcY, g_state.height - dstY));
    if (w == 0 || h == 0) return;

    const uint32_t pixelBytes = (g_state.bpp == 32) ? 4u : 3u;
    if (dstY > srcY) {
        for (uint32_t row = h; row > 0; --row) {
            uint8_t* dst = g_state.frontbuffer_base + (dstY + row - 1u) * g_state.pitch_bytes + dstX * pixelBytes;
            uint8_t* src = g_state.frontbuffer_base + (srcY + row - 1u) * g_state.pitch_bytes + srcX * pixelBytes;
            kos::lib::String::memmove(dst, src, w * pixelBytes);
        }
    } else {
        for (uint32_t row = 0; row < h; ++row) {
            uint8_t* dst = g_state.frontbuffer_base + (dstY + row) * g_state.pitch_bytes + dstX * pixelBytes;
            uint8_t* src = g_state.frontbuffer_base + (srcY + row) * g_state.pitch_bytes + srcX * pixelBytes;
            kos::lib::String::memmove(dst, src, w * pixelBytes);
        }
    }
}

static void FrontUploadFallback(const uint32_t* src,
                                uint32_t src_width,
                                uint32_t src_height,
                                uint32_t src_pitch_pixels,
                                uint32_t dst_x,
                                uint32_t dst_y) {
    if (!g_state.front_bound || !g_state.frontbuffer_base || !src) return;
    if (dst_x >= g_state.width || dst_y >= g_state.height) return;
    const uint32_t copyW = MinU32(src_width, g_state.width - dst_x);
    const uint32_t copyH = MinU32(src_height, g_state.height - dst_y);

    if (g_state.bpp == 32) {
        for (uint32_t row = 0; row < copyH; ++row) {
            uint32_t* dst = (uint32_t*)(g_state.frontbuffer_base + (dst_y + row) * g_state.pitch_bytes) + dst_x;
            const uint32_t* srcRow = src + row * src_pitch_pixels;
            kos::lib::String::memmove(dst, srcRow, copyW * 4u);
        }
    } else {
        for (uint32_t row = 0; row < copyH; ++row) {
            uint8_t* dst = g_state.frontbuffer_base + (dst_y + row) * g_state.pitch_bytes + dst_x * 3u;
            const uint32_t* srcRow = src + row * src_pitch_pixels;
            for (uint32_t col = 0; col < copyW; ++col) {
                uint32_t argb = srcRow[col];
                dst[col * 3u + 0u] = (uint8_t)(argb & 0xFFu);
                dst[col * 3u + 1u] = (uint8_t)((argb >> 8) & 0xFFu);
                dst[col * 3u + 2u] = (uint8_t)((argb >> 16) & 0xFFu);
            }
        }
    }
}

} // namespace

bool Initialize(const kos::gfx::gpu::GpuDeviceInfo& dev, uint32_t width, uint32_t height) {
    g_state.ready = false;
    g_state.fifo = nullptr;
    g_state.fifo_bytes = 0;
    g_state.caps = 0;
    g_state.fifo_caps = 0;
    g_state.front_bound = false;
    g_state.frontbuffer_base = nullptr;
    g_state.width = width;
    g_state.height = height;
    g_state.pitch_bytes = 0;
    g_state.bpp = 0;
    g_state.dirty = false;
    g_state.fifo_pending = false;
    g_state.upload_ready = false;
    g_state.upload_buffer = nullptr;
    g_state.upload_bytes = 0;
    g_state.upload_pages = 0;
    g_state.upload_phys_pages = nullptr;
    g_state.upload_path_logged = false;
    g_state.upload_fallback_logged = false;

    if (!dev.present) return false;
    if (dev.vendor_id != kVendorVmware) return false;
    if (dev.device_id != kDeviceSvga2) return false;

    using namespace kos::arch::x86::hardware::pci;
    PeripheralComponentIntercontroller pci;

    const uint32_t rawBar0 = pci.Read(dev.bus, dev.device, dev.function, 0x10u);
    if (rawBar0 == 0xFFFFFFFFu) return false;
    if ((rawBar0 & 0x1u) == 0) {
        kos::console::Logger::Log("VMSVGA: BAR0 is not I/O mapped");
        return false;
    }
    g_state.io_base = (uint16_t)(rawBar0 & 0xFFFCu);
    if (g_state.io_base == 0) return false;

    // Negotiate SVGA ID.
    RegWrite(g_state.io_base, SVGA_REG_ID, kSvgaId2);
    uint32_t negotiated = RegRead(g_state.io_base, SVGA_REG_ID);
    if (negotiated != kSvgaId2) {
        RegWrite(g_state.io_base, SVGA_REG_ID, kSvgaId1);
        negotiated = RegRead(g_state.io_base, SVGA_REG_ID);
        if (negotiated != kSvgaId1) {
            RegWrite(g_state.io_base, SVGA_REG_ID, kSvgaId0);
            negotiated = RegRead(g_state.io_base, SVGA_REG_ID);
            if (negotiated != kSvgaId0) {
                kos::console::Logger::Log("VMSVGA: failed to negotiate SVGA ID");
                return false;
            }
        }
    }

    g_state.caps = RegRead(g_state.io_base, SVGA_REG_CAPABILITIES);
    RegWrite(g_state.io_base, SVGA_REG_ENABLE, 0);
    RegWrite(g_state.io_base, SVGA_REG_WIDTH, width);
    RegWrite(g_state.io_base, SVGA_REG_HEIGHT, height);

    const uint32_t fifoPhys = RegRead(g_state.io_base, SVGA_REG_MEM_START);
    const uint32_t fifoSize = RegRead(g_state.io_base, SVGA_REG_MEM_SIZE);
    if (fifoPhys == 0 || fifoSize < 4096u) {
        kos::console::Logger::Log("VMSVGA: invalid FIFO memory");
        return false;
    }

    // Map FIFO memory.
    constexpr uint32_t ID_MAP_END = 64u * 1024u * 1024u;
    constexpr uint32_t VIRT_FIFO_BASE = 0x12000000u;
    if (fifoPhys < ID_MAP_END) {
        g_state.fifo = (volatile uint32_t*)(uint32_t)fifoPhys;
    } else {
        const uint32_t mapSize = (fifoSize + 4095u) & ~4095u;
        kos::memory::Paging::MapRange((virt_addr_t)VIRT_FIFO_BASE,
                          (phys_addr_t)fifoPhys,
                                      mapSize,
                                      kos::memory::Paging::Present |
                                      kos::memory::Paging::RW |
                                      kos::memory::Paging::CacheDisable);
        g_state.fifo = (volatile uint32_t*)VIRT_FIFO_BASE;
    }
    g_state.fifo_bytes = fifoSize;

    // Initialize FIFO ring.
    FifoSet(FIFO_MIN, 16u);
    FifoSet(FIFO_MAX, fifoSize);
    FifoSet(FIFO_NEXT_CMD, 16u);
    FifoSet(FIFO_STOP, 16u);

    if ((g_state.caps & SVGA_CAP_EXTENDED_FIFO) != 0u) {
        g_state.fifo_caps = FifoGet(FIFO_CAPABILITIES);
        if ((g_state.fifo_caps & SVGA_FIFO_CAP_GMR2) != 0u) {
            kos::console::Logger::Log("VMSVGA: FIFO GMR2 capability detected");
        } else {
            kos::console::Logger::Log("VMSVGA: FIFO GMR2 capability not detected");
        }
        if ((g_state.fifo_caps & SVGA_FIFO_CAP_ACCELFRONT) != 0u) {
            kos::console::Logger::Log("VMSVGA: FIFO ACCELFRONT capability detected");
        } else {
            kos::console::Logger::Log("VMSVGA: FIFO ACCELFRONT capability not detected");
        }
    }

    RegWrite(g_state.io_base, SVGA_REG_GUEST_ID, 0x4B4F5301u); // "KOS\x01"
    RegWrite(g_state.io_base, SVGA_REG_ENABLE, 1u);
    RegWrite(g_state.io_base, SVGA_REG_CONFIG_DONE, 1u);

    g_state.ready = true;
    kos::console::Logger::Log("VMSVGA: accelerated backend initialized");
    return true;
}

bool IsReady() { return g_state.ready; }

void BindFramebuffer(uint8_t* framebuffer_base,
                     uint32_t width,
                     uint32_t height,
                     uint32_t pitch_bytes,
                     uint8_t bpp) {
    const bool uploadConfigChanged = g_state.frontbuffer_base != framebuffer_base ||
                                     g_state.width != width ||
                                     g_state.height != height ||
                                     g_state.pitch_bytes != pitch_bytes ||
                                     g_state.bpp != bpp;
    g_state.frontbuffer_base = framebuffer_base;
    g_state.front_bound = framebuffer_base != nullptr;
    g_state.width = width;
    g_state.height = height;
    g_state.pitch_bytes = pitch_bytes;
    g_state.bpp = bpp;
    if (uploadConfigChanged) {
        ReleaseUploadSurface();
    }
    if (g_state.ready && g_state.front_bound && !g_state.upload_ready) {
        (void)EnsureUploadSurface();
    }
}

void FillRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    if (!g_state.ready || !g_state.front_bound || w == 0 || h == 0) return;
    const bool accelFront = (g_state.fifo_caps & SVGA_FIFO_CAP_ACCELFRONT) != 0u;
    if (!accelFront || !FifoSubmitFrontRopFill(color, x, y, w, h)) {
        FrontFillFallback(x, y, w, h, color);
    }
    MarkDirty(x, y, w, h);
}

void BlitRect(uint32_t src_x, uint32_t src_y, uint32_t dst_x, uint32_t dst_y, uint32_t w, uint32_t h) {
    if (!g_state.ready || !g_state.front_bound || w == 0 || h == 0) return;
    if ((g_state.caps & SVGA_CAP_RECT_COPY) != 0u) {
        if (!FifoSubmitRectCopy(src_x, src_y, dst_x, dst_y, w, h)) {
            FrontRectCopyFallback(src_x, src_y, dst_x, dst_y, w, h);
        }
    } else {
        FrontRectCopyFallback(src_x, src_y, dst_x, dst_y, w, h);
    }
    MarkDirty(dst_x, dst_y, w, h);
}

void UploadRect(const uint32_t* src,
                uint32_t src_width,
                uint32_t src_height,
                uint32_t src_pitch_pixels,
                uint32_t dst_x,
                uint32_t dst_y) {
    if (!g_state.ready || !g_state.front_bound || !src) return;
    const uint32_t copyW = MinU32(src_width, g_state.width > dst_x ? g_state.width - dst_x : 0u);
    const uint32_t copyH = MinU32(src_height, g_state.height > dst_y ? g_state.height - dst_y : 0u);
    if (copyW == 0 || copyH == 0) return;

    if (!EnsureUploadSurface()) {
        if (!g_state.upload_fallback_logged) {
            kos::console::Logger::Log("VMSVGA: upload fallback to CPU frontbuffer");
            g_state.upload_fallback_logged = true;
        }
        FrontUploadFallback(src, src_width, src_height, src_pitch_pixels, dst_x, dst_y);
        MarkDirty(dst_x, dst_y, copyW, copyH);
        return;
    }

    if (g_state.bpp == 32) {
        for (uint32_t row = 0; row < copyH; ++row) {
            uint32_t* dst = (uint32_t*)(g_state.upload_buffer + (dst_y + row) * g_state.pitch_bytes) + dst_x;
            const uint32_t* srcRow = src + row * src_pitch_pixels;
            kos::lib::String::memmove(dst, srcRow, copyW * 4u);
        }
    } else {
        for (uint32_t row = 0; row < copyH; ++row) {
            uint8_t* dst = g_state.upload_buffer + (dst_y + row) * g_state.pitch_bytes + dst_x * 3u;
            const uint32_t* srcRow = src + row * src_pitch_pixels;
            for (uint32_t col = 0; col < copyW; ++col) {
                uint32_t argb = srcRow[col];
                dst[col * 3u + 0u] = (uint8_t)(argb & 0xFFu);
                dst[col * 3u + 1u] = (uint8_t)((argb >> 8) & 0xFFu);
                dst[col * 3u + 2u] = (uint8_t)((argb >> 16) & 0xFFu);
            }
        }
    }

    if (!FifoSubmitBlitGMRFBToScreen(dst_x, dst_y, dst_x, dst_y, copyW, copyH)) {
        if (!g_state.upload_fallback_logged) {
            kos::console::Logger::Log("VMSVGA: upload fallback to CPU frontbuffer");
            g_state.upload_fallback_logged = true;
        }
        FrontUploadFallback(src, src_width, src_height, src_pitch_pixels, dst_x, dst_y);
        MarkDirty(dst_x, dst_y, copyW, copyH);
        return;
    }

    if (!g_state.upload_path_logged) {
        kos::console::Logger::Log("VMSVGA: compositor uploads using GMRFB blit path");
        g_state.upload_path_logged = true;
    }

    MarkDirty(dst_x, dst_y, copyW, copyH);
}

void PresentRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!g_state.ready || w == 0 || h == 0) return;
    if (!FifoSubmitUpdate(x, y, w, h)) return;
    SyncFifo(g_state.io_base);
}

void Present() {
    if (!g_state.ready) return;
    const uint32_t w = (g_state.dirty && g_state.dirty_x2 > g_state.dirty_x1) ? (g_state.dirty_x2 - g_state.dirty_x1) : 0;
    const uint32_t h = (g_state.dirty && g_state.dirty_y2 > g_state.dirty_y1) ? (g_state.dirty_y2 - g_state.dirty_y1) : 0;
    if (w && h) {
        PresentRect(g_state.dirty_x1, g_state.dirty_y1, w, h);
        g_state.fifo_pending = false;
    } else if (g_state.fifo_pending) {
        SyncFifo(g_state.io_base);
        g_state.fifo_pending = false;
    }
    g_state.dirty = false;
}

} // namespace vmsvga
} // namespace gpu
} // namespace drivers
} // namespace kos
