#include <process/pipe.hpp>
#include <process/scheduler.hpp>
#include <memory/heap.hpp>
#include <lib/string.hpp>
#include <console/logger.hpp>
#include <console/tty.hpp>

using namespace kos::process;
using namespace kos::memory;
using namespace kos::lib;
using namespace kos::console;

// Global pipe manager instance
PipeManager* kos::process::g_pipe_manager = nullptr;

// PipeMessage implementation

PipeMessage::PipeMessage(uint32_t sender, uint32_t size, const void* msg_data) 
    : sender_id(sender), data_size(size), data(nullptr), next(nullptr) {
    
    if (size > 0 && msg_data) {
        data = (uint8_t*)Heap::Alloc(size);
        if (data) {
            memcpy(data, msg_data, size);
        } else {
            data_size = 0;
        }
    }
}

PipeMessage::~PipeMessage() {
    if (data) {
        Heap::Free(data);
        data = nullptr;
    }
}

// Pipe implementation

Pipe::Pipe(uint32_t id, const char* pipe_name, uint32_t buffer_sz, uint32_t max_msgs)
    : pipe_id(id), buffer_size(buffer_sz), current_size(0), max_messages(max_msgs),
      message_count(0), message_queue(nullptr), queue_tail(nullptr),
      reader_count(0), writer_count(0), is_closed(false), blocking_mode(true) {
    
    // Copy pipe name
    int name_len = strlen(pipe_name);
    if (name_len >= sizeof(name)) name_len = sizeof(name) - 1;
    for (int i = 0; i < name_len; i++) {
        name[i] = pipe_name[i];
    }
    name[name_len] = '\0';
    
    // Initialize reader and writer lists
    for (int i = 0; i < 8; i++) {
        readers[i] = nullptr;
        writers[i] = nullptr;
    }
    
    // Create synchronization objects
    pipe_mutex = new Mutex();
    read_cv = new ConditionVariable();
    write_cv = new ConditionVariable();
}

Pipe::~Pipe() {
    Close();
    Flush();
    
    if (pipe_mutex) delete pipe_mutex;
    if (read_cv) delete read_cv;
    if (write_cv) delete write_cv;
}

bool Pipe::Write(uint32_t sender_id, const void* data, uint32_t size, bool block) {
    if (!data || size == 0 || is_closed) return false;
    
    // Check write permissions
    if (!CanWrite(sender_id)) {
        return false;
    }
    
    LockGuard lock(*pipe_mutex);
    
    // Check if pipe is full
    while (IsFull()) {
        if (!block || is_closed) {
            return false;
        }
        write_cv->Wait(*pipe_mutex);
    }
    
    // Create new message
    PipeMessage* msg = new PipeMessage(sender_id, size, data);
    if (!msg || !msg->data) {
        delete msg;
        return false;
    }
    
    // Add to message queue
    if (!message_queue) {
        message_queue = queue_tail = msg;
    } else {
        queue_tail->next = msg;
        queue_tail = msg;
    }
    
    message_count++;
    current_size += size;
    
    // Notify waiting readers
    read_cv->Signal();
    
    return true;
}

bool Pipe::Read(uint32_t reader_id, void* buffer, uint32_t buffer_size, 
                uint32_t* bytes_read, uint32_t* sender_id, bool block) {
    if (!buffer || buffer_size == 0 || is_closed) return false;
    
    // Check read permissions
    if (!CanRead(reader_id)) {
        return false;
    }
    
    LockGuard lock(*pipe_mutex);
    
    // Wait for messages
    while (IsEmpty()) {
        if (!block || is_closed) {
            if (bytes_read) *bytes_read = 0;
            return false;
        }
        read_cv->Wait(*pipe_mutex);
    }
    
    // Get first message
    PipeMessage* msg = message_queue;
    if (!msg) {
        if (bytes_read) *bytes_read = 0;
        return false;
    }
    
    // Copy data to buffer
    uint32_t copy_size = (msg->data_size < buffer_size) ? msg->data_size : buffer_size;
    memcpy(buffer, msg->data, copy_size);
    
    if (bytes_read) *bytes_read = copy_size;
    if (sender_id) *sender_id = msg->sender_id;
    
    // Remove message from queue
    message_queue = msg->next;
    if (!message_queue) {
        queue_tail = nullptr;
    }
    
    message_count--;
    current_size -= msg->data_size;
    
    delete msg;
    
    // Notify waiting writers
    write_cv->Signal();
    
    return true;
}

bool Pipe::Peek(void* buffer, uint32_t buffer_size, uint32_t* bytes_available, uint32_t* sender_id) {
    if (!buffer || buffer_size == 0 || is_closed) return false;
    
    LockGuard lock(*pipe_mutex);
    
    if (IsEmpty()) {
        if (bytes_available) *bytes_available = 0;
        return false;
    }
    
    PipeMessage* msg = message_queue;
    if (!msg) {
        if (bytes_available) *bytes_available = 0;
        return false;
    }
    
    // Copy data without removing message
    uint32_t copy_size = (msg->data_size < buffer_size) ? msg->data_size : buffer_size;
    memcpy(buffer, msg->data, copy_size);
    
    if (bytes_available) *bytes_available = copy_size;
    if (sender_id) *sender_id = msg->sender_id;
    
    return true;
}

bool Pipe::AddReader(Thread* thread) {
    if (!thread || reader_count >= 8) return false;
    
    LockGuard lock(*pipe_mutex);
    
    // Check if already added
    for (uint32_t i = 0; i < reader_count; i++) {
        if (readers[i] && readers[i]->task_id == thread->task_id) {
            return true; // Already added
        }
    }
    
    readers[reader_count++] = thread;
    return true;
}

bool Pipe::AddWriter(Thread* thread) {
    if (!thread || writer_count >= 8) return false;
    
    LockGuard lock(*pipe_mutex);
    
    // Check if already added
    for (uint32_t i = 0; i < writer_count; i++) {
        if (writers[i] && writers[i]->task_id == thread->task_id) {
            return true; // Already added
        }
    }
    
    writers[writer_count++] = thread;
    return true;
}

bool Pipe::RemoveReader(uint32_t thread_id) {
    LockGuard lock(*pipe_mutex);
    
    for (uint32_t i = 0; i < reader_count; i++) {
        if (readers[i] && readers[i]->task_id == thread_id) {
            // Shift remaining readers
            for (uint32_t j = i; j < reader_count - 1; j++) {
                readers[j] = readers[j + 1];
            }
            readers[--reader_count] = nullptr;
            return true;
        }
    }
    return false;
}

bool Pipe::RemoveWriter(uint32_t thread_id) {
    LockGuard lock(*pipe_mutex);
    
    for (uint32_t i = 0; i < writer_count; i++) {
        if (writers[i] && writers[i]->task_id == thread_id) {
            // Shift remaining writers
            for (uint32_t j = i; j < writer_count - 1; j++) {
                writers[j] = writers[j + 1];
            }
            writers[--writer_count] = nullptr;
            return true;
        }
    }
    return false;
}

bool Pipe::CanRead(uint32_t thread_id) const {
    // Allow reading if no specific readers are set, or if thread is in reader list
    if (reader_count == 0) return true;
    
    for (uint32_t i = 0; i < reader_count; i++) {
        if (readers[i] && readers[i]->task_id == thread_id) {
            return true;
        }
    }
    return false;
}

bool Pipe::CanWrite(uint32_t thread_id) const {
    // Allow writing if no specific writers are set, or if thread is in writer list
    if (writer_count == 0) return true;
    
    for (uint32_t i = 0; i < writer_count; i++) {
        if (writers[i] && writers[i]->task_id == thread_id) {
            return true;
        }
    }
    return false;
}

void Pipe::Close() {
    LockGuard lock(*pipe_mutex);
    is_closed = true;
    
    // Wake up all waiting threads
    read_cv->Broadcast();
    write_cv->Broadcast();
}

void Pipe::Flush() {
    LockGuard lock(*pipe_mutex);
    
    // Delete all messages
    while (message_queue) {
        PipeMessage* msg = message_queue;
        message_queue = message_queue->next;
        delete msg;
    }
    
    queue_tail = nullptr;
    message_count = 0;
    current_size = 0;
    
    // Notify waiting writers
    write_cv->Broadcast();
}

void Pipe::PrintInfo() const {
    TTY::Write("Pipe ID: ");
    TTY::WriteHex(pipe_id);
    TTY::Write(" Name: ");
    TTY::Write(name);
    TTY::Write(" Messages: ");
    TTY::WriteHex(message_count);
    TTY::Write("/");
    TTY::WriteHex(max_messages);
    TTY::Write(" Size: ");
    TTY::WriteHex(current_size);
    TTY::Write("/");
    TTY::WriteHex(buffer_size);
    TTY::Write(" Readers: ");
    TTY::WriteHex(reader_count);
    TTY::Write(" Writers: ");
    TTY::WriteHex(writer_count);
    TTY::Write(is_closed ? " [CLOSED]" : " [OPEN]");
    TTY::Write(blocking_mode ? " [BLOCKING]" : " [NON-BLOCKING]");
    TTY::Write("\n");
}

// PipeManager implementation

PipeManager::PipeManager() : pipe_count(0), next_pipe_id(1) {
    for (int i = 0; i < 64; i++) {
        pipes[i] = nullptr;
    }
    manager_mutex = new Mutex();
    Logger::Log("Pipe manager initialized");
}

PipeManager::~PipeManager() {
    CloseAllPipes();
    if (manager_mutex) delete manager_mutex;
}

Pipe* PipeManager::CreatePipe(const char* name, uint32_t buffer_size, uint32_t max_messages) {
    if (!name || pipe_count >= 64) return nullptr;
    
    LockGuard lock(*manager_mutex);
    
    // Check if pipe with same name already exists
    if (FindPipeIndex(name) >= 0) {
        return nullptr; // Pipe already exists
    }
    
    // Find empty slot
    int slot = -1;
    for (int i = 0; i < 64; i++) {
        if (!pipes[i]) {
            slot = i;
            break;
        }
    }
    
    if (slot < 0) return nullptr;
    
    // Create pipe
    Pipe* pipe = new Pipe(next_pipe_id++, name, buffer_size, max_messages);
    if (!pipe) return nullptr;
    
    pipes[slot] = pipe;
    pipe_count++;
        if (Logger::IsDebugEnabled()) {
        Logger::Log("Created pipe");
    }
    return pipe;
}

bool PipeManager::DestroyPipe(uint32_t pipe_id) {
    LockGuard lock(*manager_mutex);
    
    int index = FindPipeIndex(pipe_id);
    if (index < 0) return false;
    
    delete pipes[index];
    pipes[index] = nullptr;
    pipe_count--;
    
    Logger::Log("Destroyed pipe");
    return true;
}

bool PipeManager::DestroyPipe(const char* name) {
    if (!name) return false;
    
    LockGuard lock(*manager_mutex);
    
    int index = FindPipeIndex(name);
    if (index < 0) return false;
    
    delete pipes[index];
    pipes[index] = nullptr;
    pipe_count--;
    
    Logger::Log("Destroyed pipe");
    return true;
}

Pipe* PipeManager::FindPipe(uint32_t pipe_id) const {
    int index = FindPipeIndex(pipe_id);
    return (index >= 0) ? pipes[index] : nullptr;
}

Pipe* PipeManager::FindPipe(const char* name) const {
    if (!name) return nullptr;
    
    int index = FindPipeIndex(name);
    return (index >= 0) ? pipes[index] : nullptr;
}

int PipeManager::FindPipeIndex(uint32_t pipe_id) const {
    for (int i = 0; i < 64; i++) {
        if (pipes[i] && pipes[i]->GetId() == pipe_id) {
            return i;
        }
    }
    return -1;
}

int PipeManager::FindPipeIndex(const char* name) const {
    if (!name) return -1;
    
    for (int i = 0; i < 64; i++) {
        if (pipes[i] && String::strcmp((const int8_t*)pipes[i]->GetName(), (const int8_t*)name, strlen(name)) == 0) {
            return i;
        }
    }
    return -1;
}

void PipeManager::CloseAllPipes() {
    LockGuard lock(*manager_mutex);
    
    for (int i = 0; i < 64; i++) {
        if (pipes[i]) {
            delete pipes[i];
            pipes[i] = nullptr;
        }
    }
    pipe_count = 0;
}

void PipeManager::PrintAllPipes() const {
    LockGuard lock(*manager_mutex);
    
    TTY::Write("=== Pipe Manager ===\n");
    TTY::Write("Active pipes: ");
    TTY::WriteHex(pipe_count);
    TTY::Write("/64\n");
    
    for (int i = 0; i < 64; i++) {
        if (pipes[i]) {
            pipes[i]->PrintInfo();
        }
    }
}

// PipeAPI implementation (process namespace)

namespace kos::process::PipeAPI {
    
    uint32_t CreatePipe(const char* name, uint32_t buffer_size, uint32_t max_messages) {
        if (!g_pipe_manager) return 0;
        
        Pipe* pipe = g_pipe_manager->CreatePipe(name, buffer_size, max_messages);
        return pipe ? pipe->GetId() : 0;
    }
    
    bool DestroyPipe(uint32_t pipe_id) {
        if (!g_pipe_manager) return false;
        return g_pipe_manager->DestroyPipe(pipe_id);
    }
    
    bool DestroyPipe(const char* name) {
        if (!g_pipe_manager) return false;
        return g_pipe_manager->DestroyPipe(name);
    }
    
    bool WritePipe(uint32_t pipe_id, const void* data, uint32_t size, bool block) {
        if (!g_pipe_manager || !g_scheduler) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(pipe_id);
        if (!pipe) return false;
        
        Thread* current = g_scheduler->GetCurrentTask();
        uint32_t sender_id = current ? current->task_id : 0;
        
        return pipe->Write(sender_id, data, size, block);
    }
    
    bool WritePipe(const char* name, const void* data, uint32_t size, bool block) {
        if (!g_pipe_manager || !g_scheduler) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(name);
        if (!pipe) return false;
        
        Thread* current = g_scheduler->GetCurrentTask();
        uint32_t sender_id = current ? current->task_id : 0;
        
        return pipe->Write(sender_id, data, size, block);
    }
    
    bool ReadPipe(uint32_t pipe_id, void* buffer, uint32_t buffer_size, 
                  uint32_t* bytes_read, uint32_t* sender_id, bool block) {
        if (!g_pipe_manager || !g_scheduler) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(pipe_id);
        if (!pipe) return false;
        
        Thread* current = g_scheduler->GetCurrentTask();
        uint32_t reader_id = current ? current->task_id : 0;
        
        return pipe->Read(reader_id, buffer, buffer_size, bytes_read, sender_id, block);
    }
    
    bool ReadPipe(const char* name, void* buffer, uint32_t buffer_size, 
                  uint32_t* bytes_read, uint32_t* sender_id, bool block) {
        if (!g_pipe_manager || !g_scheduler) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(name);
        if (!pipe) return false;
        
        Thread* current = g_scheduler->GetCurrentTask();
        uint32_t reader_id = current ? current->task_id : 0;
        
        return pipe->Read(reader_id, buffer, buffer_size, bytes_read, sender_id, block);
    }
    
    bool AddPipeReader(uint32_t pipe_id, uint32_t thread_id) {
        if (!g_pipe_manager || !g_scheduler) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(pipe_id);
        Thread* thread = g_scheduler->FindTask(thread_id);
        
        if (!pipe || !thread) return false;
        
        return pipe->AddReader(thread);
    }
    
    bool AddPipeWriter(uint32_t pipe_id, uint32_t thread_id) {
        if (!g_pipe_manager || !g_scheduler) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(pipe_id);
        Thread* thread = g_scheduler->FindTask(thread_id);
        
        if (!pipe || !thread) return false;
        
        return pipe->AddWriter(thread);
    }
    
    bool RemovePipeReader(uint32_t pipe_id, uint32_t thread_id) {
        if (!g_pipe_manager) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(pipe_id);
        if (!pipe) return false;
        
        return pipe->RemoveReader(thread_id);
    }
    
    bool RemovePipeWriter(uint32_t pipe_id, uint32_t thread_id) {
        if (!g_pipe_manager) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(pipe_id);
        if (!pipe) return false;
        
        return pipe->RemoveWriter(thread_id);
    }
    
    uint32_t FindPipe(const char* name) {
        if (!g_pipe_manager) return 0;
        
        Pipe* pipe = g_pipe_manager->FindPipe(name);
        return pipe ? pipe->GetId() : 0;
    }
    
    bool GetPipeInfo(uint32_t pipe_id, uint32_t* message_count, uint32_t* current_size,
                     uint32_t* available_space) {
        if (!g_pipe_manager) return false;
        
        Pipe* pipe = g_pipe_manager->FindPipe(pipe_id);
        if (!pipe) return false;
        
        if (message_count) *message_count = pipe->GetMessageCount();
        if (current_size) *current_size = pipe->GetCurrentSize();
        if (available_space) *available_space = pipe->GetAvailableSpace();
        
        return true;
    }
}