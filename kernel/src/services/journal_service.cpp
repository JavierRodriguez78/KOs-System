#include "services/journal_service.hpp"
#include "lib/string.hpp"
#include <lib/socket.hpp>


using namespace kos::services;
using namespace kos::lib;

#define JOURNAL_SOCKET_PATH "/run/systemd/journal/socket"
#define LOG_BUFFER_SIZE 100
#define LOG_ENTRY_SIZE 100

JournalService::JournalService() : logCount(0), journalSocket(nullptr) {}

JournalService::~JournalService() {
    Stop();
    if (journalSocket) {
        delete journalSocket;
        journalSocket = nullptr;
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
    return ok;
}

void JournalService::Stop() {
    if (journalSocket) {
        journalSocket->closeSocket();
        delete journalSocket;
        journalSocket = nullptr;
    }
}

void JournalService::log(const char* message) {
    if (logCount < LOG_BUFFER_SIZE) {
        kos::lib::String::strncpy(reinterpret_cast<int8_t*>(logBuffer[logCount]), reinterpret_cast<const int8_t*>(message), LOG_ENTRY_SIZE - 1);
        logBuffer[logCount][LOG_ENTRY_SIZE - 1] = '\0';
        ++logCount;
    }
    sendToSocket(message);
}

// setupSocket and closeSocket are now handled by the Socket class

void JournalService::sendToSocket(const char* message) {
    if (!journalSocket || journalSocket->getFd() < 0) return;
        if (journalSocket)
            journalSocket->send(message, kos::lib::String::strlen(message));
}
