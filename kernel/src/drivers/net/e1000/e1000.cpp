#include <drivers/net/e1000/e1000.hpp>
#include <drivers/net/e1000/e1000_consts.hpp>
#include <console/logger.hpp>
#include <arch/x86/hardware/pci/peripheral_component_intercontroller.hpp>
#include <arch/x86/hardware/pci/peripheral_component_inter_connect_device_descriptor.hpp>
#include "include/net/nic.hpp"
#include <memory/paging.hpp>
#include <memory/heap.hpp>
#include <lib/stdio.hpp>
#include <lib/string.hpp>

using namespace kos::console;
using namespace kos::arch::x86::hardware::pci;
using namespace kos::drivers::net::e1000; // bring constants into scope
using namespace kos::memory;
using namespace kos::lib;

// Forward declaration of the driver instance (set during Activate)
static E1000Driver* g_e1000_instance = nullptr;

// Forward declare RX submission function
extern "C" void e1000_submit_rx_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len);

E1000Driver::E1000Driver() {}

uint32_t E1000Driver::mmio_read32(uint32_t reg) {
    if (!mmio_base_) return 0;
    volatile uint32_t* addr = reinterpret_cast<volatile uint32_t*>(mmio_base_ + reg);
    return *addr;
}

void E1000Driver::mmio_write32(uint32_t reg, uint32_t val) {
    if (!mmio_base_) return;
    volatile uint32_t* addr = reinterpret_cast<volatile uint32_t*>(mmio_base_ + reg);
    *addr = val;
}

void E1000Driver::read_mac_address() {
    Logger::Log("e1000: Reading MAC address from hardware...");
    
    uint32_t ral = mmio_read32(REG_RAL0);
    uint32_t rah = mmio_read32(REG_RAH0);
    
    char buf[32];
    kos::sys::snprintf(buf, sizeof(buf), "0x%08X", ral);
    Logger::LogKV("e1000.RAL0", buf);
    kos::sys::snprintf(buf, sizeof(buf), "0x%08X", rah);
    Logger::LogKV("e1000.RAH0", buf);
    
    uint8_t mac[6];
    mac[0] = (uint8_t)(ral & 0xFF);
    mac[1] = (uint8_t)((ral >> 8) & 0xFF);
    mac[2] = (uint8_t)((ral >> 16) & 0xFF);
    mac[3] = (uint8_t)((ral >> 24) & 0xFF);
    mac[4] = (uint8_t)(rah & 0xFF);
    mac[5] = (uint8_t)((rah >> 8) & 0xFF);
    
    kos_nic_set_mac(mac);
    
    // Log MAC address
    char mac_str[32];
    kos::sys::snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    Logger::LogKV("e1000.mac", mac_str);
    Logger::Log("e1000: MAC address set in NIC layer");
}

bool E1000Driver::init_hardware() {
    if (!mmio_base_) {
        Logger::Log("e1000: MMIO base not mapped, cannot initialize");
        return false;
    }
    
    // Global reset
    mmio_write32(REG_CTRL, mmio_read32(REG_CTRL) | 0x04000000); // Set RST bit
    for (volatile int i = 0; i < 100000; ++i); // Wait for reset
    
    // Disable interrupts
    mmio_write32(REG_IMC, 0xFFFFFFFF);
    
    // Set up device control - enable link, full duplex
    uint32_t ctrl = mmio_read32(REG_CTRL);
    ctrl |= (1 << 6);  // SLU (Set Link Up)
    ctrl &= ~(1 << 11); // Clear LRST
    mmio_write32(REG_CTRL, ctrl);
    
    // Read MAC address from hardware
    read_mac_address();
    
    // Initialize TX ring
    if (!init_tx_ring()) {
        Logger::Log("e1000: TX ring initialization failed");
        return false;
    }
    
    // Initialize RX ring
    if (!init_rx_ring()) {
        Logger::Log("e1000: RX ring initialization failed");
        return false;
    }
    
    Logger::Log("e1000: hardware initialization complete");
    return true;
}

bool E1000Driver::init_tx_ring() {
    // Allocate TX descriptors (must be 16-byte aligned)
    tx_descs_ = (TxDesc*)Heap::Alloc(sizeof(TxDesc) * TX_DESC_COUNT);
    if (!tx_descs_) return false;
    
    // Allocate TX buffers
    tx_buffers_ = (uint8_t**)Heap::Alloc(sizeof(uint8_t*) * TX_DESC_COUNT);
    if (!tx_buffers_) {
        Heap::Free(tx_descs_);
        return false;
    }
    
    // Initialize TX descriptors
    for (uint32_t i = 0; i < TX_DESC_COUNT; ++i) {
        tx_buffers_[i] = (uint8_t*)Heap::Alloc(TX_BUFFER_SIZE);
        if (!tx_buffers_[i]) {
            // Cleanup on failure
            for (uint32_t j = 0; j < i; ++j) {
                Heap::Free(tx_buffers_[j]);
            }
            Heap::Free(tx_buffers_);
            Heap::Free(tx_descs_);
            return false;
        }
        
        tx_descs_[i].addr = (uint64_t)(uintptr_t)tx_buffers_[i];
        tx_descs_[i].length = 0;
        tx_descs_[i].cso = 0;
        tx_descs_[i].cmd = 0;
        tx_descs_[i].status = 1; // DD bit (descriptor done)
        tx_descs_[i].css = 0;
        tx_descs_[i].special = 0;
    }
    
    tx_head_ = 0;
    tx_tail_ = 0;
    
    // Configure TX registers
    mmio_write32(REG_TDBAL, (uint32_t)(uintptr_t)tx_descs_);
    mmio_write32(REG_TDBAH, 0);
    mmio_write32(REG_TDLEN, TX_DESC_COUNT * sizeof(TxDesc));
    mmio_write32(REG_TDH, 0);
    mmio_write32(REG_TDT, 0);
    
    // Enable TX
    mmio_write32(REG_TCTL, TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (64 << TCTL_COLD_SHIFT));
    mmio_write32(REG_TIPG, 0x00702008);  // IPG values for copper
    
    Logger::Log("e1000: TX ring initialized");
    return true;
}

bool E1000Driver::init_rx_ring() {
    // Allocate RX descriptors (must be 16-byte aligned)
    rx_descs_ = (RxDesc*)Heap::Alloc(sizeof(RxDesc) * RX_DESC_COUNT);
    if (!rx_descs_) return false;
    
    // Allocate RX buffers
    rx_buffers_ = (uint8_t**)Heap::Alloc(sizeof(uint8_t*) * RX_DESC_COUNT);
    if (!rx_buffers_) {
        Heap::Free(rx_descs_);
        return false;
    }
    
    // Initialize RX descriptors
    for (uint32_t i = 0; i < RX_DESC_COUNT; ++i) {
        rx_buffers_[i] = (uint8_t*)Heap::Alloc(RX_BUFFER_SIZE);
        if (!rx_buffers_[i]) {
            // Cleanup on failure
            for (uint32_t j = 0; j < i; ++j) {
                Heap::Free(rx_buffers_[j]);
            }
            Heap::Free(rx_buffers_);
            Heap::Free(rx_descs_);
            return false;
        }
        
        rx_descs_[i].addr = (uint64_t)(uintptr_t)rx_buffers_[i];
        rx_descs_[i].length = 0;
        rx_descs_[i].checksum = 0;
        rx_descs_[i].status = 0;
        rx_descs_[i].errors = 0;
        rx_descs_[i].special = 0;
    }
    
    rx_head_ = 0;
    rx_tail_ = RX_DESC_COUNT - 1;
    
    // Configure RX registers
    mmio_write32(REG_RDBAL, (uint32_t)(uintptr_t)rx_descs_);
    mmio_write32(REG_RDBAH, 0);
    mmio_write32(REG_RDLEN, RX_DESC_COUNT * sizeof(RxDesc));
    mmio_write32(REG_RDH, 0);
    mmio_write32(REG_RDT, RX_DESC_COUNT - 1);
    
    // Enable RX
    mmio_write32(REG_RCTL, RCTL_EN | RCTL_BAM);  // Enable RX, broadcast accept
    
    Logger::Log("e1000: RX ring initialized");
    return true;
}

bool E1000Driver::tx_send(const uint8_t* data, uint32_t len) {
    if (!tx_descs_ || !tx_buffers_ || len > TX_BUFFER_SIZE) {
        return false;
    }
    
    // Get current tail descriptor
    uint32_t tail = tx_tail_;
    TxDesc* desc = &tx_descs_[tail];
    
    // Check if descriptor is free (DD bit set)
    if (!(desc->status & 1)) {
        // Descriptor still in use
        return false;
    }
    
    // Copy data to TX buffer
    for (uint32_t i = 0; i < len; ++i) {
        tx_buffers_[tail][i] = data[i];
    }
    
    // Setup descriptor
    desc->length = (uint16_t)len;
    desc->cso = 0;
    desc->cmd = 0x0B; // EOP | IFCS | RS
    desc->status = 0;  // Clear DD bit
    
    // Update tail pointer
    tx_tail_ = (tail + 1) % TX_DESC_COUNT;
    mmio_write32(REG_TDT, tx_tail_);
    
    return true;
}

void E1000Driver::rx_poll() {
    if (!rx_descs_ || !rx_buffers_) return;
    
    // Check each descriptor from current position
    uint32_t head = rx_head_;
    
    while (true) {
        RxDesc* desc = &rx_descs_[head];
        
        // Check if descriptor has data (DD bit)
        if (!(desc->status & 1)) break;
        
        Logger::Log("e1000: RX packet received!");
        
        // Process received packet
        if (desc->length > 0 && desc->errors == 0) {
            e1000_submit_rx_frame(rx_buffers_[head], desc->length);
        }
        
        // Reset descriptor for reuse
        desc->status = 0;
        desc->errors = 0;
        desc->length = 0;
        
        // Move to next descriptor
        head = (head + 1) % RX_DESC_COUNT;
        
        // Update tail to tell hardware this descriptor is available
        rx_tail_ = (head == 0) ? (RX_DESC_COUNT - 1) : (head - 1);
        mmio_write32(REG_RDT, rx_tail_);
    }
    
    rx_head_ = head;
}

static bool e1000_tx_impl(const kos::common::uint8_t* frame, kos::common::uint32_t len) {
    Logger::Log("e1000_tx_impl: called");
    if (!g_e1000_instance) {
        Logger::Log("e1000_tx_impl: no instance");
        return false;
    }
    
    bool result = g_e1000_instance->tx_send(frame, len);
    if (result) {
        Logger::Log("e1000_tx_impl: sent");
    } else {
        Logger::Log("e1000_tx_impl: failed");
    }
    return result;
}

// Provide a simple RX submission hook the low-level ISR/poll can call
extern "C" void e1000_submit_rx_frame(const kos::common::uint8_t* frame, kos::common::uint32_t len) {
    // Forward to NIC-neutral RX path
    kos_nic_driver_rx(frame, len);
}

// Global function to poll RX from external callers (e.g., timer or main loop)
extern "C" void e1000_rx_poll() {
    if (g_e1000_instance) {
        g_e1000_instance->rx_poll();
    }
}

void E1000Driver::Activate() {
    Logger::Log("e1000: Activate() called");
    
    if (!probe_once()) {
        Logger::Log("e1000: no device found");
        return;
    }
    
    Logger::Log("e1000: device detected");
    log_device();
    
    char buf[32];
    kos::sys::snprintf(buf, sizeof(buf), "0x%08X", io_base_);
    Logger::LogKV("e1000.io_base", buf);
    
    if (io_base_ != 0) {
        // Map MMIO region properly (with cache-disable flag)
        const uint32_t E1000_MMIO_SIZE = 128 * 1024; // 128KB MMIO region
        const uint32_t ID_MAP_END = 64 * 1024 * 1024; // 64MB identity-mapped
        
        if (io_base_ < ID_MAP_END) {
            // Already identity-mapped, use directly
            mmio_base_ = reinterpret_cast<volatile uint8_t*>(io_base_);
            Logger::LogKV("e1000.mmio_base", "identity_mapped");
        } else {
            // Map to virtual address (use address after framebuffer region)
            const uint32_t VIRT_E1000_BASE = 0x11000000u; // 272 MiB
            kos::memory::Paging::MapRange(
                (virt_addr_t)VIRT_E1000_BASE,
                (phys_addr_t)io_base_,
                E1000_MMIO_SIZE,
                kos::memory::Paging::Present | 
                kos::memory::Paging::RW | 
                kos::memory::Paging::CacheDisable
            );
            mmio_base_ = reinterpret_cast<volatile uint8_t*>(VIRT_E1000_BASE);
            Logger::LogKV("e1000.mmio_base", "mapped_virtual");
        }
        
        // Initialize hardware
        if (init_hardware()) {
            // Store instance pointer for TX callback
            g_e1000_instance = this;
            
            // Register TX function
            kos_nic_register_tx(e1000_tx_impl);
            Logger::Log("e1000: TX function registered successfully");
            Logger::Log("e1000: driver activated with TX/RX rings");
        } else {
            Logger::Log("e1000: hardware initialization failed");
            mmio_base_ = nullptr;
        }
    } else {
        Logger::Log("e1000: invalid BAR0 address");
    }
}

bool E1000Driver::probe_once() {
    PeripheralComponentIntercontroller pci;
    for (int bus = 0; bus < 256; ++bus) {
        for (int dev = 0; dev < 32; ++dev) {
            int functions = pci.DeviceHasFunctions(bus, dev) ? 8 : 1;
            for (int fn = 0; fn < functions; ++fn) {
                auto d = pci.GetDeviceDescriptor(bus, dev, fn);
                if (d.vendor_id == PCI_VENDOR_INTEL && d.device_id == PCI_DEVICE_82540EM) {
                    vendor_ = d.vendor_id; device_ = d.device_id; irq_ = (uint8_t)d.interrupt;
                    uint32_t bar0 = pci.Read(bus, dev, fn, PCI_BAR0_OFFSET);
                    io_base_ = (bar0 & PCI_BAR_ADDR_MASK); // handle I/O vs MMIO elsewhere later
                    found_ = true;
                    return true;
                }
            }
        }
    }
    return false;
}

void E1000Driver::log_device() {
    if (!found_) return;
    Logger::LogKV("e1000.vendor", "0x8086");
    Logger::LogKV("e1000.device", "0x100E");
}
