#pragma once

#ifndef KOS_LIB_SOCKET_HPP
#define KOS_LIB_SOCKET_HPP

namespace kos {
    namespace lib {

        // Custom enums for domain, type, protocol
        enum class SocketDomain {
            UNIX = 1,
            INET = 2
            // Add more as needed
        };

        // Custom enums for socket type and protocol
        enum class SocketType {
            DGRAM = 1,
            STREAM = 2
            // Add more as needed
        };

        // Custom enum for socket protocol
        enum class SocketProtocol {
            DEFAULT = 0
            // Add more as needed
        };


        /*
        @brief Socket class for managing network sockets
        */
        class Socket {
            public:
                /*
                @brief Constructor to create a socket with specified domain, type, and protocol
                @param domain The socket domain (e.g., UNIX, INET)
                @param type The socket type (e.g., DGRAM, STREAM)
                @param protocol The socket protocol (e.g., DEFAULT) 
                */
                Socket(SocketDomain domain, SocketType type, SocketProtocol protocol);
                
                /*
                @brief Destructor to clean up the socket
                */  
                ~Socket();

                /*
                @brief Set up the socket
                @param path The path to the socket (for UNIX domain sockets)
                @return True on success, false on failure
                */
                bool setupSocket(const char* path);

                /*
                @brief Connect to the socket
                @param path The path to the socket (for UNIX domain sockets)
                @return True on success, false on failure
                */
                bool connect(const char* path);

                /*
                @brief Close the socket
                */
                void closeSocket();

                /*
                @brief Stop the socket
                */
                void stop();

    
                /*
                @brief Get the file descriptor of the socket
                @return The file descriptor of the socket
                */  
                int getFd() const;


                /*
                @brief Create a socket
                @param domain The socket domain (e.g., UNIX, INET)
                @param type The socket type (e.g., DGRAM, STREAM)
                @param protocol The socket protocol (e.g., DEFAULT)
                @return The file descriptor of the created socket, or -1 on failure
                */
                int kos_socket(SocketDomain domain, SocketType type, SocketProtocol protocol);

                /*
                @brief Send data through the socket
                @param data The data to send
                @param length The length of the data to send
                @return The number of bytes sent, or -1 on failure
                */
                int send(const char* data, int length);

            private:
                // File descriptor for the socket
                int socketFd;
        };
    } // namespace lib
} // namespace kos

#endif // KOS_LIB_SOCKET_HPP