#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "io_helpers.h"

#define BUFFER_SIZE 1024
#define MAX_USER_MSG 128

typedef struct ClientNode {
    int socket;
    int id;
    char hostname[INET_ADDRSTRLEN];
    struct ClientNode *next;
} ClientNode;

int server_running = 0;
static int server_socket = -1;
static int server_port = -1;
static pid_t server_pid = -1;
static ClientNode *clients = NULL;
static int client_counter = 0;

// ======== Linked List Operations ========

ClientNode *add_client(int client_sock, const char *hostname) {
    ClientNode *new_client = malloc(sizeof(ClientNode));
    if (!new_client) {
        perror("malloc");
        return NULL;
    }

    new_client->socket = client_sock;
    new_client->id = ++client_counter;
    strncpy(new_client->hostname, hostname, INET_ADDRSTRLEN);
    new_client->next = clients;
    clients = new_client;

    return new_client;
}

void remove_client(int client_sock) {
    ClientNode **current = &clients;
    while (*current) {
        if ((*current)->socket == client_sock) {
            ClientNode *temp = *current;
            *current = (*current)->next;

            close(temp->socket);
            free(temp);
            break;
        }
        current = &(*current)->next;
    }
}

void cleanup_clients() {
    ClientNode *current = clients;
    while (current) {
        ClientNode *temp = current;
        current = current->next;
        close(temp->socket);
        free(temp);
    }
    clients = NULL;
    client_counter = 0;
}

// ======== Server Functions ========

void broadcast_message(char *msg) {
    display_message(msg);
    display_message("\n");
    
    ClientNode *current = clients;
    while (current) {
        if (send(current->socket, msg, strlen(msg), 0) < 0) {
            perror("send");
        }
        current = current->next;
    }
}

void run_server() {
    fd_set read_fds;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms timeout

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        int max_fd = server_socket;

        // Add all client sockets to fd_set
        ClientNode *current = clients;
        while (current) {
            FD_SET(current->socket, &read_fds);
            if (current->socket > max_fd) {
                max_fd = current->socket;
            }
            current = current->next;
        }

        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            perror("select");
            break;
        }

        // Check for new connections
        if (FD_ISSET(server_socket, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t addr_len = sizeof(client_addr);
            int new_socket = accept(server_socket, 
                                   (struct sockaddr *)&client_addr, 
                                   &addr_len);
            
            if (new_socket < 0) {
                perror("accept");
                break;
            }

            char client_host[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, 
                     client_host, INET_ADDRSTRLEN);
            
            if (!add_client(new_socket, client_host)) {
                close(new_socket);
                display_error("ERROR: ", "Failed to add client");
            }
        }

        // Check client activity
        current = clients;
        while (current) {
            if (FD_ISSET(current->socket, &read_fds)) {
                char buffer[BUFFER_SIZE] = {0};
                int bytes_read = recv(current->socket, buffer, BUFFER_SIZE, 0);
                if (bytes_read == BUFFER_SIZE) {
                    send(current->socket, "Warning: message too long and may be truncated.\n", 48, 0);
                }
                
                if (bytes_read <= 0) {
                    ClientNode *temp = current;
                    current = current->next;
                    remove_client(temp->socket);
                    continue;
                }

                // Handle special commands
                if (strncmp(buffer, "\\connected", 10) == 0) {
                    char msg[BUFFER_SIZE];
                    snprintf(msg, sizeof(msg), 
                            "Client %d: %d clients connected",
                            current->id, client_counter);
                    send(current->socket, msg, strlen(msg), 0);
                    current = current->next;
                    continue;
                }

                // Broadcast regular message
                char msg[BUFFER_SIZE + 20];
                snprintf(msg, sizeof(msg), "client %d: %s", 
                         current->id, buffer);
                broadcast_message(msg);
            }
            current = current->next;
        }
    }

    cleanup_clients();
    close(server_socket);
    exit(0);
}

// ======== Command Handlers ========

ssize_t start_server(int port) {
    server_running = 1;
    if (server_pid != -1) {
        display_error("ERROR: ", "Server already running");
        return -1;
    }

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        return -1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
                  &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_socket);
        return -1;
    }

    // Bind socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        display_error("ERROR: ", "Address already in use");
        close(server_socket);
        return -1;
    }

    // Listen
    if (listen(server_socket, SOMAXCONN) < 0) {
        perror("listen");
        close(server_socket);
        return -1;
    }

    // Make socket non-blocking
    fcntl(server_socket, F_SETFL, O_NONBLOCK);

    // Fork server process
    server_pid = fork();
    if (server_pid < 0) {
        perror("fork");
        close(server_socket);
        return -1;
    }

    if (server_pid == 0) {
        // Child process (server)
        run_server();
    }

    return 0;
}

ssize_t close_server() {
    if (server_pid == -1) {
        display_error("ERROR: ", "No server running");
        return -1;
    }

    if (kill(server_pid, SIGTERM) < 0) {
        perror("kill");
        return -1;
    }

    // Wait for server to exit
    int status;
    waitpid(server_pid, &status, 0);

    server_pid = -1;
    server_port = -1;
    cleanup_clients();
    close(server_socket);
    display_message("Server stopped\n");
    server_running = 0;

    return 0;
}

// ... (send_message and start_client functions remain the same as previous version)
ssize_t bn_send_msg(char **tokens) {
    if (tokens[1] == NULL) {
        display_error("ERROR: ", "No port provided");
        return -1;
    }

    if (tokens[2] == NULL) {
        display_error("ERROR: ", "No hostname provided");
        return -1;
    }

    if (tokens[3] == NULL) {
        display_error("ERROR: ", "No message provided");
        return -1;
    }

    char *endptr;
    long port = strtol(tokens[1], &endptr, 10);
    if (*endptr != '\0' || port <= 0 || port > 65535) {
        display_error("ERROR: ", "Invalid port number");
        return -1;
    }
    char msg_buf[MAX_USER_MSG + 3] = "";
    for (int i = 3; tokens[i] != NULL; i++) {
        if (strlen(msg_buf) + strlen(tokens[i]) + 2 >= MAX_USER_MSG) {
            display_error("ERROR: ", "Message too long");
            return -1;
        }
        strcat(msg_buf, tokens[i]);
        if (tokens[i + 1] != NULL) strcat(msg_buf, " ");
    }
    strcat(msg_buf, "\r\n");

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, tokens[2], &server_addr.sin_addr) <= 0) {
        display_error("ERROR: ", "Invalid hostname or IP");
        close(sock_fd);
        return -1;
    }

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock_fd);
        return -1;
    }

    if (send(sock_fd, msg_buf, strlen(msg_buf), 0) == -1) {
        display_error("ERROR: ", "Failed to send message");
        close(sock_fd);
        return -1;
    }

    close(sock_fd);
    return 0;
}

ssize_t start_client(int port, const char *hostname) {
    if (port <= 0 || port > 65535) {
        display_error("ERROR: ", "Invalid port number");
        return -1;
    }

    if (hostname == NULL || strlen(hostname) == 0) {
        display_error("ERROR: ", "Invalid hostname");
        return -1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        display_error("ERROR: ", "socket failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, hostname, &server_addr.sin_addr) <= 0) {
        display_error("ERROR: ", "Invalid hostname/IP address");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }


    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    int running = 1;

    while (running) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); 
        FD_SET(sock, &read_fds);          

        int max_fd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {

                running = 0;
                break;
            }


            buffer[strcspn(buffer, "\n")] = 0;

            if (strcmp(buffer, "\\connected") == 0) {
                if (send(sock, buffer, strlen(buffer), 0) < 0) {
                    perror("send");
                    break;
                }
                continue;
            }

            if (send(sock, buffer, strlen(buffer), 0) < 0) {
                perror("send");
                break;
            }
        }

        if (FD_ISSET(sock, &read_fds)) {
            int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes <= 0) {
                display_message("\nServer disconnected\n");
                running = 0;
                break;
            }
            buffer[bytes] = '\0';
            display_message(buffer);
            display_message("\n");
        }
    }

    close(sock);
    return 0;
}


