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
JournalService::JournalService() : logCount(0), journalSocket(nullptr), logFileFd(-1), logSeq(0) { fsReady = false; pendingFlush = false; }

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
    if (journalSocket) { journalSocket->closeSocket(); delete journalSocket; journalSocket = nullptr; }
    journalSocket = new kos::lib::Socket(kos::lib::SocketDomain::UNIX, kos::lib::SocketType::DGRAM, kos::lib::SocketProtocol::DEFAULT);
    bool ok = journalSocket->connect("/run/systemd/journal/socket");
    // If FS is available, set ready and create dirs
    fsReady = (kos::services::get_fs() != nullptr);
    if (fsReady) {
        ensureLogDir();
        openLogFile();
        if (pendingFlush) { Flush(); pendingFlush = false; }
    } else {
        pendingFlush = true;
    }
    log("JournalService started");
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
    if (!message) return;
    if (logCount < LOG_BUFFER_SIZE) {
        kos::lib::String::strncpy((int8_t*)logBuffer[logCount], (const int8_t*)message, LOG_ENTRY_SIZE - 1);
        logBuffer[logCount][LOG_ENTRY_SIZE - 1] = 0;
        ++logCount;
    }
    sendToSocket(message);
    persistLog(message);
}

void JournalService::Flush() {
    for (int i = 0; i < logCount; ++i) {
        persistLog(logBuffer[i]);
    }
}

void JournalService::Rotate() {
    // Simple rotation: append sequence number into rotated file name
    ++logSeq;
    char rotated[64];
    // /var/log/system.log.N
    const char* base = "/var/log/system.log";
    int bi=0; for (; base[bi] && bi < (int)sizeof(rotated)-1; ++bi) rotated[bi] = base[bi];
    rotated[bi++]='.';
    // convert seq to decimal
    uint32_t n = logSeq; char rev[16]; int ri=0; if(n==0){rev[ri++]='0';} while(n && ri<16){ rev[ri++]=char('0'+(n%10)); n/=10; }
    while(ri) rotated[bi++] = rev[--ri];
    rotated[bi]=0;
    // For simplicity, we just write a rotation marker line to new file and continue writing there
    openLogFile(); // reopen (placeholder; current fs API doesn't maintain file descriptors)
    writeRaw("--- log rotated ---");
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
    if (!message) return;
    if (kos::services::get_fs()) {
        fsReady = true;
        ensureLogDir();
        writeRaw(message);
    } else {
        pendingFlush = true;
    }
}

void JournalService::ensureLogDir() {
    auto fs = get_fs(); if (!fs) return;
    // Minimal: attempt to create /var and /var/log (ignore failures)
    fs->Mkdir((const int8_t*)"/var", 1);
    fs->Mkdir((const int8_t*)"/var/log", 1);
}

void JournalService::openLogFile() {
    // Placeholder: Filesystem lacks open/append; we emulate by appending via WriteFile writes.
}

void JournalService::writeRaw(const char* line) {
    auto fs = get_fs(); if (!fs) return;
    fs->WriteFile((const int8_t*)"/var/log/system.log", (const uint8_t*)line, kos::lib::String::strlen(line));
    fs->WriteFile((const int8_t*)"/var/log/system.log", (const uint8_t*)"\n", 1);
}

void JournalService::Tick() {
    readFromSocket();
}
