
#ifndef KOS_LIBC_SOCKET_H
#define KOS_LIBC_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

// C enums for domain, type, protocol
typedef enum {
    KOS_SOCKET_DOMAIN_UNIX = 1,
    KOS_SOCKET_DOMAIN_INET = 2
    // Add more as needed
} kos_socket_domain_t;

typedef enum {
    KOS_SOCKET_TYPE_DGRAM = 1,
    KOS_SOCKET_TYPE_STREAM = 2
    // Add more as needed
} kos_socket_type_t;

typedef enum {
    KOS_SOCKET_PROTOCOL_DEFAULT = 0
    // Add more as needed
} kos_socket_protocol_t;

// C API for sockets
int kos_socket(kos_socket_domain_t domain, kos_socket_type_t type, kos_socket_protocol_t protocol);
int kos_socket_setup(int sockfd, const char* path);
int kos_socket_connect(int sockfd, const char* path);
void kos_socket_close(int sockfd);
void kos_socket_stop(int sockfd);
int kos_socket_send(int sockfd, const char* data, int length);

#ifdef __cplusplus
}
#endif

#endif // KOS_LIBC_SOCKET_H
