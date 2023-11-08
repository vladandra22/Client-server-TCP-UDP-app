#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/types.h> // for socket related types
#include <sys/socket.h> // for socket functions
#include <netinet/in.h> // for internet address family
#include <netinet/tcp.h>
#include <arpa/inet.h> // for internet address manipulation functions
#include <errno.h>
#include <unistd.h>
#include <poll.h>

#include <vector>

#include "helpers.h"

#define MAX_CONNECTIONS 1000
#define BUFLEN 1600

using namespace std;

// Implementare asemanatoare cu cea din server.cpp, insa cu un singur socket. 

int main(int argc, char *argv[]){

    // Dezactivam buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd;

    // Extragem portul
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // SOCK_STREAM = comunicare FIFO
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // conectam socket-ul
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    // trimitem catre server ID-ul clientului primit ca argument
    rc = send(sockfd, argv[1], strlen(argv[1]) + 1, 0);
    DIE(rc < 0, "send");

    // dezactivam algo Nagle
    int ok = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&ok, sizeof(int));

    // adaugam file descriptorii de la inceput (sockfd si stdin)
    struct pollfd poll_fds[MAX_CONNECTIONS];
    poll_fds[0].fd = sockfd;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = 0;
    poll_fds[1].events = POLLIN;
    int num_conn = 2;

    while (1) {
        rc = poll(poll_fds, num_conn, -1);
        DIE(rc < 0, "poll");
        for (int i = 0; i < num_conn; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == 0) {
                    // citesc de la stdin, trimit catre dest
                    char buf[BUFLEN];
                    memset(buf, 0, BUFLEN);
                    fgets(buf, BUFLEN - 1, stdin);
                    // daca primit comanda de exit, inchid socket-ul si ies din client
                    if (strncmp(buf, "exit", 4) == 0) {
                        close(sockfd);
                        return 0;
                        // daca primesc subscribe/unsubscribe de la stdin, trimit comanda completa catre server
                    } else {
                        rc = send(sockfd, buf, strlen(buf), 0);
                        DIE(rc < 1, "send error");
                        if (strncmp(buf, "subscribe", strlen("subscribe")) == 0) {
                            printf("Subscribed to topic.\n");
                        } else if (strncmp(buf, "unsubscribe", strlen("unsubscribe")) == 0) {
                            printf("Unsubscribed from topic.\n");
                        }
                    }
                } else {
                    char buf[BUFLEN];
                    memset(buf, 0, BUFLEN);
                    rc = recv(sockfd, buf, sizeof(buf), 0);
                    DIE(rc < 0, "esec mare\n");
                    // daca primesc mesaje de iesire de la server, ies si din client
                    if (rc == 0 || strncmp(buf, "exit", 4) == 0 || strcmp(buf, "terminat") == 0) {
                        close(sockfd);
                        return 0;
                    // altfel, daca primesc mesaje de la client, le printez.
                    } else {
                        printf("%s\n", buf);
                    }
                }
            }
        }

    }
close(sockfd);
return 0;
}