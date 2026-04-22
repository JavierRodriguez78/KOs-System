#include "../../include/services/message_queue.hpp"
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>

namespace kos::services {

// ============================================================================
// MessageQueue Implementation
// ============================================================================

std::unique_ptr<MessageQueue> MessageQueue::instance_;
std::mutex MessageQueue::instance_mutex_;
std::unordered_map<std::string, uint32_t> MessageQueue::process_registry_;
std::mutex MessageQueue::registry_mutex_;

MessageQueue& MessageQueue::Instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::make_unique<MessageQueue>();
    }
    return *instance_;
}

bool MessageQueue::Initialize(const std::string& process_name) {
    MessageQueue& mq = Instance();
    mq.process_name_ = process_name;
    mq.my_pid_ = getpid();
    
    // Create message queue name from process name
    std::string queue_name = "/mq_" + process_name;
    
    // Remove old queue if exists
    mq_unlink(queue_name.c_str());
    
    // Create new queue
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 32;              // Max 32 messages
    attr.mq_msgsize = sizeof(Message); // Size per message
    attr.mq_curmsgs = 0;
    
    mq.queue_fd_ = mq_open(queue_name.c_str(), 
                           O_CREAT | O_RDWR | O_NONBLOCK, 
                           S_IRUSR | S_IWUSR, 
                           &attr);
    
    if (mq.queue_fd_ == -1) {
        std::cerr << "Failed to create message queue: " << process_name << std::endl;
        return false;
    }
    
    // Register process
    {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        process_registry_[process_name] = mq.my_pid_;
    }
    
    return true;
}

bool MessageQueue::Send(uint32_t receiver_pid, const Message& message) {
    // For now, simple approach: send to queue with receiver_pid as key
    // In production, would use actual IPC mechanism
    
    Message msg = message;
    msg.sender_pid = my_pid_;
    msg.receiver_pid = receiver_pid;
    
    if (msg.message_id == 0) {
        msg.message_id = next_message_id_++;
    }
    
    if (queue_fd_ == -1) {
        std::cerr << "Message queue not initialized" << std::endl;
        return false;
    }
    
    // Try to send. In real implementation, would route to specific process queue
    int result = mq_send(queue_fd_, (const char*)&msg, sizeof(Message), 1);
    
    return result == 0;
}

void MessageQueue::Broadcast(const Message& message) {
    // Would iterate through all registered processes
    // For MVP, just send to own queue
    Message msg = message;
    msg.receiver_pid = 0;  // 0 = broadcast
    Send(0, msg);
}

bool MessageQueue::HasMessage() {
    if (queue_fd_ == -1) return false;
    
    struct mq_attr attr;
    if (mq_getattr(queue_fd_, &attr) == -1) {
        return false;
    }
    
    return attr.mq_curmsgs > 0;
}

Message MessageQueue::Receive(int timeout_ms) {
    Message msg;
    memset(&msg, 0, sizeof(msg));
    
    if (queue_fd_ == -1) {
        return msg;
    }
    
    // For blocking receive, need to change queue to blocking mode
    struct mq_attr attr, old_attr;
    attr.mq_flags = 0;  // Blocking mode
    
    if (timeout_ms > 0) {
        attr.mq_flags = O_NONBLOCK;
    }
    
    mq_setattr(queue_fd_, &attr, &old_attr);
    
    unsigned int prio = 0;
    ssize_t received = mq_receive(queue_fd_, (char*)&msg, sizeof(Message), &prio);
    
    // Restore original attributes
    mq_setattr(queue_fd_, &old_attr, nullptr);
    
    if (received == -1) {
        return Message();  // Empty message on error
    }
    
    return msg;
}

std::vector<Message> MessageQueue::ReceiveAll() {
    std::vector<Message> messages;
    
    while (HasMessage()) {
        Message msg = Receive(0);
        if (msg.type != Message::Type::CUSTOM || msg.payload_size > 0) {
            messages.push_back(msg);
        }
    }
    
    return messages;
}

bool MessageQueue::Reply(const Message& original_message, const Message& reply) {
    Message response = reply;
    response.sender_pid = my_pid_;
    response.receiver_pid = original_message.sender_pid;
    response.message_id = original_message.message_id;
    
    return Send(response.receiver_pid, response);
}

uint32_t MessageQueue::GetPidByName(const std::string& process_name) const {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = process_registry_.find(process_name);
    if (it != process_registry_.end()) {
        return it->second;
    }
    return 0;
}

void MessageQueue::RegisterProcess(uint32_t pid, const std::string& name) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    process_registry_[name] = pid;
}

// ============================================================================
// ApplicationLauncher Implementation
// ============================================================================

uint32_t ApplicationLauncher::Launch(const DesktopEntry& entry) {
    if (entry.exec.empty()) {
        return 0;
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        std::cerr << "Failed to fork process" << std::endl;
        return 0;
    }
    
    if (pid == 0) {
        // Child process
        // Parse exec and arguments
        std::vector<char*> args;
        std::string exec_copy = entry.exec;
        
        char* token = strtok((char*)exec_copy.c_str(), " ");
        while (token != nullptr) {
            args.push_back(token);
            token = strtok(nullptr, " ");
        }
        args.push_back(nullptr);  // NULL terminator for execvp
        
        // Initialize message queue for this app
        MessageQueue::Initialize(entry.name);
        
        // Execute
        execvp(args[0], args.data());
        
        // If execvp returns, error occurred
        std::cerr << "Failed to execute: " << entry.exec << std::endl;
        exit(1);
    }
    
    // Parent process
    MessageQueue::RegisterProcess(pid, entry.name);
    return pid;
}

std::vector<uint32_t> ApplicationLauncher::LaunchMultiple(const std::vector<DesktopEntry>& entries) {
    std::vector<uint32_t> pids;
    for (const auto& entry : entries) {
        uint32_t pid = Launch(entry);
        if (pid != 0) {
            pids.push_back(pid);
        }
    }
    return pids;
}

void ApplicationLauncher::Terminate(uint32_t pid) {
    kill(pid, SIGTERM);
    // Give process time to cleanup
    sleep(1);
    // Force kill if still running
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, WNOHANG);
}

bool ApplicationLauncher::WaitReady(uint32_t pid, int timeout_ms) {
    // Wait for APP_READY message from application
    // This is a simplified implementation
    // In production, would use actual message queue waiting
    
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (true) {
        // Check if process still exists
        if (kill(pid, 0) == -1) {
            return false;  // Process died
        }
        
        clock_gettime(CLOCK_MONOTONIC, &now);
        int elapsed = (now.tv_sec - start.tv_sec) * 1000 + 
                      (now.tv_nsec - start.tv_nsec) / 1000000;
        
        if (elapsed > timeout_ms) {
            return false;  // Timeout
        }
        
        usleep(100000);  // Check every 100ms
    }
    
    return true;
}

} // namespace kos::services
