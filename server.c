#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"
#include <netinet/tcp.h>

/* Function that runs a server that receives data from UDP clients
and sends it to TCP clients. */
void run_server(int tcp_fd, int udp_fd) {
    int max_clients = 16, num_clients = 0, rc, exit_now = 0;
    struct client_packet received_packet;
    struct server_packet sent_packet;
    struct pollfd *poll_fds = (struct pollfd *)malloc((max_clients + 3) * sizeof(struct pollfd));
    struct client *clients = (struct client *)calloc(max_clients + 3, sizeof(struct client));

    for (int i = 0; i < max_clients + 3; i++) {
        clients[i].topics = (char **)malloc(16 * sizeof(char *));
        clients[i].max_topics = 16;
        for (int j = 0; j < 16; j++) {
            clients[i].topics[j] = (char *)malloc(51 * sizeof(char));
        }
    }

    // Set the tcp socket for listening
    rc = listen(tcp_fd, INT32_MAX);
    DIE(rc < 0, "listen");

    // Add the standard input listener to the poll
    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;
    clients[0].isOnline = 1;

    // Add the TCP listener to the poll
    poll_fds[1].fd = tcp_fd;
    poll_fds[1].events = POLLIN;
    clients[1].isOnline = 1;

    // Add the UDP listener to the poll
    poll_fds[2].fd = udp_fd;
    poll_fds[2].events = POLLIN;
    clients[2].isOnline = 1;

    while (!exit_now) {
        rc = poll(poll_fds, num_clients + 3, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_clients + 3; i++) {
            // The client is online and it takes an action
            if (clients[i].isOnline && (poll_fds[i].revents & POLLIN)) {
                if (poll_fds[i].fd == tcp_fd) {  // We receive a tcp packet
                    int j;
                    struct sockaddr_in tcp_addr;
                    socklen_t tcp_len = sizeof(tcp_addr);
                    int newsockfd = accept(tcp_fd, (struct sockaddr *)&tcp_addr, &tcp_len);
                    DIE(newsockfd < 0, "accept");

                    // Receive the client's ID
                    char id[10];
                    rc = recv(newsockfd, id, 10, 0);
                    DIE(rc < 0, "recv");

                    // Check if the client is already connected or is new
                    for (j = 3; j < num_clients + 3; j++) {
                        if (!strcmp(id, clients[j].id)) {
                            if (clients[j].isOnline) {
                                printf("Client %s already connected.\n", id);
                                close(newsockfd);
                                break;
                            } else {  // The client has connected before so we update its status
                                printf("New client %s connected from %s:%d.\n",
                                    clients[j].id, inet_ntoa(tcp_addr.sin_addr), ntohs(tcp_addr.sin_port)
                                    );
                                poll_fds[j].fd = newsockfd;
                                clients[j].isOnline = 1;
                                break;
                            }
                        }
                    }
                    
                    if (j == num_clients + 3) {  // The client is new, we add it to the poll and in clients
                        num_clients++;
                        if (num_clients > max_clients) {  // Expand the max number of clients
                            max_clients *= 2;
                            poll_fds = (struct pollfd *)realloc(poll_fds, (max_clients + 3) * sizeof(struct pollfd));
                            clients = (struct client *)realloc(clients, (max_clients + 3) * sizeof(struct client));
                            DIE(!poll_fds || !clients, "realloc");
                        }
                        poll_fds[j].fd = newsockfd;
                        poll_fds[j].events = POLLIN;

                        strcpy(clients[j].id, id);
                        clients[j].isOnline = 1;
                        clients[j].num_topics = 0;

                        printf("New client %s connected from %s:%d.\n",
                            clients[j].id, inet_ntoa(tcp_addr.sin_addr), ntohs(tcp_addr.sin_port)
                            );
                    }
                } else if (poll_fds[i].fd == udp_fd) {  // We receive a udp packet
                    socklen_t udp_len;
                    struct sockaddr_in serv_addr;
                    rc = recvfrom(udp_fd, &received_packet, CLIENT_PACKET_MAXSIZE, 0, (struct sockaddr *)&serv_addr, &udp_len);
                    DIE(rc < 0, "recvfrom");

                    // Prepare the packet to send to tcp clients
                    memset(&sent_packet, 0, sizeof(struct server_packet));
                    strcpy(sent_packet.topic, received_packet.topic);
                    sent_packet.topic[50] = 0;  // Make sure the topic ends in '\0'

                    strcpy(sent_packet.ip, inet_ntoa(serv_addr.sin_addr)); 
                    sent_packet.port = htons(serv_addr.sin_port);

                    switch (received_packet.type) {
                        case 0: {  // INT
                            unsigned int integer = ntohl(*(uint32_t *)(received_packet.data + 1));
                            if (received_packet.data[0]) {
                                sprintf(sent_packet.data, "%d", -integer);
                            } else {
                                sprintf(sent_packet.data, "%d", integer);
                            }
                            break;
                        }
                        case 1: {  // SHORT REAL
                            float short_real = ntohs(*(uint16_t *)(received_packet.data)) / 100.0;
                            sprintf(sent_packet.data, "%.2f", short_real);
                            break;
                        }
                        case 2: {  // FLOAT
                            uint32_t real = ntohl(*(uint32_t *)(received_packet.data + 1));
                            uint8_t num_decimals = *(received_packet.data + sizeof(uint32_t) + 1);
                            int power = 1;

                            for (int j = 0; j < num_decimals; j++) {
                                power *= 10;
                            }

                            if (received_packet.data[0]) {
                                sprintf(sent_packet.data, "%.*f", num_decimals, -(real * 1.0 / power));
                            } else {
                                sprintf(sent_packet.data, "%.*f", num_decimals, real * 1.0 / power);
                            }
                            break;
                        }
                        case 3: {  // STRING
                            strcpy(sent_packet.data, received_packet.data);
                            break;
                        }
                    }

                    // Separate each level of the received topic
                    char aux[51];
                    strcpy(aux, sent_packet.topic);
                    char *q = strtok(aux, "/");
                    char levels[51][51];
                    int l = 0, n;
                    while (q) {
                        strcpy(levels[l], q);
                        q = strtok(NULL, "/");
                        l++;
                    }
                    n = l;

                    sent_packet.type = received_packet.type;

                    for (int j = 3; j < num_clients + 3; j++) {
                        if (clients[j].isOnline) {
                            for (int k = 0; k < clients[j].num_topics; k++) {
                                if (!strcmp(clients[j].topics[k], sent_packet.topic)) {
                                    rc = send_all(poll_fds[j].fd, &sent_packet, SERVER_PACKET_MAXSIZE);
                                    DIE(rc < 0, "send");
                                    break;
                                }

                                // Separate each level of the current topic
                                int skip = 0;
                                strcpy(aux, clients[j].topics[k]);
                                char *p = strtok(aux, "/");
                                l = 0;
                                while (p && l < n) {
                                    // If wildcard '+' => skip once
                                    if (!strcmp(p, "+")) {
                                        p = strtok(NULL, "/");
                                        l++;
                                        if (!p) {
                                            break;
                                        }
                                    }
                                    
                                    // If wildcard '*' => skip until same level or end
                                    if (!strcmp(p, "*")) {
                                        p = strtok(NULL, "/");
                                        if (!p) {
                                            l = n;
                                            break;
                                        }
                                        skip = 1;
                                    }
                                    if (strcmp(p, levels[l])) {
                                        if (skip) {
                                            l++;
                                            continue;
                                        } else {
                                            break;
                                        }
                                    } else {
                                        if (skip) {
                                            skip = 0;
                                        }
                                    }
                                    p = strtok(NULL, "/");
                                    l++;
                                }

                                // Topics match => send the packet
                                if (!p && l == n) {
                                    rc = send_all(poll_fds[j].fd, &sent_packet, sizeof(struct server_packet));
                                    DIE(rc < 0, "send");
                                    break;
                                }
                            }
                        }
                    }
                } else if (poll_fds[i].fd == STDIN_FILENO) {  // Input from keyboard
                    char buf[20] = {0};
                    if (fgets(buf, sizeof(buf), stdin)) {
                        if (!strncmp(buf, "exit\n", 5)) {  // Exit command
                            exit_now = 1;
                            break;
                        } else {
                            fprintf(stderr, "Invalid command!\n");
                        }
                    }
                } else {
                    // Received data from a tcp client
                    int rc = recv(poll_fds[i].fd, &received_packet,
                                        sizeof(struct client_packet), 0);
                    DIE(rc < 0, "recv");
                    if (rc == 0) {
                        received_packet.type = 0;
                    }

                    switch (received_packet.type) {
                        case 0: {  // Exit instruction
                            printf("Client %s disconnected.\n", clients[i].id);
                            close(poll_fds[i].fd);
                            clients[i].isOnline = 0;
                            break;
                        }
                        case 1: {  // Subscribe instruction
                            int j;
                            received_packet.topic[50] = 0;  // Make sure the string ends in '\0'
                            for (j = 0; j < clients[i].num_topics; j++) {
                                if (!strcmp(clients[i].topics[j], received_packet.topic)) {
                                    break;
                                }
                            }

                            // We register the new topic
                            if (j == clients[i].num_topics) {
                                // Increase max number of topics is needed
                                if (clients[i].num_topics == clients[i].max_topics) {
                                    clients[i].max_topics *= 2;
                                    clients[i].topics = (char **)realloc(clients[i].topics, clients[i].max_topics * sizeof(char *));
                                    
                                    for (int k = clients[i].num_topics; k < clients[i].max_topics; k++) {
                                        clients[i].topics[k] = (char *)malloc(51 * sizeof(char));
                                    }
                                }

                                // Add the new topic
                                strcpy(clients[i].topics[clients[i].num_topics++], received_packet.topic);
                            }
                            break;
                        }
                        case 2: {  // Unsubscribe instruction
                            int j = 0;
                            received_packet.topic[50] = 0;
                            for (j = 0; j < clients[i].num_topics; j++) {
                                if (!strcmp(clients[i].topics[j], received_packet.topic)) {
                                    break;
                                }
                            }

                            // Remove topic
                            if (j != clients[i].num_topics) {
                                for (int k = j; k < clients[i].num_topics - 1; k++) {
                                    strcpy(clients[i].topics[k], clients[i].topics[k + 1]);
                                }
                                clients[i].num_topics--;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    // Close all client connections
    for (int i = 3; i < num_clients + 3; i++) {
        if (clients[i].isOnline) {
            close(poll_fds[i].fd);
        }
    }

    // Free memory
    for (int i = 0; i < max_clients + 3; i++) {
        for (int j = 0; j < clients[i].max_topics; j++) {
            free(clients[i].topics[j]);
        }
        free(clients[i].topics);
    }
    free(clients);
    free(poll_fds);
}

int main(int argc, char *argv[]) {
    // Turning off buffering for printing
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    
    if (argc != 2) {
        printf("\n Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // Reading port
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Creating tcp and udp sockets
    int tcp_fd = socket(AF_INET, SOCK_STREAM, 0);
    int udp_fd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(tcp_fd < 0 || udp_fd < 0, "socket");

    // Turning off Nagle algorithm
    int enable = 1;
    if (setsockopt(tcp_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(enable))) {
        perror("setsockopt(TCP_NODELAY) failed");
    }

    // Completing server address, address family and port for connection
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // We bind the TCP and UDP sockets
    rc = bind(tcp_fd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "tcp bind");

    rc = bind(udp_fd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "udp bind");

    // Run the server
    run_server(tcp_fd, udp_fd);

    // Close the TCP and UDP sockets
    close(tcp_fd);
    close(udp_fd);

    return 0;
}

