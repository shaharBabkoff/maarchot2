#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>

void process_mode4(int port, const char *program, int mode, const char *client_host, int client_port);
void process_mode123(int port, const char *program, int mode);
void run_program(const char *program);
void printErrorAndExit(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
    char *port = NULL;
    char *client_host = NULL;
    char *client_port = NULL;
    char *program = NULL;
    int mode = 0; // 1 for input, 2 for output, 3 for both, 4 for input from client and output to server

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-i") == 0)
        {
            mode = 1;
        }
        else if (strcmp(argv[i], "-o") == 0)
        {
            if (mode == 1)
            {
                mode = 4;
            }
            else
            {
                mode = 2;
            }
        }
        else if (strcmp(argv[i], "-b") == 0)
        {
            mode = 3;
        }
        else if (strcmp(argv[i], "-e") == 0)
        {
            if (i + 1 < argc)
            {
                program = argv[++i];
            }
            else
            {
                fprintf(stderr, "Error: missing argument for -e\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (strncmp(argv[i], "TCPS", 4) == 0)
        {
            port = argv[i] + 4;
        }
        else if (strncmp(argv[i], "TCPC", 4) == 0)
        {
            char *sep = strchr(argv[i] + 4, ',');
            if (sep)
            {
                client_host = argv[i] + 4;
                *sep = '\0';
                client_port = sep + 1;
            }
            else
            {
                fprintf(stderr, "Error: invalid TCPC format\n");
                exit(EXIT_FAILURE);
            }
        }
        else
        {
            fprintf(stderr, "Error: invalid argument %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    if (!program)
    {
        fprintf(stderr, "Error: program not specified\n");
        exit(EXIT_FAILURE);
    }

    if (client_host && client_port && port)
    {
        process_mode4(atoi(port), program, mode, client_host, atoi(client_port));
    }
    else if (port)
    {
        process_mode123(atoi(port), program, mode);
    }
    else
    {
        fprintf(stderr, "Error: missing TCP specification\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void process_mode123(int port, const char *program, int mode)
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow the socket to be reused
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(server_fd);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
    {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    // Use a switch statement to handle the mode and set up dup2 accordingly
    switch (mode) // 1 for input, 2 for output, 3 for both, 4 for input from client and output to server
    {
    case 1:
        dup2(new_socket, STDIN_FILENO);
        break;
    case 2:
        dup2(new_socket, STDOUT_FILENO);
        break;
    case 3:
        dup2(new_socket, STDIN_FILENO);
        dup2(new_socket, STDOUT_FILENO);
        break;
    default:
        fprintf(stderr, "Error: invalid mode\n");
        close(new_socket);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Split the program string into program name and arguments
    run_program(program);
    printf("going to close sockets\n");
    close(new_socket);
    close(server_fd);
    printf("done closing sockets\n");
}

void process_mode4(int port, const char *program, int mode, const char *client_host, int client_port)
{
    int server_fd, new_socket, client_socket;
    struct sockaddr_in server_address, client_serv_addr;
    int opt = 1;
    int addrlen = sizeof(server_address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow the socket to be reused
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0)
    {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);

    if ((new_socket = accept(server_fd, (struct sockaddr *)&server_address, (socklen_t *)&addrlen)) < 0)
    {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    // Connect to the client server
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        close(new_socket);
        close(server_fd);
        return;
    }

    client_serv_addr.sin_family = AF_INET;
    client_serv_addr.sin_port = htons(client_port);
    if (strcmp(client_host, "localhost") == 0)
    {
        printf("client host is localhost\n");
        client_serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    else
    {
        printf("client host is NOT localhost\n");
        if (inet_pton(AF_INET, client_host, &client_serv_addr.sin_addr) <= 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            close(new_socket);
            close(client_socket);
            close(server_fd);
            return;
        }
    }
    printf("Connecting to client server at %s:%d\n", client_host, client_port);

    if (connect(client_socket, (struct sockaddr *)&client_serv_addr, sizeof(client_serv_addr)) < 0)
    {
        printf("\nConnection to client server failed \n");
        close(new_socket);
        close(client_socket);
        close(server_fd);
        return;
    }

    printf("Connected to client server at %s:%d\n", client_host, client_port);

    // Use a switch statement to handle the mode and set up dup2 accordingly
    if (mode == 4)
    {
        dup2(new_socket, STDIN_FILENO);     // Input from client
        dup2(client_socket, STDOUT_FILENO); // Output to server
    }
    else
    {
        fprintf(stderr, "Error: invalid mode\n");
        close(new_socket);
        close(client_socket);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    // switch (mode)
    // {
    // case 1:
    //     dup2(client_socket, STDOUT_FILENO); // Output to client
    //     dup2(new_socket, STDIN_FILENO);     // Input from server
    //     break;
    // case 2:
    //     dup2(client_socket, STDOUT_FILENO); // Output to server
    //     break;
    // case 3:
    //     dup2(new_socket, STDIN_FILENO);     // Input from client
    //     dup2(client_socket, STDOUT_FILENO); // Output to server
    //     break;
    // case 4:
    //     dup2(new_socket, STDIN_FILENO);     // Input from client
    //     dup2(client_socket, STDOUT_FILENO); // Output to server
    //     break;
    // default:
    //     fprintf(stderr, "Error: invalid mode\n");
    //     close(new_socket);
    //     close(client_socket);
    //     close(server_fd);
    //     exit(EXIT_FAILURE);
    // }

    // Split the program string into program name and arguments
    run_program(program);
    printf("going to close sockets\n");
    close(new_socket);
    close(client_socket);
    close(server_fd);
    printf("done closing sockets\n");
}
void run_program(const char *program)
{
    char *args[10];
    int i = 0;
    char *token = strtok(strdup(program), " ");
    while (token != NULL && i < 9)
    {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;

    // execvp(args[0], args);

    // perror("execvp");
    pid_t pid = fork(); // יוצרים תהליך ילד
    if (pid == -1)
    {
        // קריאה ל-fork נכשלה
        printErrorAndExit("fork");
    }
    else if (pid == 0)
    {
        printf("in child process, going to execute command\n");
        // זהו הקוד שמתבצע בתהליך הילד
        if (execvp(args[0], args) == -1)
        {
            printf("in child process, execvp returned -1\n");
            printErrorAndExit("execvp");
        }
        printf("in child process, done executing\n");
    }
    else
    {
        printf("in parent process, waiting for child to end\n");
        // זהו הקוד שמתבצע בתהליך האב
        if (wait(NULL) == -1)
        {
            printErrorAndExit("wait");
        }
        printf("in parent process, return from wait\n");
    }
}
