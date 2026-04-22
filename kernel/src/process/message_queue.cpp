#include <process/message_queue.hpp>
#include <process/scheduler.hpp>
#include <process/thread_manager.hpp>
#include <memory/heap.hpp>
#include <lib/string.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>

using namespace kos::process;
using namespace kos::memory;
using namespace kos::lib;
using namespace kos::console;

MessageQueueManager* kos::process::g_message_queue_manager = nullptr;

namespace {
    bool NamesEqual(const char* a, const char* b) {
        if (!a || !b) return false;
        uint32_t len_a = strlen(a);
        uint32_t len_b = strlen(b);
        if (len_a != len_b) return false;
        return String::strcmp((const int8_t*)a, (const int8_t*)b, len_a) == 0;
    }

    uint32_t CurrentEndpointId() {
        if (!g_scheduler) return 0;
        Thread* current = g_scheduler->GetCurrentTask();
        if (!current) return 0;

        if (g_thread_manager) {
            uint32_t pid = g_thread_manager->GetPid(current->task_id);
            if (pid != 0) return pid;
        }

        return current->task_id;
    }
}

QueueMessage::QueueMessage(uint32_t id, uint32_t sender, uint32_t receiver,
                           MessageType msg_type, uint32_t size, const void* payload)
    : message_id(id), sender_id(sender), receiver_id(receiver), type(msg_type),
      data_size(size), data(nullptr), next(nullptr) {

    if (size > 0 && payload) {
        data = (uint8_t*)Heap::Alloc(size);
        if (data) {
            memcpy(data, payload, size);
        } else {
            data_size = 0;
        }
    }
}

QueueMessage::~QueueMessage() {
    if (data) {
        Heap::Free(data);
        data = nullptr;
    }
}

MessageQueue::MessageQueue(uint32_t id, const char* queue_name,
                           uint32_t max_msgs, uint32_t max_msg_size)
    : queue_id(id), max_messages(max_msgs), max_message_size(max_msg_size),
      message_count(0), message_head(nullptr), message_tail(nullptr), next_message_id(1),
      is_closed(false), blocking_mode(true) {

    int name_len = strlen(queue_name ? queue_name : "");
    if (name_len >= (int)sizeof(name)) name_len = sizeof(name) - 1;
    for (int i = 0; i < name_len; ++i) {
        name[i] = queue_name[i];
    }
    name[name_len] = '\0';

    queue_mutex = new Mutex();
    read_cv = new ConditionVariable();
    write_cv = new ConditionVariable();
}

MessageQueue::~MessageQueue() {
    Close();
    Flush();

    if (queue_mutex) delete queue_mutex;
    if (read_cv) delete read_cv;
    if (write_cv) delete write_cv;
}

QueueMessage* MessageQueue::FindMatchingMessage(uint32_t receiver_id, QueueMessage** prev_out) const {
    QueueMessage* prev = nullptr;
    QueueMessage* msg = message_head;

    while (msg) {
        if (receiver_id == 0 || msg->receiver_id == 0 || msg->receiver_id == receiver_id) {
            if (prev_out) *prev_out = prev;
            return msg;
        }
        prev = msg;
        msg = msg->next;
    }

    if (prev_out) *prev_out = nullptr;
    return nullptr;
}

bool MessageQueue::Send(uint32_t sender_id, uint32_t receiver_id, MessageType type,
                        const void* data, uint32_t size, bool block) {
    if (is_closed) return false;
    if (size > 0 && !data) return false;
    if (size > max_message_size) return false;

    LockGuard lock(*queue_mutex);

    while (IsFull()) {
        if (!block || is_closed) return false;
        write_cv->Wait(*queue_mutex);
    }

    QueueMessage* msg = new QueueMessage(next_message_id++, sender_id, receiver_id, type, size, data);
    if (!msg || (size > 0 && !msg->data)) {
        delete msg;
        return false;
    }

    if (!message_head) {
        message_head = message_tail = msg;
    } else {
        message_tail->next = msg;
        message_tail = msg;
    }

    message_count++;
    read_cv->Broadcast();
    return true;
}

bool MessageQueue::Receive(uint32_t receiver_id, MessageType* out_type,
                           void* buffer, uint32_t buffer_size, uint32_t* bytes_read,
                           uint32_t* out_sender_id, uint32_t* out_message_id,
                           bool block) {
    if (is_closed) return false;
    if (!buffer || buffer_size == 0) return false;

    LockGuard lock(*queue_mutex);

    QueueMessage* prev = nullptr;
    QueueMessage* msg = FindMatchingMessage(receiver_id, &prev);

    while (!msg) {
        if (!block || is_closed) {
            if (bytes_read) *bytes_read = 0;
            return false;
        }
        read_cv->Wait(*queue_mutex);
        msg = FindMatchingMessage(receiver_id, &prev);
    }

    uint32_t copy_size = (msg->data_size < buffer_size) ? msg->data_size : buffer_size;
    if (copy_size > 0 && msg->data) {
        memcpy(buffer, msg->data, copy_size);
    }

    if (bytes_read) *bytes_read = copy_size;
    if (out_type) *out_type = msg->type;
    if (out_sender_id) *out_sender_id = msg->sender_id;
    if (out_message_id) *out_message_id = msg->message_id;

    if (prev) {
        prev->next = msg->next;
    } else {
        message_head = msg->next;
    }
    if (message_tail == msg) {
        message_tail = prev;
    }

    message_count--;
    delete msg;

    write_cv->Signal();
    return true;
}

bool MessageQueue::Peek(uint32_t receiver_id, MessageType* out_type,
                        void* buffer, uint32_t buffer_size, uint32_t* bytes_available,
                        uint32_t* out_sender_id, uint32_t* out_message_id) const {
    if (is_closed) return false;
    if (!buffer || buffer_size == 0) return false;

    LockGuard lock(*queue_mutex);

    QueueMessage* msg = FindMatchingMessage(receiver_id, nullptr);
    if (!msg) {
        if (bytes_available) *bytes_available = 0;
        return false;
    }

    uint32_t copy_size = (msg->data_size < buffer_size) ? msg->data_size : buffer_size;
    if (copy_size > 0 && msg->data) {
        memcpy(buffer, msg->data, copy_size);
    }

    if (bytes_available) *bytes_available = copy_size;
    if (out_type) *out_type = msg->type;
    if (out_sender_id) *out_sender_id = msg->sender_id;
    if (out_message_id) *out_message_id = msg->message_id;

    return true;
}

void MessageQueue::Close() {
    LockGuard lock(*queue_mutex);
    is_closed = true;
    read_cv->Broadcast();
    write_cv->Broadcast();
}

void MessageQueue::Flush() {
    LockGuard lock(*queue_mutex);

    while (message_head) {
        QueueMessage* msg = message_head;
        message_head = message_head->next;
        delete msg;
    }

    message_tail = nullptr;
    message_count = 0;
    write_cv->Broadcast();
}

void MessageQueue::PrintInfo() const {
    TTY::Write("MQ ID: ");
    TTY::WriteHex(queue_id);
    TTY::Write(" Name: ");
    TTY::Write(name);
    TTY::Write(" Msgs: ");
    TTY::WriteHex(message_count);
    TTY::Write("/");
    TTY::WriteHex(max_messages);
    TTY::Write(" MaxMsgSize: ");
    TTY::WriteHex(max_message_size);
    TTY::Write(is_closed ? " [CLOSED]" : " [OPEN]");
    TTY::Write(blocking_mode ? " [BLOCKING]" : " [NON-BLOCKING]");
    TTY::Write("\n");
}

MessageQueueManager::MessageQueueManager() : queue_count(0), next_queue_id(1) {
    for (int i = 0; i < 64; ++i) {
        queues[i] = nullptr;
    }
    manager_mutex = new Mutex();
    Logger::Log("Message queue manager initialized");
}

MessageQueueManager::~MessageQueueManager() {
    CloseAllQueues();
    if (manager_mutex) delete manager_mutex;
}

MessageQueue* MessageQueueManager::CreateQueue(const char* name, uint32_t max_messages,
                                               uint32_t max_message_size) {
    if (!name || queue_count >= 64 || max_messages == 0 || max_message_size == 0) return nullptr;

    LockGuard lock(*manager_mutex);

    if (FindQueueIndex(name) >= 0) {
        return nullptr;
    }

    int slot = -1;
    for (int i = 0; i < 64; ++i) {
        if (!queues[i]) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return nullptr;

    MessageQueue* queue = new MessageQueue(next_queue_id++, name, max_messages, max_message_size);
    if (!queue) return nullptr;

    queues[slot] = queue;
    queue_count++;
    if (Logger::IsDebugEnabled()) {
        Logger::Log("Created message queue");
    }
    return queue;
}

bool MessageQueueManager::DestroyQueue(uint32_t queue_id) {
    LockGuard lock(*manager_mutex);

    int index = FindQueueIndex(queue_id);
    if (index < 0) return false;

    delete queues[index];
    queues[index] = nullptr;
    queue_count--;
    Logger::Log("Destroyed message queue");
    return true;
}

bool MessageQueueManager::DestroyQueue(const char* name) {
    if (!name) return false;

    LockGuard lock(*manager_mutex);

    int index = FindQueueIndex(name);
    if (index < 0) return false;

    delete queues[index];
    queues[index] = nullptr;
    queue_count--;
    Logger::Log("Destroyed message queue");
    return true;
}

MessageQueue* MessageQueueManager::FindQueue(uint32_t queue_id) const {
    int index = FindQueueIndex(queue_id);
    return (index >= 0) ? queues[index] : nullptr;
}

MessageQueue* MessageQueueManager::FindQueue(const char* name) const {
    if (!name) return nullptr;

    int index = FindQueueIndex(name);
    return (index >= 0) ? queues[index] : nullptr;
}

int MessageQueueManager::FindQueueIndex(uint32_t queue_id) const {
    for (int i = 0; i < 64; ++i) {
        if (queues[i] && queues[i]->GetId() == queue_id) {
            return i;
        }
    }
    return -1;
}

int MessageQueueManager::FindQueueIndex(const char* name) const {
    if (!name) return -1;

    for (int i = 0; i < 64; ++i) {
        if (queues[i] && NamesEqual(queues[i]->GetName(), name)) {
            return i;
        }
    }
    return -1;
}

void MessageQueueManager::CloseAllQueues() {
    LockGuard lock(*manager_mutex);

    for (int i = 0; i < 64; ++i) {
        if (queues[i]) {
            delete queues[i];
            queues[i] = nullptr;
        }
    }
    queue_count = 0;
}

void MessageQueueManager::PrintAllQueues() const {
    LockGuard lock(*manager_mutex);

    TTY::Write("=== Message Queue Manager ===\n");
    TTY::Write("Active queues: ");
    TTY::WriteHex(queue_count);
    TTY::Write("/64\n");

    for (int i = 0; i < 64; ++i) {
        if (queues[i]) {
            queues[i]->PrintInfo();
        }
    }
}

namespace kos::process::MessageQueueAPI {

    uint32_t CreateQueue(const char* name, uint32_t max_messages, uint32_t max_message_size) {
        if (!g_message_queue_manager) return 0;

        MessageQueue* queue = g_message_queue_manager->CreateQueue(name, max_messages, max_message_size);
        return queue ? queue->GetId() : 0;
    }

    bool DestroyQueue(uint32_t queue_id) {
        if (!g_message_queue_manager) return false;
        return g_message_queue_manager->DestroyQueue(queue_id);
    }

    bool DestroyQueue(const char* name) {
        if (!g_message_queue_manager) return false;
        return g_message_queue_manager->DestroyQueue(name);
    }

    bool Send(uint32_t queue_id, uint32_t receiver_id, MessageType type,
              const void* data, uint32_t size, bool block) {
        if (!g_message_queue_manager) return false;

        MessageQueue* queue = g_message_queue_manager->FindQueue(queue_id);
        if (!queue) return false;

        return queue->Send(CurrentEndpointId(), receiver_id, type, data, size, block);
    }

    bool Send(const char* name, uint32_t receiver_id, MessageType type,
              const void* data, uint32_t size, bool block) {
        if (!g_message_queue_manager) return false;

        MessageQueue* queue = g_message_queue_manager->FindQueue(name);
        if (!queue) return false;

        return queue->Send(CurrentEndpointId(), receiver_id, type, data, size, block);
    }

    bool Receive(uint32_t queue_id, MessageType* out_type,
                 void* buffer, uint32_t buffer_size, uint32_t* bytes_read,
                 uint32_t* out_sender_id, uint32_t* out_message_id,
                 bool block) {
        if (!g_message_queue_manager) return false;

        MessageQueue* queue = g_message_queue_manager->FindQueue(queue_id);
        if (!queue) return false;

        return queue->Receive(CurrentEndpointId(), out_type, buffer, buffer_size,
                              bytes_read, out_sender_id, out_message_id, block);
    }

    bool Receive(const char* name, MessageType* out_type,
                 void* buffer, uint32_t buffer_size, uint32_t* bytes_read,
                 uint32_t* out_sender_id, uint32_t* out_message_id,
                 bool block) {
        if (!g_message_queue_manager) return false;

        MessageQueue* queue = g_message_queue_manager->FindQueue(name);
        if (!queue) return false;

        return queue->Receive(CurrentEndpointId(), out_type, buffer, buffer_size,
                              bytes_read, out_sender_id, out_message_id, block);
    }

    uint32_t FindQueue(const char* name) {
        if (!g_message_queue_manager) return 0;

        MessageQueue* queue = g_message_queue_manager->FindQueue(name);
        return queue ? queue->GetId() : 0;
    }

    bool GetQueueInfo(uint32_t queue_id, uint32_t* message_count,
                      uint32_t* max_messages, uint32_t* max_message_size) {
        if (!g_message_queue_manager) return false;

        MessageQueue* queue = g_message_queue_manager->FindQueue(queue_id);
        if (!queue) return false;

        if (message_count) *message_count = queue->GetMessageCount();
        if (max_messages) *max_messages = queue->GetMaxMessages();
        if (max_message_size) *max_message_size = queue->GetMaxMessageSize();
        return true;
    }
}
