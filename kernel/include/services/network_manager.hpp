#ifndef KOS_SERVICES_NETWORK_MANAGER_HPP
#define KOS_SERVICES_NETWORK_MANAGER_HPP

#include <services/service.hpp>

namespace kos {
    namespace services {

        /*
        * @brief Network configuration structure
        */
        struct NetConfig {
            // IPv4 dotted decimal strings; kept tiny and static-friendly
            char ifname[8];
            char mode[8];     // "dhcp" or "static"
            char ip[16];
            char mask[16];
            char gw[16];
            char dns[16];
        };

        // Minimal Network Manager service
        // NOTE: This is a configuration/orchestration stub. It does not provide a NIC driver
        // or TCP/IP stack yet. It reads desired config and exposes the selected values to logs
        // and optional state files so userland can react. Real networking will require a NIC
        // driver and an IP stack (see README notes added by this service).
        
        /*
        * @brief Network Manager service
        *  Implements basic network configuration management.
        */
        class NetworkManagerService : public IService {
        public:
            
            /*
            * @brief Get the name of the service
            * @return The name of the service
            * Unique short name (8.3 friendly) used in config, e.g. "NETWORK"
            */
            virtual const char* Name() const override { return "NETWORK"; }
            
            /*
            * @brief Start the service
            * @return True if the service started successfully, false otherwise
            * Sets up network configuration based on predefined settings.  
            */
            virtual bool Start() override;
            
            /*
            * @brief Tick the service
            * @return True if the service ticked successfully, false otherwise
            * Optional periodic work. Override Tick() if the service needs background work.
            */
            virtual void Tick() override {}
            
            /*
            * @brief Get the tick interval for the service
            * @return The tick interval in milliseconds
            * Desired tick interval in milliseconds for periodic work (if Tick is overridden).
            */
            virtual uint32_t TickIntervalMs() const override { return 0; }

            /*
            * @brief Get the default enabled state for the service
            * @return True if the service is enabled by default, false otherwise    
            * Default enabled state if not configured.
            */
            virtual bool DefaultEnabled() const override { return false; }

            /*
            * @brief Get the current network configuration
            * @return The current network configuration
            * Provides access to the current network configuration.
            */
            const NetConfig& Current() const { return cfg_; }

        private:

            /*
            * @brief Load network configuration from persistent storage
            * @return True if the configuration was loaded successfully, false otherwise
            * Loads network configuration from persistent storage.
            */
            bool loadConfig();

            /*
            * @brief Try to configure DHCP
            * @return True if DHCP configuration was successful, false otherwise
            * Attempts to configure the network interface using DHCP.
            */
            bool tryDhcpStub();

            /*
            * @brief Write network state files
            * Writes the current network configuration to state files.
            */
            void writeStateFiles();

            /*
            * @brief Log the current network configuration
            * @param prefix The prefix to use for log messages
            * Logs the current network configuration to the system log.
            */
            void logConfig(const char* prefix);

            /*
            * @brief The current network configuration
            * Provides access to the current network configuration.
            */
            NetConfig cfg_{};
            
            // Optional worker thread so the service is visible in 'top'
            /*
            * @brief The thread ID for the worker thread
            * @return The thread ID for the worker thread
            * The thread ID for the worker thread.
            */
            unsigned int threadId_ = 0;
            
            /*
            * @brief The worker thread function
            * The worker thread function for handling network tasks.
            */
            static void worker_trampoline();

            /*
            * @brief The instance of the network manager service
            * The instance of the network manager service.
            */
            static NetworkManagerService* s_self; // for simple trampoline capture
        };

    } // namespace services
} // namespace kos

#endif // KOS_SERVICES_NETWORK_MANAGER_HPP
