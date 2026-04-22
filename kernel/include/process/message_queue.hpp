#ifndef __KOS__PROCESS__MESSAGE_QUEUE_H
#define __KOS__PROCESS__MESSAGE_QUEUE_H

#include <common/types.hpp>
#include <process/sync.hpp>

using namespace kos::common;

namespace kos {
    namespace process {

        enum MessageType : uint32_t {
            MSG_TYPE_GENERIC = 0,
            MSG_TYPE_COMMAND = 1,
            MSG_TYPE_EVENT = 2,
            MSG_TYPE_RESPONSE = 3,
            MSG_TYPE_SYSTEM = 4,
        };

        struct QueueMessage {
            uint32_t message_id;
            uint32_t sender_id;
            uint32_t receiver_id;    // 0 means broadcast/any receiver
            MessageType type;
            uint32_t data_size;
            uint8_t* data;
            QueueMessage* next;

            QueueMessage(uint32_t id, uint32_t sender, uint32_t receiver,
                         MessageType msg_type, uint32_t size, const void* payload);
            ~QueueMessage();
        };

        class MessageQueue {
        private:
            uint32_t queue_id;
            char name[32];
            uint32_t max_messages;
            uint32_t max_message_size;
            uint32_t message_count;

            QueueMessage* message_head;
            QueueMessage* message_tail;
            uint32_t next_message_id;

            Mutex* queue_mutex;
            ConditionVariable* read_cv;
            ConditionVariable* write_cv;

            bool is_closed;
            bool blocking_mode;

            QueueMessage* FindMatchingMessage(uint32_t receiver_id, QueueMessage** prev_out) const;

        public:
            MessageQueue(uint32_t id, const char* queue_name,
                         uint32_t max_msgs = 64, uint32_t max_msg_size = 256);
            ~MessageQueue();

            bool Send(uint32_t sender_id, uint32_t receiver_id, MessageType type,
                      const void* data, uint32_t size, bool block = true);
            bool Receive(uint32_t receiver_id, MessageType* out_type,
                         void* buffer, uint32_t buffer_size, uint32_t* bytes_read,
                         uint32_t* out_sender_id = nullptr,
                         uint32_t* out_message_id = nullptr,
                         bool block = true);
            bool Peek(uint32_t receiver_id, MessageType* out_type,
                      void* buffer, uint32_t buffer_size, uint32_t* bytes_available,
                      uint32_t* out_sender_id = nullptr,
                      uint32_t* out_message_id = nullptr) const;

            void Close();
            void Flush();
            void SetBlockingMode(bool blocking) { blocking_mode = blocking; }

            uint32_t GetId() const { return queue_id; }
            const char* GetName() const { return name; }
            uint32_t GetMessageCount() const { return message_count; }
            uint32_t GetMaxMessages() const { return max_messages; }
            uint32_t GetMaxMessageSize() const { return max_message_size; }
            bool IsEmpty() const { return message_count == 0; }
            bool IsFull() const { return message_count >= max_messages; }
            bool IsClosed() const { return is_closed; }
            bool IsBlocking() const { return blocking_mode; }

            void PrintInfo() const;
        };

        class MessageQueueManager {
        private:
            MessageQueue* queues[64];
            uint32_t queue_count;
            uint32_t next_queue_id;
            Mutex* manager_mutex;

            int FindQueueIndex(uint32_t queue_id) const;
            int FindQueueIndex(const char* name) const;

        public:
            MessageQueueManager();
            ~MessageQueueManager();

            MessageQueue* CreateQueue(const char* name, uint32_t max_messages = 64,
                                      uint32_t max_message_size = 256);
            bool DestroyQueue(uint32_t queue_id);
            bool DestroyQueue(const char* name);

            MessageQueue* FindQueue(uint32_t queue_id) const;
            MessageQueue* FindQueue(const char* name) const;

            void CloseAllQueues();
            void PrintAllQueues() const;
            uint32_t GetQueueCount() const { return queue_count; }
        };

        extern MessageQueueManager* g_message_queue_manager;

        namespace MessageQueueAPI {
            uint32_t CreateQueue(const char* name, uint32_t max_messages = 64,
                                 uint32_t max_message_size = 256);
            bool DestroyQueue(uint32_t queue_id);
            bool DestroyQueue(const char* name);

            bool Send(uint32_t queue_id, uint32_t receiver_id, MessageType type,
                      const void* data, uint32_t size, bool block = true);
            bool Send(const char* name, uint32_t receiver_id, MessageType type,
                      const void* data, uint32_t size, bool block = true);

            bool Receive(uint32_t queue_id, MessageType* out_type,
                         void* buffer, uint32_t buffer_size, uint32_t* bytes_read,
                         uint32_t* out_sender_id = nullptr,
                         uint32_t* out_message_id = nullptr,
                         bool block = true);
            bool Receive(const char* name, MessageType* out_type,
                         void* buffer, uint32_t buffer_size, uint32_t* bytes_read,
                         uint32_t* out_sender_id = nullptr,
                         uint32_t* out_message_id = nullptr,
                         bool block = true);

            uint32_t FindQueue(const char* name);
            bool GetQueueInfo(uint32_t queue_id, uint32_t* message_count,
                              uint32_t* max_messages, uint32_t* max_message_size);
        }

    } // namespace process
} // namespace kos

#endif // __KOS__PROCESS__MESSAGE_QUEUE_H
