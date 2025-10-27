#pragma once

#ifndef KOS_SERVICES_JOURNAL_SERVICE_HPP
#define KOS_SERVICES_JOURNAL_SERVICE_HPP

#include "service.hpp"
#include "lib/string.hpp"
#include "lib/socket.hpp"


namespace kos {
    namespace services {
        // Journal service: logs system events to the systemd journal via a Unix socket.
        class JournalService : public IService {
            public:
                JournalService();
                ~JournalService() override;

                const char* Name() const override { return "JOURNAL"; }
                bool Start() override;
                void Stop() override;
                void log(const char* message);
                void Tick() override;

            private:
                void sendToSocket(const char* message);
                void readFromSocket();
                void persistLog(const char* message);
                static constexpr int LOG_BUFFER_SIZE = 128;
                static constexpr int LOG_ENTRY_SIZE = 256;
                char logBuffer[LOG_BUFFER_SIZE][LOG_ENTRY_SIZE];
                int logCount;
                kos::lib::Socket* journalSocket;
                int logFileFd;
        };
    } // namespace services
} // namespace kos
#endif // KOS_SERVICES_JOURNAL_SERVICE_HPP