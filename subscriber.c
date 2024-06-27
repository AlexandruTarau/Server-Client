#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"
#include <netinet/tcp.h>

/* Function that runs the client. */
void run_client(int sockfd) {
    char buf[100];

    struct client_packet sent_packet;
    struct server_packet recv_packet;

    struct pollfd pfds[2];

    // Read user data from standard input
    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    
    // Add listener socket
    pfds[1].fd = sockfd;
    pfds[1].events = POLLIN;

    while (1) {
        poll(pfds, 2, -1);

        if (pfds[0].revents & POLLIN) {
            if (fgets(buf, sizeof(buf), stdin)) {
                int ok = 1;

                if (!strncmp(buf, "exit\n", 5)) {
                    sent_packet.type = 0;
                } else if (!strncmp(buf, "subscribe ", 10)) {
                    buf[strlen(buf) - 1] = 0;  // Make sure the string ends with '\0'
                    char *topic = buf + 10;
                    strcpy(sent_packet.topic, topic);
                    sent_packet.type = 1;
                    printf("Subscribed to topic %s\n", topic);
                } else if (!strncmp(buf, "unsubscribe ", 12)) {
                    buf[strlen(buf) - 1] = 0;  // Make sure the string ends with '\0'
                    char *topic = buf + 12;
                    strcpy(sent_packet.topic, topic);
                    sent_packet.type = 2;
                    printf("Unsubscribed from topic %s\n", topic);
                } else {
                    fprintf(stderr, "Invalid command!\n");
                    ok = 0;
                }
                if (ok) {
                    send(sockfd, &sent_packet, sizeof(struct client_packet), 0);
                }
            }
        }
        if (pfds[1].revents & POLLIN) {
            // Receive a message and show it's content
            int rc = recv_all(sockfd, &recv_packet, SERVER_PACKET_MAXSIZE);

            if (rc <= 0) {
                break;
            }
            char type[11] = {0};
            switch (recv_packet.type) {
                case 0: {
                    strcpy(type, "INT");
                    break;
                }
                case 1: {
                    strcpy(type, "SHORT_REAL");
                    break;
                }
                case 2: {
                    strcpy(type, "FLOAT");
                    break;
                }
                case 3: {
                    strcpy(type, "STRING");
                    break;
                }
            }
            printf("%s:%u - %s - %s - %s\n", recv_packet.ip, recv_packet.port, recv_packet.topic, type, recv_packet.data);
        }
    }
}

int main(int argc, char *argv[]) {
    // Turning off buffering for printing
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd = -1;

    if (argc != 4) {
        printf("\n Usage: %s <id> <ip> <port>\n", argv[0]);
        return 1;
    }

    // Reading port
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Creating TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // Completing server address, address family and port for connection
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Connecting to server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    // Send client ID
    rc = send(sockfd, argv[1], 10, 0);
    DIE(rc < 0, "send");

    // Turning off Nagle algorithm
    int enable = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(enable))) {
        perror("setsockopt(TCP_NODELAY) failed");
    }

    // Run the client
    run_client(sockfd);

    // Close the socket
    close(sockfd);

    return 0;
}