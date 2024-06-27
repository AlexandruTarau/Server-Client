#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>

/* Function that receives exactly len bytes from the buffer. */
int recv_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;

    while(bytes_remaining > 0) {
        int rc = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (rc <= 0) {
            return rc;
        }
        bytes_remaining -= rc;
        bytes_received += rc;
    }

    return bytes_received;
}

/* Function that sends exactly len bytes to the buffer. */
int send_all(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;

    while(bytes_remaining > 0) {
        int rc = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        if (rc <= 0) {
            return rc;
        }
        bytes_remaining -= rc;
        bytes_sent += rc;
    }

    return bytes_sent;
}