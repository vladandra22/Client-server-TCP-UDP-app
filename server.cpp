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

#include <cmath>
#include <vector>
#include <queue>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "helpers.h"

#define MAX_CONNECTIONS 1000
#define BUFLEN 1600

using namespace std;

struct client{
    int fd;
    char id_client[11];
    int online;
    vector<pair<string, int>> topics;
    queue<string> q; // Coada de mesaje, unde salvam mesajele cat a fost offline
};

int main(int argc, char *argv[]){

    vector<client> clients;

    // Dezactivam buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Parsam portul ca un nr
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "port invalid");

    // UDP sock file descriptor si TCP sock file descriptor
    int udp_sockfd, tcp_sockfd;
    struct sockaddr_in serv_addr, udp_addr;
    
    // SOCK_DGRAM = flux de date bidirectional
    // protocol = 0
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "fail creare socket udp");

    // SOCK_STREAM = comunicare FIFO
    tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcp_sockfd < 0, "fail creare socket tcp");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(port);

    // bind = leaga un socket de un port si o adresa
    rc = bind(tcp_sockfd, (const struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "bind");

    rc = bind(udp_sockfd, (const struct sockaddr *)&udp_addr, sizeof(udp_addr));
    DIE(rc < 0, "bind");

    // Dezactivam algo Nagle
    int ok = 1;
    setsockopt(tcp_sockfd,IPPROTO_TCP,TCP_NODELAY, &ok,sizeof(int));

    // listen pe socketul tcp
    rc = listen(tcp_sockfd, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

    struct pollfd poll_fds[MAX_CONNECTIONS];
    // File descriptorii mei actuali (voi adauga pe parcurs socket-urile clientilor)
    int num_clients = 3;
    poll_fds[0].fd = tcp_sockfd;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = 0;
    poll_fds[1].events = POLLIN;
    poll_fds[2].fd = udp_sockfd;
    poll_fds[2].events = POLLIN;
    
    while(1){
        rc = poll(poll_fds, num_clients, -1);
        DIE(rc < 0, "poll");
        for(int i = 0; i < num_clients; i++){
            if(poll_fds[i].revents & POLLIN){
                if(poll_fds[i].fd == 0) { //stdin
                    char buf[20];
                    fgets(buf, 20, stdin);
                    if(strncmp(buf, "exit", 4) == 0){
                        for(int j = 0; j < clients.size(); j++){
                            clients[j].online = 0;
                        }
                        close(tcp_sockfd);
                        close(udp_sockfd);
                        return 0;
                    }
                    else{
                       // printf("Invalid command\n");
                        for(int j = 0; j < clients.size(); j++){
                                clients[j].online = 0;
                            }
                        close(tcp_sockfd);
                        close(udp_sockfd);
                        return 0;
                    }
                }
                else if(poll_fds[i].fd == tcp_sockfd){
                    // Acceptam  noua conexiune
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    // accept = acceptam noua conexiune
                    int newsockfd = accept(tcp_sockfd, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept\n");


                    int ok2 = 1;
                    setsockopt(newsockfd,IPPROTO_TCP,TCP_NODELAY, &ok2,sizeof(int));

                    // primim id client
                    char id_curr_client[11];
                    memset(id_curr_client, 0, 11);
                    rc = recv(newsockfd, id_curr_client, sizeof(id_curr_client), 0);
                    DIE(rc < 0, "nu am primit id client");
                    
                    int gasit = 0, j;
                    for(j = 0; j < clients.size(); j++){
                        if(strcmp(clients[j].id_client, id_curr_client) == 0){
                            gasit = 1;
                            break;
                        }
                    }

                    // daca nu exista/ e primul, il adaugam in vector
                    if(clients.empty() || gasit == 0){
                        client new_client;
                        memset(&new_client, 0, sizeof(new_client));
                        new_client.fd = newsockfd;
                        strcpy(new_client.id_client, id_curr_client);
                        new_client.online = 1;
                        clients.push_back(new_client);
                        printf("New client %s connected from %s:%d.\n", new_client.id_client, inet_ntoa(cli_addr.sin_addr), port);
                        // adaug file descriptor-ul clientului in poll-ul nostru
                        poll_fds[num_clients].fd = newsockfd;
                        poll_fds[num_clients].events = POLLIN;
                        num_clients++;
                    }
                    // daca exista si el este online, inchidem socket-ul deschis de clientul ce incearca sa se conecteze
                    else if(gasit == 1){
                        if(clients[j].online == 1){
                            printf("Client %s already connected.\n", id_curr_client);
                            rc = send(newsockfd, "terminat", sizeof("terminat"), 0);
                            DIE(rc < 0, "client already connected fail\n");
                            close(newsockfd);
                        }
                        // daca exista si nu e online, il reconectam
                        else if(clients[j].online == 0){
                            clients[j].online = 1;
                            printf("New client %s connected from %s:%d.\n", clients[j].id_client, inet_ntoa(cli_addr.sin_addr), port);
                            // gasim socket-ul vechi al clientului si il inlocuim
                            int idx;
                            for(idx = 0; idx < num_clients; idx++){
                                if(poll_fds[idx].fd == clients[j].fd)
                                    break;
                            }
                            // setez noul socket primit la noul client.
                            clients[j].fd = newsockfd;
                            poll_fds[idx].fd = newsockfd;
                             while (!clients[j].q.empty()) {
                                    string msg = clients[j].q.front();
                                    msg = msg + "\n";
                                    clients[j].q.pop();
                                    rc = send(newsockfd, msg.c_str(), msg.size(), 0);
                                    DIE(rc < 0, "send history failed\n");
                                }

                        }
                    }
                    
                }
               else if (poll_fds[i].fd == udp_sockfd) {
                    // udp
                    char buf[BUFLEN];
                    memset(buf, 0, BUFLEN);
                    socklen_t udp_len = sizeof(udp_addr);
                    rc = recvfrom(udp_sockfd, buf, BUFLEN, 0, (struct sockaddr*)&udp_addr, &udp_len);
                    DIE(rc < 0, "fail from udp");
                    if (rc < 52) {
                        // nu am destule caractere primite, deci dau skip
                        continue;
                    }
                    char topic[51];
                    char type[20];
                    memset(type, 0, 20);
                    memset(topic, 0, 51);
                    memcpy(topic, buf, 50);
                    string udp_message;
                    stringstream build_udp_message;
                    // primim tipul de date pe 1 byte, unsigned int: cazul 0, 1, 2, 3
                    int data_type = buf[50];
                    switch (data_type) {
                        case 0: { // octet de semn + uint32_t = nr intreg fara semn
                            unsigned char sign_byte = buf[51];
                            int nr;
                            if (rc < 56) {
                                // nu sunt destule date primite, deci dau skip
                                continue;
                            }
                            uint32_t nr_buf;
                            memset(&nr_buf, 0, sizeof(uint32_t));
                            memcpy(&nr_buf, buf + 52, sizeof(uint32_t));
                            nr_buf= ntohl(nr_buf);
                            if (sign_byte == 1)
                                nr = nr_buf * (-1);
                            else 
                                nr = nr_buf;
                            memcpy(type, "INT", sizeof("INT"));
                            build_udp_message << inet_ntoa(udp_addr.sin_addr) << ":" << port << " - " << topic << " - " << type << " - " << nr;
                            udp_message = build_udp_message.str();
                            break;
                        }
                        case 1: { // short real - uint16_t repr nr inmultit cu 100
                            uint16_t nr_aux;
                            memset(&nr_aux, 0, sizeof(nr_aux));
                            memcpy(&nr_aux, buf + 51, sizeof(uint16_t));
                            float nr;
                            nr = (float)ntohs(nr_aux) / 100;
                            memcpy(type, "SHORT_REAL", sizeof("SHORT_REAL"));
                            build_udp_message << fixed << setprecision(2);
                            build_udp_message << inet_ntoa(udp_addr.sin_addr) << ":" << port << " - " << topic << " - " << type << " - " << nr;
                            udp_message = build_udp_message.str();
                            break;
                            }
                        case 2: { // float = octet semn + uint32_t + uint8_t
                            unsigned char sign_byte = buf[51];
                            uint32_t nr_aux;
                            memset(&nr_aux, 0, sizeof(uint32_t));
                            memcpy(&nr_aux, buf + 52, sizeof(uint32_t));
                            uint8_t exponent;
                            memset(&exponent, 0, sizeof(uint8_t));
                            memcpy(&exponent, buf + 56, sizeof(uint8_t));
                            float nr;
                            nr = ntohl(nr_aux) / powf(10, exponent);
                            if(sign_byte == 1)
                                nr *= (-1);
                            memcpy(type, "FLOAT", sizeof("FLOAT"));
                            build_udp_message << fixed << setprecision(4);
                            build_udp_message << inet_ntoa(udp_addr.sin_addr) << ":" << port << " - " << topic << " - " << type << " - " << nr;
                            udp_message = build_udp_message.str();
                            break;
                            }
                        case 3: { // sir de 1500 caractere, \n sau datagrame mai mici
                            char message[1501];
                            memset(message, 0, 1500);
                            memcpy(message, buf + 51, 1501);
                            memcpy(type, "STRING", sizeof("STRING"));
                            build_udp_message << inet_ntoa(udp_addr.sin_addr) << ":" << port << " - " << topic << " - " << type << " - " << message;
                            udp_message = build_udp_message.str();
                            break;
                        }
                        default: continue;
                    }
                    // caut in topicurile clientilor topicul primit de la udp.
                    // daca nu e online, adaug in coada.
                    for(int j = 0; j < (int)clients.size(); j++){
                        for(int k = 0; k < (int)clients[j].topics.size(); k++){
                            string client_topic = clients[j].topics[k].first;
                            int sf = clients[j].topics[k].second;
                            string this_topic(topic);
                            if(client_topic == this_topic)
                                if(clients[j].online == 1) {
                                    int rc = send(clients[j].fd, udp_message.c_str(), 1600, 0);
                                    DIE(rc < 0, "send message fail\n");
                                }
                                else if(clients[j].online == 0 && sf == 1){
                                    clients[j].q.push(udp_message);
                                }
                        }
                    }

                    }
                else {
                    // mesaj client TCP
                    char buf[BUFLEN];
                    memset(buf, 0, BUFLEN);
                    int rc = recv(poll_fds[i].fd, &buf, sizeof(buf), 0);
                    DIE(rc < 0, "recv");
                    client curr_client;
                    int index;
                    for(index = 0; index < (int)clients.size(); index++){
                        if(clients[index].fd == poll_fds[i].fd){
                            curr_client = clients[index];
                            break;
                             
                        }
                    }

                    // daca nu mai primim nimic, clientul a inchis conexiunea
                    if (rc == 0) {
                        // conexiunea s-a inchis
                        printf("Client %s disconnected.\n",  curr_client.id_client);
                        clients[index].online = 0;
                        close(poll_fds[i].fd);
                    }
                    else {
                        // verificam continutul mesajului.
                        // primim comenzi de tipul: subscribe <topic> <sf> sau unsubscribe <topic>
                            if (strncmp(buf, "subscribe", strlen("subscribe")) == 0) {
                                // verificam format ok pt mesajj
                                if (strlen(buf) < strlen("subscribe ") + 3) {
                                   // printf("Invalid subscribe message format. \n");
                                }
    
                                // extragem topic si sf
                                char topic[51];
                                memset(topic, 0, sizeof(topic));
                                int topic_length = strlen(buf) - strlen("subscribe ") - 3;
                                strncpy(topic, buf + strlen("subscribe "), strlen(buf) - strlen("subscribe ") - 3);
                                topic[topic_length] = '\0';
                                int sf = buf[strlen(buf) - 2] - '0';
                                
                                // verificam sa nu existe topicul si daca nu exista, il adaugam la client
                                string t = topic;
                                int topic_idx = -1;
                                for (int i = 0; i < (int)clients[index].topics.size(); i++) {
                                    if (clients[index].topics[i].first == t) {
                                        topic_idx = i;
                                        break;
                                    }
                                }
                                if(topic_idx != -1){
                                   // printf("Client already subscribed. \n");
                                }
                                else {
                                    clients[index].topics.push_back({t, sf});
                                    //printf("Client %s subscribed to topic %s %d\n", clients[index].id_client, t.c_str(), sf);
                                }
                            }
                            // verificam comanda de unsubscribe
                            else if (strncmp(buf, "unsubscribe", strlen("unsubscribe")) == 0) {
                                // verificam format ok mesaj
                                if (strlen(buf) < strlen("unsubscribe ") + 1) {
                                   // printf("Invalid unsubscribe message format. \n");
                                }
                                
                                // Extragem topic
                                char topic[51];
                                memset(topic, 0, sizeof(topic));
                                int topic_length = strlen(buf) - strlen("unsubscribe ") - 1;
                                strncpy(topic, buf + strlen("unsubscribe "), strlen(buf) - strlen("unsubscribe ") - 1);
                                topic[topic_length] = '\0';
                                // verificam sa nu mai existe topicul si daca exista, il stergem de la client
                                string t = topic;
                                int topic_idx = -1;
                                for (int i = 0; i < (int)clients[index].topics.size(); i++) {
                                    if (clients[index].topics[i].first == t) {
                                        topic_idx = i;
                                        break;
                                    }
                                }

                                if (topic_idx >= 0) {
                                    clients[index].topics.erase(clients[index].topics.begin() + topic_idx);
                                  //  printf("Client %s unsubscribed from topic %s. \n", clients[index].id_client, t.c_str());
                                }
                                else {
                                  //  printf("Client %s is not subscribed to topic %s. \n", clients[index].id_client, t.c_str());
                                }
                            }
                        }
                    }

            }
        }
        }
    close(tcp_sockfd);
    close(udp_sockfd);
    return 0;
}