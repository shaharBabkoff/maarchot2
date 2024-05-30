#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <getopt.h>

#define BUFFER_SIZE 1024
#define COMMAND_SIZE 2048

int timeout = 0;
int child_pid = 0;

void handle_alarm(int sig) {
    printf("Timeout occurred, exiting.\n");
    if (child_pid > 0) {
        kill(child_pid, SIGKILL);
    }
    exit(0);
}

void start_udp_server(int port, const char *program, int mode) {
    int sockfd;
    char buffer[BUFFER_SIZE];
    int pipe_in[2], pipe_out[2];
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;

    if (pipe(pipe_in) == -1 || pipe(pipe_out) == -1) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    len = sizeof(cliaddr);

    // Fork and execute the command
    if ((child_pid = fork()) == 0) {
        // Child process
        if (mode == 1) {
            dup2(pipe_in[0], STDIN_FILENO); // Input from pipe
            close(pipe_in[1]);
            close(pipe_out[0]);
            close(pipe_out[1]);
        } else if (mode == 2) {
            dup2(pipe_out[1], STDOUT_FILENO); // Output to pipe
            close(pipe_in[0]);
            close(pipe_in[1]);
            close(pipe_out[0]);
        } else {
            dup2(pipe_in[0], STDIN_FILENO);
            dup2(pipe_out[1], STDOUT_FILENO);
            close(pipe_in[1]);
            close(pipe_out[0]);
        }

        char *argv[] = {"/bin/sh", "-c", (char *)program, NULL};
        execvp(argv[0], argv);

        // If execvp fails
        perror("execvp failed");
        exit(EXIT_FAILURE);
    }

    // Parent process
    if (mode != 2) {
        close(pipe_in[0]);
    }
    if (mode != 1) {
        close(pipe_out[1]);
    }

    if (mode == 2 || mode == 3) {
        // Read the initial output from the process
        int n = read(pipe_out[0], buffer, BUFFER_SIZE);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Initial result from command: %s", buffer);
            sendto(sockfd, buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
        }
    }

    if (mode == 2) {
        // Mode 2: Output to client, input from stdin
        while (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
            // Remove newline character from input
            buffer[strcspn(buffer, "\n")] = 0;
            write(pipe_in[1], buffer, strlen(buffer));
            write(pipe_in[1], "\n", 1);

            // Read the result from the process
            int n = read(pipe_out[0], buffer, BUFFER_SIZE);
            if (n > 0) {
                buffer[n] = '\0';
                printf("Result from command: %s", buffer);
                sendto(sockfd, buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
            }
        }
    } else {
        // Mode 1 or 3
        while (1) {
            int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
            buffer[n] = '\0';
            printf("Received input from client: %s\n", buffer);

            // Send the client's input to the running process
            write(pipe_in[1], buffer, strlen(buffer));
            write(pipe_in[1], "\n", 1);

            if (mode != 1) {
                // Read the result from the process if mode is not 1
                n = read(pipe_out[0], buffer, BUFFER_SIZE);
                if (n > 0) {
                    buffer[n] = '\0';
                    printf("Result from command: %s", buffer);
                    sendto(sockfd, buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&cliaddr, len);
                }
            }
        }
    }

    close(pipe_in[1]);
    close(pipe_out[0]);
    wait(NULL); // Wait for child process to exit
}

void start_udp_client(char *hostname, int port) {
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in servaddr;
    struct hostent *server;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    memcpy(&servaddr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    servaddr.sin_port = htons(port);

    // Wait for initial output from the server
    int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, MSG_WAITALL, NULL, NULL);
    buffer[n] = '\0';
    printf("Server: %s\n", buffer);

    while (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
        // Remove newline character from input
        buffer[strcspn(buffer, "\n")] = 0;
        sendto(sockfd, buffer, strlen(buffer), 0, (const struct sockaddr *)&servaddr, sizeof(servaddr));
        
        n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, MSG_WAITALL, NULL, NULL);
        buffer[n] = '\0';
        printf("Server: %s\n", buffer);
    }
}

int main(int argc, char *argv[]) {
    int opt;
    char *input_mode = NULL;
    char *output_mode = NULL;
    char *exec_cmd = NULL;
    int mode = 3; // Default to both input and output

    while ((opt = getopt(argc, argv, "i:o:b:e:t:m:")) != -1) {
        switch (opt) {
            case 'i':
                input_mode = optarg;
                mode = 1; // Set mode to 1 if -i is used
                break;
            case 'o':
                output_mode = optarg;
                mode = 2; // Set mode to 2 if -o is used
                break;
            case 'b':
                input_mode = optarg;
                output_mode = optarg;
                mode = 3; // Set mode to 3 if -b is used
                break;
            case 'e':
                exec_cmd = optarg;
                break;
            case 't':
                timeout = atoi(optarg);
                signal(SIGALRM, handle_alarm);
                alarm(timeout);
                break;
            case 'm':
                mode = atoi(optarg);
                if (mode < 1 || mode > 3) {
                    fprintf(stderr, "Invalid mode. Use 1 for input, 2 for output, 3 for both.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            default:
                fprintf(stderr, "Usage: %s -i input_mode -o output_mode -b both_modes -e exec_cmd -t timeout -m mode\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (input_mode && strncmp(input_mode, "UDPS", 4) == 0) {
        int port = atoi(input_mode + 4);
        start_udp_server(port, exec_cmd, mode);
    } else if (output_mode && strncmp(output_mode, "UDPC", 4) == 0) {
        char *host_port = output_mode + 4;
        char *colon = strchr(host_port, ',');
        if (colon) {
            *colon = '\0';
            char *hostname = host_port;
            int port = atoi(colon + 1);
            start_udp_client(hostname, port);
        } else {
            fprintf(stderr, "Invalid UDP client format. Use UDPC<IP, PORT>\n");
            exit(0);
        }
    } else if (input_mode && strncmp(input_mode, "UDPS", 4) == 0 && output_mode) {
        // Handle both input and output modes if both are specified
        int port = atoi(input_mode + 4);
        start_udp_server(port, exec_cmd, mode);
        if (strncmp(output_mode, "UDPC", 4) == 0) {
            char *host_port = output_mode + 4;
            char *colon = strchr(host_port, ',');
            if (colon) {
                *colon = '\0';
                char *hostname = host_port;
                int port = atoi(colon + 1);
                start_udp_client(hostname, port);
            } else {
                fprintf(stderr, "Invalid UDP client format. Use UDPC<IP, PORT>\n");
                exit(0);
            }
        }
    }

    return 0;
}
