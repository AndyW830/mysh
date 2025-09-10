#ifndef _SERVER_H
#define _SERVER_H

#include <sys/types.h>

#define BUFFER_SIZE 1024

typedef struct ClientNode {
    int socket;
    int id;
    struct ClientNode *next;
} ClientNode;

extern ClientNode *clients;
extern int client_counter;
extern int server_running;

ssize_t add_client(int client_socket);
ssize_t remove_client(int client_socket);
ssize_t close_server();
ssize_t send_message(const char *hostname, int port, const char *message);
ssize_t start_client(int port, const char *hostname);
ssize_t start_server(int port);
ssize_t bn_send_msg(char **tokens);

#endif // SERVER_CLIENT_H