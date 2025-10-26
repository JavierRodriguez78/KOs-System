
#include "lib/socket.hpp"
#include "lib/string.hpp"

using namespace kos::lib;

struct InternalSocket {
    int fd;
    bool bound = false;
    bool connected = false;
    char path[128];
    SocketDomain domain;
    SocketType type;
    SocketProtocol protocol;
};

static constexpr int MAX_SOCKETS = 128;
static InternalSocket socket_table[MAX_SOCKETS];
static int socket_count = 0;
static int next_fd = 100;

static InternalSocket* find_socket(int fd) {
    for (int i = 0; i < socket_count; ++i) {
        if (socket_table[i].fd == fd) return &socket_table[i];
    }
    return nullptr;
}

int Socket::kos_socket(SocketDomain domain, SocketType type, SocketProtocol protocol) {
    if (socket_count >= MAX_SOCKETS) return -1;
    int fd = next_fd++;
    InternalSocket& sock = socket_table[socket_count++];
    sock.fd = fd;
    sock.bound = false;
    sock.connected = false;
    sock.path[0] = '\0';
    sock.domain = domain;
    sock.type = type;
    sock.protocol = protocol;
    return fd;
}

Socket::Socket(SocketDomain domain, SocketType type, SocketProtocol protocol) {
    socketFd = kos_socket(domain, type, protocol);
    // Error handling can be added here if needed
}

Socket::~Socket() {
    closeSocket();
}

bool Socket::setupSocket(const char* path) {
    InternalSocket* sock = find_socket(socketFd);
    if (!sock) return false;
    if (sock->bound) return false;
    sock->bound = true;
    String::strncpy(reinterpret_cast<int8_t*>(sock->path), reinterpret_cast<const int8_t*>(path), sizeof(sock->path));
    return true;
}

bool Socket::connect(const char* path) {
    InternalSocket* sock = find_socket(socketFd);
    if (!sock) return false;
    if (sock->connected) return false;
    sock->connected = true;
    String::strncpy(reinterpret_cast<int8_t*>(sock->path), reinterpret_cast<const int8_t*>(path), sizeof(sock->path));
    return true;
}

void Socket::closeSocket() {
    for (int i = 0; i < socket_count; ++i) {
        if (socket_table[i].fd == socketFd) {
            for (int j = i; j < socket_count - 1; ++j) {
                socket_table[j] = socket_table[j + 1];
            }
            --socket_count;
            socketFd = -1;
            break;
        }
    }
}

void Socket::stop() {
    closeSocket();
}

int Socket::getFd() const {
    return socketFd;
}

int Socket::send(const char* data, int length) {
    InternalSocket* sender = find_socket(socketFd);
    if (!sender || socketFd < 0 || !sender->connected) return -1;
    InternalSocket* receiver = nullptr;
    for (int i = 0; i < socket_count; ++i) {
        if (i == sender - socket_table) continue;
        // Use the overload with explicit length for int8_t*
        if (String::strcmp(reinterpret_cast<const int8_t*>(socket_table[i].path), reinterpret_cast<const int8_t*>(sender->path), sizeof(socket_table[i].path)) == 0) {
            receiver = &socket_table[i];
            break;
        }
    }
    if (!receiver) return -2;
    // Simulate data transfer: in a real kernel, this would enqueue data to the receiver's buffer
    return length;
}

