#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

int send_all(int sockfd, void *buff, size_t len);
int recv_all(int sockfd, void *buff, size_t len);

#define CLIENT_PACKET_MAXSIZE 1551
#define SERVER_PACKET_MAXSIZE 1572

struct client_packet {
    char topic[50];
    uint8_t type;
    char data[1500];
};

struct server_packet {
    char ip[16];
    uint16_t port;
    uint8_t type;
    char topic[51];
    char data[1501];
};

struct client {
    char id[10];
    char **topics;
    uint16_t num_topics;
    uint16_t max_topics;
    uint8_t isOnline;
};

#endif
