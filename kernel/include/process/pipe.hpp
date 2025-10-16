#ifndef __KOS__PROCESS__PIPE_H
#define __KOS__PROCESS__PIPE_H

#include <common/types.hpp>
#include <process/thread.h>
#include <process/sync.hpp>

using namespace kos::common;

namespace kos {
    namespace process {

        // Forward declarations
        class Thread;
        class Mutex;
        class ConditionVariable;

        // Pipe message structure
        struct PipeMessage {
            uint32_t sender_id;        // ID of sending thread
            uint32_t data_size;        // Size of data in bytes
            uint8_t* data;             // Pointer to message data
            PipeMessage* next;         // Next message in queue
            
            PipeMessage(uint32_t sender, uint32_t size, const void* msg_data);
            ~PipeMessage();
        };

        // Pipe for inter-task communication
        class Pipe {
        private:
            uint32_t pipe_id;              // Unique pipe identifier
            char name[32];                 // Pipe name for identification
            uint32_t buffer_size;          // Maximum buffer size in bytes
            uint32_t current_size;         // Current used buffer size
            uint32_t max_messages;         // Maximum number of messages
            uint32_t message_count;        // Current number of messages
            
            PipeMessage* message_queue;    // Queue of messages
            PipeMessage* queue_tail;       // Tail of message queue
            
            Mutex* pipe_mutex;             // Mutex for thread-safe access
            ConditionVariable* read_cv;    // Condition variable for readers
            ConditionVariable* write_cv;   // Condition variable for writers
            
            Thread* readers[8];            // List of threads that can read
            Thread* writers[8];            // List of threads that can write
            uint32_t reader_count;         // Number of registered readers
            uint32_t writer_count;         // Number of registered writers
            
            bool is_closed;                // Whether pipe is closed
            bool blocking_mode;            // Blocking or non-blocking mode

        public:
            Pipe(uint32_t id, const char* pipe_name, uint32_t buffer_sz = 4096, 
                 uint32_t max_msgs = 64);
            ~Pipe();

            // Pipe operations
            bool Write(uint32_t sender_id, const void* data, uint32_t size, bool block = true);
            bool Read(uint32_t reader_id, void* buffer, uint32_t buffer_size, 
                     uint32_t* bytes_read, uint32_t* sender_id = nullptr, bool block = true);
            bool Peek(void* buffer, uint32_t buffer_size, uint32_t* bytes_available,
                     uint32_t* sender_id = nullptr);
            
            // Permission management
            bool AddReader(Thread* thread);
            bool AddWriter(Thread* thread);
            bool RemoveReader(uint32_t thread_id);
            bool RemoveWriter(uint32_t thread_id);
            bool CanRead(uint32_t thread_id) const;
            bool CanWrite(uint32_t thread_id) const;
            
            // Pipe management
            void Close();
            void SetBlockingMode(bool blocking) { blocking_mode = blocking; }
            void Flush();
            
            // Information
            uint32_t GetId() const { return pipe_id; }
            const char* GetName() const { return name; }
            uint32_t GetMessageCount() const { return message_count; }
            uint32_t GetCurrentSize() const { return current_size; }
            uint32_t GetAvailableSpace() const { return buffer_size - current_size; }
            bool IsEmpty() const { return message_count == 0; }
            bool IsFull() const { return current_size >= buffer_size || message_count >= max_messages; }
            bool IsClosed() const { return is_closed; }
            bool IsBlocking() const { return blocking_mode; }
            
            void PrintInfo() const;
        };

        // Pipe manager for system-wide pipe management
        class PipeManager {
        private:
            Pipe* pipes[64];               // Array of pipes
            uint32_t pipe_count;           // Number of active pipes
            uint32_t next_pipe_id;         // For generating unique pipe IDs
            Mutex* manager_mutex;          // Mutex for thread-safe access
            
            // Internal helpers
            int FindPipeIndex(uint32_t pipe_id) const;
            int FindPipeIndex(const char* name) const;
            
        public:
            PipeManager();
            ~PipeManager();
            
            // Pipe creation and destruction
            Pipe* CreatePipe(const char* name, uint32_t buffer_size = 4096, uint32_t max_messages = 64);
            bool DestroyPipe(uint32_t pipe_id);
            bool DestroyPipe(const char* name);
            
            // Pipe lookup
            Pipe* FindPipe(uint32_t pipe_id) const;
            Pipe* FindPipe(const char* name) const;
            
            // System operations
            void CloseAllPipes();
            void PrintAllPipes() const;
            uint32_t GetPipeCount() const { return pipe_count; }
        };

        // Global pipe manager instance
        extern PipeManager* g_pipe_manager;

        // Pipe API for applications
        namespace PipeAPI {
            // Pipe creation
            uint32_t CreatePipe(const char* name, uint32_t buffer_size = 4096, uint32_t max_messages = 64);
            bool DestroyPipe(uint32_t pipe_id);
            bool DestroyPipe(const char* name);
            
            // Pipe operations
            bool WritePipe(uint32_t pipe_id, const void* data, uint32_t size, bool block = true);
            bool WritePipe(const char* name, const void* data, uint32_t size, bool block = true);
            bool ReadPipe(uint32_t pipe_id, void* buffer, uint32_t buffer_size, 
                         uint32_t* bytes_read, uint32_t* sender_id = nullptr, bool block = true);
            bool ReadPipe(const char* name, void* buffer, uint32_t buffer_size, 
                         uint32_t* bytes_read, uint32_t* sender_id = nullptr, bool block = true);
            
            // Pipe management
            bool AddPipeReader(uint32_t pipe_id, uint32_t thread_id);
            bool AddPipeWriter(uint32_t pipe_id, uint32_t thread_id);
            bool RemovePipeReader(uint32_t pipe_id, uint32_t thread_id);
            bool RemovePipeWriter(uint32_t pipe_id, uint32_t thread_id);
            
            // Information
            uint32_t FindPipe(const char* name);
            bool GetPipeInfo(uint32_t pipe_id, uint32_t* message_count, uint32_t* current_size,
                           uint32_t* available_space);
        }

    } // namespace process
} // namespace kos

#endif // __KOS__PROCESS__PIPE_H