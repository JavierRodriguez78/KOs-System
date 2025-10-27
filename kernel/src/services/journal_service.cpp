#include "services/journal_service.hpp"
#include "lib/string.hpp"
#include <lib/socket.hpp>


using namespace kos::services;
using namespace kos::lib;

#define JOURNAL_SOCKET_PATH "/run/systemd/journal/socket"
#define LOG_BUFFER_SIZE 100
#define LOG_ENTRY_SIZE 100

// Remove duplicate constructor
#include <fs/filesystem.hpp>

// Helper at global scope to access the filesystem pointer defined in kernel.cpp
namespace kos {
namespace services {
extern kos::fs::Filesystem* get_fs();
}
}
JournalService::JournalService() : logCount(0), journalSocket(nullptr), logFileFd(-1) {}

JournalService::~JournalService() {
    Stop();
    if (journalSocket) {
        delete journalSocket;
        journalSocket = nullptr;
    }
    if (logFileFd >= 0) {
        // Close log file (pseudo code, replace with your FS API)
        // fs_close(logFileFd);
        logFileFd = -1;
    }
}

bool JournalService::Start() {
    if (journalSocket) {
        journalSocket->closeSocket();
        delete journalSocket;
        journalSocket = nullptr;
    }
    journalSocket = new kos::lib::Socket(kos::lib::SocketDomain::UNIX, kos::lib::SocketType::DGRAM, kos::lib::SocketProtocol::DEFAULT);
    bool ok = journalSocket->connect("/run/systemd/journal/socket");
    // Open log file for appending (pseudo code, replace with your FS API)
    // logFileFd = fs_open("/var/log/system.log", O_WRONLY | O_CREAT | O_APPEND);
    // If using your Filesystem class:
    // auto fs = get_fs();
    // if (fs) logFileFd = fs->OpenFile("/var/log/system.log", FS_WRITE | FS_APPEND | FS_CREATE);
    return ok;
}

void JournalService::Stop() {
    if (journalSocket) {
        journalSocket->closeSocket();
        delete journalSocket;
        journalSocket = nullptr;
    }
    if (logFileFd >= 0) {
        // Close log file (pseudo code, replace with your FS API)
        // fs_close(logFileFd);
        logFileFd = -1;
    }
}

void JournalService::log(const char* message) {
    if (logCount < LOG_BUFFER_SIZE) {
        kos::lib::String::strncpy(reinterpret_cast<int8_t*>(logBuffer[logCount]), reinterpret_cast<const int8_t*>(message), LOG_ENTRY_SIZE - 1);
        logBuffer[logCount][LOG_ENTRY_SIZE - 1] = '\0';
        ++logCount;
    }
    sendToSocket(message);
    persistLog(message);
}

// setupSocket and closeSocket are now handled by the Socket class

void JournalService::sendToSocket(const char* message) {
    if (!journalSocket || journalSocket->getFd() < 0) return;
        if (journalSocket)
            journalSocket->send(message, kos::lib::String::strlen(message));
}

void JournalService::readFromSocket() {
    if (!journalSocket || journalSocket->getFd() < 0) return;
    char buf[LOG_ENTRY_SIZE];
    // If recv is not implemented, use a stub or available method
    // int bytes = journalSocket->recv(buf, LOG_ENTRY_SIZE - 1);
    // For now, simulate no data read:
    int bytes = 0;
    // If you have a Filesystem or other API to read from socket, use it here
    if (bytes > 0) {
        buf[bytes] = '\0';
        persistLog(buf);
    }
}

void JournalService::persistLog(const char* message) {
    auto fs = get_fs();
    if (fs) {
        fs->WriteFile("/var/log/system.log", (const uint8_t*)message, kos::lib::String::strlen(message));
        fs->WriteFile("/var/log/system.log", (const uint8_t*)"\n", 1);
    }
}

void JournalService::Tick() {
    readFromSocket();
}
