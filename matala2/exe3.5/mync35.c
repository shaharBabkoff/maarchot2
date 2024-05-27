#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>

void start_server(int port, const char *client_host, int client_port, int mode);
void start_basic_server(int port, int mode);
void *relay_input(void *sockets);

typedef struct {
    int source;
    int destination;
} relay_sockets_t;

int main(int argc, char *argv[]) {
    char *port = NULL;
    char *client_host = NULL;
    char *client_port = NULL;
    int mode = 0; // 1 for input, 2 for output, 3 for both, 4 for input from client and output to server

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-i") == 0) {
            if (mode == 2) {
                mode = 4;
            } else {
                mode = 1;
            }
        } else if (strcmp(argv[i], "-o") == 0) {
            if (mode == 1) {
                mode = 4;
            } else {
                mode = 2;
            }
        } else if (strcmp(argv[i], "-b") == 0) {
            mode = 3;
        } else if (strncmp(argv[i], "TCPS", 4) == 0) {
            port = argv[i] + 4;
        } else if (strncmp(argv[i], "TCPC", 4) == 0) {
            char *sep = strchr(argv[i] + 4, ',');
            if (sep) {
                client_host = argv[i] + 4;
                *sep = '\0';
                client_port = sep + 1;
            } else {
                fprintf(stderr, "Error: invalid TCPC format\n");
                exit(EXIT_FAILURE);
            }
        } else {
            fprintf(stderr, "Error: invalid argument %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    if (client_host && client_port && port) {
        start_server(atoi(port), client_host, atoi(client_port), mode);
    } else if (port) {
        start_basic_server(atoi(port), mode);
    } else {
        fprintf(stderr, "Error: missing TCP specification\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void start_basic_server(int port, int mode) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow the socket to be reused
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    if (mode == 3) {
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, relay_input, (void *)&new_socket);

        char buffer[1024];
        int bytes_read;
        while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
            write(new_socket, buffer, bytes_read);
        }

        pthread_join(thread_id, NULL);
    } else if (mode == 1) {
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, relay_input, (void *)&new_socket);
        pthread_join(thread_id, NULL);
    } else if (mode == 2) {
        relay_sockets_t sockets = { STDIN_FILENO, new_socket };
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, relay_input, (void *)&sockets);
        pthread_join(thread_id, NULL);
    }

    close(new_socket);
    close(server_fd);
}

void start_server(int port, const char *client_host, int client_port, int mode) {
    int server_fd, new_socket, client_socket;
    struct sockaddr_in server_address, client_serv_addr;
    int opt = 1;
    int addrlen = sizeof(server_address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow the socket to be reused
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    if ((new_socket = accept(server_fd, (struct sockaddr *)&server_address, (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    // Connect to the client server
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        close(new_socket);
        close(server_fd);
        return;
    }

    client_serv_addr.sin_family = AF_INET;
    client_serv_addr.sin_port = htons(client_port);

    if (strcmp(client_host, "localhost") == 0) {
        client_serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    } else {
        if (inet_pton(AF_INET, client_host, &client_serv_addr.sin_addr) <= 0) {
            printf("\nInvalid address/ Address not supported \n");
            close(new_socket);
            close(client_socket);
            close(server_fd);
            return;
        }
    }

    printf("Connecting to client server at %s:%d\n", client_host, client_port);

    if (connect(client_socket, (struct sockaddr *)&client_serv_addr, sizeof(client_serv_addr)) < 0) {
        printf("\nConnection to client server failed \n");
        close(new_socket);
        close(client_socket);
        close(server_fd);
        return;
    }

    printf("Connected to client server at %s:%d\n", client_host, client_port);

    if (mode == 4) {
        relay_sockets_t sockets1 = { new_socket, client_socket };
        pthread_t thread_id1;
        pthread_create(&thread_id1, NULL, relay_input, (void *)&sockets1);
        pthread_join(thread_id1, NULL);
    } else if (mode == 1) {
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, relay_input, (void *)&new_socket);
        pthread_join(thread_id, NULL);
    } else if (mode == 2) {
        relay_sockets_t sockets = { STDIN_FILENO, client_socket };
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, relay_input, (void *)&sockets);
        pthread_join(thread_id, NULL);
    } else if (mode == 3) {
        relay_sockets_t sockets1 = { new_socket, client_socket };
        pthread_t thread_id1;
        pthread_create(&thread_id1, NULL, relay_input, (void *)&sockets1);
        pthread_join(thread_id1, NULL);
    }

    close(new_socket);
    close(client_socket);
    close(server_fd);
}

void *relay_input(void *sockets) {
    relay_sockets_t *socks = (relay_sockets_t *)sockets;
    char buffer[1024];
    int bytes_read;

    while ((bytes_read = read(socks->source, buffer, sizeof(buffer))) > 0) {
        write(socks->destination, buffer, bytes_read);
    }

    return NULL;
}
