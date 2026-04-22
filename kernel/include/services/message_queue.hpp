#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <memory>

namespace kos::services {

/**
 * @brief Inter-Process Communication via Message Queue
 * 
 * Messages between WindowManager and applications.
 * Uses POSIX message queues (/dev/mqueue on Linux).
 * 
 * Message Types:
 * - WINDOW_CREATE: Request to create a window
 * - WINDOW_CLOSE: Notify window closure
 * - WINDOW_RENDER: Render request
 * - WINDOW_INPUT: Input event (mouse, keyboard)
 * - WINDOW_RESIZE: Window resize notification
 * - APP_START: Application start request
 * - APP_STOP: Application stop request
 * - APP_READY: Application ready notification
 */

struct Message {
    enum class Type : uint8_t {
        WINDOW_CREATE = 1,
        WINDOW_CLOSE = 2,
        WINDOW_RENDER = 3,
        WINDOW_INPUT = 4,
        WINDOW_RESIZE = 5,
        APP_START = 6,
        APP_STOP = 7,
        APP_READY = 8,
        CUSTOM = 255
    };
    
    Type type;
    uint32_t sender_pid;      // PID of sender
    uint32_t receiver_pid;    // PID of receiver (0 = broadcast)
    uint32_t message_id;      // Unique message ID for tracking
    uint32_t window_id;       // Associated window (if applicable)
    uint32_t payload_size;    // Size of payload data
    uint8_t payload[512];     // Message data
    
    // Helper constructors
    Message() : type(Type::CUSTOM), sender_pid(0), receiver_pid(0), 
                message_id(0), window_id(0), payload_size(0) {}
    
    explicit Message(Type t) : type(t), sender_pid(0), receiver_pid(0),
                               message_id(0), window_id(0), payload_size(0) {}
};

/**
 * @brief Message Queue for inter-process communication
 */
class MessageQueue {
public:
    /**
     * @brief Get or create queue for a process
     * @param process_name Name of the process (used to create queue name)
     * @return True if queue created/opened successfully
     */
    static bool Initialize(const std::string& process_name);
    
    /**
     * @brief Get the global message queue instance
     */
    static MessageQueue& Instance();
    
    /**
     * @brief Send message to another process
     * @param receiver_pid PID of receiving process
     * @param message Message to send
     * @return True if sent successfully
     */
    bool Send(uint32_t receiver_pid, const Message& message);
    
    /**
     * @brief Broadcast message to all processes
     * @param message Message to broadcast
     */
    void Broadcast(const Message& message);
    
    /**
     * @brief Check if message is available (non-blocking)
     */
    bool HasMessage();
    
    /**
     * @brief Receive next message (blocking with timeout)
     * @param timeout_ms Timeout in milliseconds (-1 = infinite)
     * @return Message if available, empty message otherwise
     */
    Message Receive(int timeout_ms = -1);
    
    /**
     * @brief Receive all pending messages
     */
    std::vector<Message> ReceiveAll();
    
    /**
     * @brief Reply to a message
     * @param original_message The message to reply to
     * @param reply Reply message
     */
    bool Reply(const Message& original_message, const Message& reply);
    
    /**
     * @brief Get process name/PID mapping
     */
    uint32_t GetPidByName(const std::string& process_name) const;
    
    /**
     * @brief Register process name
     */
    void RegisterProcess(uint32_t pid, const std::string& name);

private:
    MessageQueue() = default;
    
    static std::unique_ptr<MessageQueue> instance_;
    static std::mutex instance_mutex_;
    
    std::string process_name_;
    uint32_t my_pid_ = 0;
    int queue_fd_ = -1;
    uint32_t next_message_id_ = 1;
    
    // Process registry (name -> PID)
    static std::unordered_map<std::string, uint32_t> process_registry_;
    static std::mutex registry_mutex_;
};

/**
 * @brief Helper for WindowManager <-> Application communication
 */
class ApplicationLauncher {
public:
    /**
     * @brief Launch an application from desktop entry
     * @param entry Desktop entry with exec path
     * @return PID of launched process
     */
    static uint32_t Launch(const DesktopEntry& entry);
    
    /**
     * @brief Launch multiple applications
     */
    static std::vector<uint32_t> LaunchMultiple(const std::vector<DesktopEntry>& entries);
    
    /**
     * @brief Terminate application
     * @param pid Process ID to terminate
     */
    static void Terminate(uint32_t pid);
    
    /**
     * @brief Wait for application to be ready
     * @param pid Process ID
     * @param timeout_ms Timeout in milliseconds
     * @return True if APP_READY message received
     */
    static bool WaitReady(uint32_t pid, int timeout_ms = 5000);
};

} // namespace kos::services
