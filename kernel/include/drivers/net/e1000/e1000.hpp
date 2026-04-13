#pragma once

#include <drivers/driver.hpp>
#include <common/types.hpp>

using namespace kos::common;

namespace kos { 
    namespace drivers { 
        namespace net {
            namespace e1000 {

                /*
                *@brief E1000 network driver class.
                *This class implements the E1000 network driver for Intel 8254x/8255x
                *Ethernet controllers.
                */
                class E1000Driver : public Driver {
                    public:

                        /**
                         * @brief Construct a new E1000Driver object
                         */
                        E1000Driver();
                        
                        /*
                        * @brief Activate the E1000 network driver.
                        * This method initializes the driver and prepares it for operation.
                        */
                        virtual void Activate() override;
                        
                        /*
                        * @brief Reset the E1000 network driver.
                        * @return int   0 on success, negative error code on failure.
                        * This method resets the driver and prepares it for a new operation.
                        */
                        virtual int Reset() override { return 0; }

                        /*
                        * @brief Deactivate the E1000 network driver.
                        * This method deactivates the driver and releases any resources it holds.
                        */
                        virtual void Deactivate() override {}
                        
                        /*
                        * @brief Send a packet using TX ring
                        */
                        bool tx_send(const uint8_t* data, uint32_t len);
                        
                        /*
                        * @brief Poll for received packets
                        */
                        void rx_poll();

                        /*
                        * @brief Emit one-shot RX-path hardware register snapshot to logger
                        */
                        void snapshot_rx_registers();

                    private:
                        /*
                        * @brief Probe the PCI bus for the E1000 device.
                        * @return true if the device is found, false otherwise.
                        * This method scans the PCI bus to locate the E1000 network device.
                        */
                        bool probe_once();
                        
                        /*
                        * @brief Initialize the E1000 hardware
                        * @return true if initialization succeeded, false otherwise
                        */
                        bool init_hardware();

                        /*
                        * @brief Enable PCI memory and bus-mastering for DMA
                        */
                        bool enable_pci_bus_mastering();
                        
                        /*
                        * @brief Read MAC address from device
                        */
                        void read_mac_address();
                        
                        /*
                        * @brief Log the E1000 device information.
                        * This method logs relevant information about the detected E1000 device.
                        */  
                        void log_device();
                        
                        /*
                        * @brief Read 32-bit value from MMIO register
                        */
                        uint32_t mmio_read32(uint32_t reg);
                        
                        /*
                        * @brief Write 32-bit value to MMIO register
                        */
                        void mmio_write32(uint32_t reg, uint32_t val);
                        
                        /*
                         * @brief Initialize TX descriptor ring
                         */
                        bool init_tx_ring();
                        
                        /*
                         * @brief Initialize RX descriptor ring
                         */
                        bool init_rx_ring();

                        /*
                        * @brief E1000 vendor ID.
                        */
                        uint16_t vendor_{0};
                        
                        /*
                        * @brief E1000 device ID.
                        * @var device_id
                        */
                        uint16_t device_{0};
                        uint16_t bus_{0};
                        uint16_t dev_{0};
                        uint16_t fn_{0};
                        
                        /*
                        * @brief E1000 device I/O base address.
                        * @var io_base
                        */
                        uint32_t io_base_{0};
                        bool bar0_is_mmio_{true};
                        
                        /*
                        * @brief E1000 MMIO base address (virtual)
                        * @var mmio_base
                        */
                        volatile uint8_t* mmio_base_{nullptr};
                        
                        /*
                        * @brief E1000 device IRQ line.
                        * @var irq
                        */
                        uint8_t irq_{0};
                        
                        /*
                        * @brief E1000 device found status.
                        * @var found
                        */  
                        bool found_{false};
                        
                        // TX descriptor ring structures
                        struct TxDesc {
                            uint64_t addr;
                            uint16_t length;
                            uint8_t cso;
                            uint8_t cmd;
                            uint8_t status;
                            uint8_t css;
                            uint16_t special;
                        } __attribute__((packed));
                        
                        // RX descriptor ring structures
                        struct RxDesc {
                            uint64_t addr;
                            uint16_t length;
                            uint16_t checksum;
                            uint8_t status;
                            uint8_t errors;
                            uint16_t special;
                        } __attribute__((packed));
                        
                        TxDesc* tx_descs_{nullptr};
                        uint8_t** tx_buffers_{nullptr};
                        uint32_t tx_head_{0};
                        uint32_t tx_tail_{0};
                        
                        RxDesc* rx_descs_{nullptr};
                        uint8_t** rx_buffers_{nullptr};
                        uint32_t rx_head_{0};
                        uint32_t rx_tail_{0};
                    };
                }
            }
        }
    }
