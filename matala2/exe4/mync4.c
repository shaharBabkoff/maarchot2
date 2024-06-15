#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>

// forward declarations
void run_program(const char *program, int udp_server_sock, int udp_client_sock, struct sockaddr_in *udp_server_addr, int tcp_server_sock, int tcp_client_sock);
void read_and_write(int source, int destination);
void run_chat(int udp_server_sock, int udp_client_sock, struct sockaddr_in *udp_server_addr, struct sockaddr_in *udp_client_addr, int tcp_server_sock, int tcp_client_sock, char *buffer, ssize_t buffer_size, ssize_t buffer_content);
void process(int tcp_port, char *tcp_client_host, int tcp_client_port, int udp_port, char *udp_client_host, int udp_client_port, char *program, int mode, int tcpmuxs);

// variable indicating a timeout has occured
volatile sig_atomic_t timeout_expired = 0;

// method called when a timeout occurs
void handle_alarm(int sig)
{
    timeout_expired = 1;
}

// method to print a message and exit the process due to an error
void printErrorAndExit(const char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}

// method to create and return a tcp client socket connected to the specified host and port
int connet_tcp_client(const char *client_host, int client_port)
{

    struct sockaddr_in client_serv_addr;

    int client_socket = 0;
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    client_serv_addr.sin_family = AF_INET;
    client_serv_addr.sin_port = htons(client_port);

    if (strcmp(client_host, "localhost") == 0)
    {
        client_serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    }
    else
    {
        if (inet_pton(AF_INET, client_host, &client_serv_addr.sin_addr) <= 0)
        {
            printf("\nInvalid address/ Address not supported \n");
            close(client_socket);
            return -1;
        }
    }

    printf("Connecting to client server at %s:%d\n", client_host, client_port);

    if (connect(client_socket, (struct sockaddr *)&client_serv_addr, sizeof(client_serv_addr)) < 0)
    {
        printf("\nConnection to client server failed \n");
        close(client_socket);
        return -1;
    }

    printf("Connected to server at %s:%d\n", client_host, client_port);

    return client_socket;
}

// method to bind a tcp listner to the specified port
int bind_tcp_server(int port)
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;

    printf("bind_server %d\n", port);
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
    return server_fd;
}

// method to read from a straem file descriptor and write to a stream file descriptor
void read_and_write(int src_fd, int dest_fd)
{
    char buffer[1024];
    int bytes_read;
    // fflush(stdout);
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0)
    {
        write(dest_fd, buffer, bytes_read);
    }
}

// method to read from a straem file descriptor and write to a datagram file descriptor
void read_and_sendto(int src_fd, int dest_dgram_fd, struct sockaddr_in *client_addr)
{
    char buffer[1024];
    int bytes_read;

    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0)
    {
        sendto(dest_dgram_fd, buffer, bytes_read, 0, (struct sockaddr *)client_addr, sizeof(*client_addr));
    }
}

// method to read from an input datagram file descriptor and write to an output straem file descriptor
// methos receives an optional initial content to write to the output fd
void recvfrom_and_write(int src_dgram_fd, char *buffer, ssize_t buffer_size, ssize_t buffer_content, int dest_fd)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while (1)
    {
        if (buffer_content == -1)
        {
            break;
        }

        if (buffer_content > 0)
        {
            write(dest_fd, buffer, buffer_content);
        }
        buffer_content = recvfrom(src_dgram_fd, buffer, buffer_size, 0, (struct sockaddr *)&client_addr, &addr_len);
    }
}

// method to read from an input datagram file descriptor and write to an output datagram file descriptor
// method receives an optional initial content to write to the output fd
void recvfrom_and_sendto(int src_dgram_fd, char *buffer, ssize_t buffer_size, ssize_t buffer_content, int dest_dgram_fd, struct sockaddr_in *dest_addr)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while (1)
    {
        if (buffer_content == -1)
        {
            break;
        }

        if (buffer_content > 0)
        {
            sendto(dest_dgram_fd, buffer, buffer_content, 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr));
        }
        buffer_content = recvfrom(src_dgram_fd, buffer, buffer_size, 0, (struct sockaddr *)&client_addr, &addr_len);
        printf("received %ld from socket\n", buffer_content);
    }
}

// create initialize and return a udp server socket bound to the given port
int start_udp_server(int port)
{
    int server_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sock < 0)
    {
        printErrorAndExit("Error creating socket");
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Error binding socket");
        close(server_sock);
        exit(EXIT_FAILURE);
    }

    return server_sock;
}

// create initialize a client socket and sockaddr_in structure for the given host and port
int start_udp_client(char *hostname, int port, struct sockaddr_in *server_addr)
{
    int client_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_sock < 0)
    {
        printErrorAndExit("Error creating socket");
    }

    struct hostent *server = gethostbyname(hostname);
    if (server == NULL)
    {
        printf("could not resolve hostname %s\n", hostname);
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(port);
    memcpy(&server_addr->sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    return client_sock;
}

// main method:
// 1. parse input and set variables with the given process arguments
// 2. set an alarm if -t option was given
// 3. call main process method
int main(int argc, char *argv[])
{
    char *tcp_port = NULL;
    char *tcp_client_host = NULL;
    char *tcp_client_port = NULL;
    char *udp_port = NULL;
    char *udp_client_host = NULL;
    char *udp_client_port = NULL;
    char *program = NULL;
    int tcpmuxs = 0;
    int timeout = 0;

    int mode = 0; // 1 for input, 2 for output, 3 for both, 4 for input from client and output to server

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-t") == 0)
        {
            timeout = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "-i") == 0)
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
        else if (strncmp(argv[i], "TCPMUXS", 7) == 0)
        {
            tcp_port = argv[i] + 7;
            tcpmuxs = 1;
        }
        else if (strncmp(argv[i], "TCPS", 4) == 0)
        {
            tcp_port = argv[i] + 4;
        }
        else if (strncmp(argv[i], "TCPC", 4) == 0)
        {
            char *sep = strchr(argv[i] + 4, ',');
            if (sep)
            {
                tcp_client_host = argv[i] + 4;
                *sep = '\0';
                tcp_client_port = sep + 1;
            }
            else
            {
                fprintf(stderr, "Error: invalid TCPC format\n");
                exit(EXIT_FAILURE);
            }
        }

        else if (strncmp(argv[i], "UDPS", 4) == 0)
        {
            udp_port = argv[i] + 4;
        }
        else if (strncmp(argv[i], "UDPC", 4) == 0)
        {
            printf("UDPC\n");
            char *sep = strchr(argv[i] + 4, ',');
            if (sep)
            {
                udp_client_host = argv[i] + 4;
                *sep = '\0';
                udp_client_port = sep + 1;
                printf("UDPC: %s:%s\n", udp_client_host, udp_client_port);
            }
            else
            {
                fprintf(stderr, "Error: invalid UDPC format\n");
                exit(EXIT_FAILURE);
            }
        }

        else
        {
            fprintf(stderr, "Error: invalid argument %s\n", argv[i]);
            exit(EXIT_FAILURE);
        }
    }

    if (timeout > 0)
    {
        signal(SIGALRM, handle_alarm);
        alarm(timeout);
    }

    process(
        tcp_port ? atoi(tcp_port) : 0,
        tcp_client_host,
        tcp_client_port ? atoi(tcp_client_port) : 0,
        udp_port ? atoi(udp_port) : 0,
        udp_client_host,
        udp_client_port ? atoi(udp_client_port) : 0,
        program,
        mode,
        tcpmuxs);

    return 0;
}

// method to
// 1. setup network connectivity (input/output tcp/udp)
// 2. for UDP input opttion, wait for a client to send some data so that the clients address can be captured
// 3. if program was given (-e option), call the run_program method. if TCPMUXS option was given, keep on calling
//    run_program in a child process for every new client that connects
// 4. if no program was given, call the run_chat method
void process(
    int tcp_port,
    char *tcp_client_host,
    int tcp_client_port,
    int udp_port,
    char *udp_client_host,
    int udp_client_port,
    char *program,
    int mode,
    int tcpmuxs)
{
    // processing will commence with a child process, the parent process will wait for the child process or a timeout (if -t option was given)
    pid_t pid_process = fork();
    if (pid_process == -1)
    {
        printErrorAndExit("fork");
    }
    else if (pid_process == 0)
    {

        struct sockaddr_in server_addr, client_addr, address;
        socklen_t addr_len;
        int addrlen = sizeof(address);
        char buffer[1024];
        int n = 0;

        int tcp_server_fd = 0;
        int tcp_server_sock = 0;
        int tcp_client_sock = 0;
        int udp_server_sock = 0;
        int udp_client_sock = 0;
        // -i option with TCPS
        if (tcp_port > 0)
        {
            printf("going to call bind_tcp_server\n");
            tcp_server_fd = bind_tcp_server(tcp_port);
            if ((tcp_server_sock = accept(tcp_server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
            {
                perror("accept");
                close(tcp_server_fd);
                exit(EXIT_FAILURE);
            }
        }
        // -o option with TCPC
        if (tcp_client_host != NULL && tcp_client_port > 0)
        {
            tcp_client_sock = connet_tcp_client(tcp_client_host, tcp_client_port);
        }
        // -i option with UDPS
        if (udp_port > 0)
        {
            udp_server_sock = start_udp_server(udp_port);
            printf("started server on %d, server_sock: %d\n", udp_port, udp_server_sock);
        }
        // -o option with UDPC
        if (udp_client_host != NULL && udp_client_port > 0)
        {
            printf("UDPC going to start\n");
            udp_client_sock = start_udp_client(udp_client_host, udp_client_port, &server_addr);
        }

        // in case of -i UDPS, we wait for a UDP client to send something so that we can obtain the client address and subsequently
        // transmit data to that client
        if (udp_server_sock > 0)
        {
            addr_len = sizeof(client_addr);
            printf("waiting for a client to connect and transmit some data\n");
            n = recvfrom(udp_server_sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr, &addr_len);
            if (n == -1)
            {
                printErrorAndExit("recvfrom");
            }
        }
        if (program)
        {
            do
            {
                pid_t pidmux = fork();
                if (pidmux == -1)
                {
                    printErrorAndExit("fork");
                }
                else if (pidmux == 0)
                {
                    printf("going to execute program\n");
                    run_program(
                        program,
                        udp_server_sock,
                        mode == 3 ? udp_server_sock : udp_client_sock,
                        mode == 3 ? &client_addr : &server_addr,
                        tcp_server_sock,
                        mode == 3 ? tcp_server_sock : tcp_client_sock);
                    close(tcp_server_sock);
                    printf("done run_program, going to exit\n");
                    return;
                }
                else
                {
                    printf("created child process to run_program %d\n", pidmux);
                    if (tcpmuxs)
                    {
                        close(tcp_server_sock);
                        tcp_server_sock = 0;
                        printf("waiting for another client\n");
                        if ((tcp_server_sock = accept(tcp_server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0)
                        {
                            perror("accept");
                            close(tcp_server_fd);
                            exit(EXIT_FAILURE);
                        }
                        printf("another client connected\n");
                    }
                    else
                    {
                        waitpid(pidmux, NULL, 0);
                        printf("child process to run_program: return from waitpid\n");
                    }
                }

            } while (tcpmuxs);
        }
        else
        {
            // run_chat_mixed(udp_server_sock, udp_client_sock, &server_addr, tcp_server_sock, tcp_client_sock);
            run_chat(
                udp_server_sock,
                mode == 3 ? udp_server_sock : udp_client_sock,
                mode == 3 ? &client_addr : &server_addr,
                &client_addr,
                tcp_server_sock,
                mode == 3 ? tcp_server_sock : tcp_client_sock,
                buffer,
                sizeof(buffer),
                n);
        }
    }
    else
    {
        printf("created child process to process %d\n", pid_process);
        while ((waitpid(pid_process, NULL, WNOHANG)) == 0)
        {
            if (timeout_expired)
            {
                printf("Timeout expired\n");
                break;
            }
            sleep(1);
        }

        printf("in parent process, return from wait\n");
        kill(0, SIGTERM);
    }
}

void run_chat(
    int udp_server_sock,
    int udp_client_sock,
    struct sockaddr_in *udp_server_addr,
    struct sockaddr_in *udp_client_addr,
    int tcp_server_sock,
    int tcp_client_sock,
    char *buffer,
    ssize_t buffer_size,
    ssize_t buffer_content)
{
    pid_t pid = fork();
    if (pid == -1)
    {
        printErrorAndExit("fork");
    }
    else if (pid == 0)
    {
        // child process to read from and write to one party of the chat

        // ./mync -i UDPS6060
        if (udp_server_sock > 0 && udp_client_sock == 0 && tcp_client_sock == 0)
        {
            read_and_sendto(STDIN_FILENO, udp_server_sock, udp_client_addr);
        }
        // ./mync -o UDPClocalhost,5050
        else if (udp_server_sock == 0 && udp_client_sock > 0 && tcp_server_sock == 0)
        {
            read_and_sendto(STDIN_FILENO, udp_client_sock, udp_server_addr);
        }
        // ./mync -i UDPS6060 -o UDPClocalhost,5050
        else if (udp_server_sock > 0 && udp_client_sock > 0 && udp_server_sock != udp_client_sock)
        {
            recvfrom_and_sendto(udp_client_sock, buffer, buffer_size, 0, udp_server_sock, udp_client_addr);
        }
        // ./mync -i TCPS6060
        else if (tcp_server_sock > 0 && udp_client_sock == 0 && tcp_client_sock == 0)
        {
            read_and_write(tcp_server_sock, STDOUT_FILENO);
        }
        // ./mync -o TCPClocalhost,5050
        else if (tcp_server_sock == 0 && udp_server_sock == 0 && tcp_client_sock > 0)
        {
            read_and_write(STDIN_FILENO, tcp_client_sock);
        }
        // ./mync -i TCPS6060 -o TCPClocalhost,5050
        else if (tcp_server_sock > 0 && tcp_client_sock > 0)
        {
            read_and_write(tcp_server_sock, tcp_client_sock);
        }
        // ./mync -i TCPS6060 -o UDPClocalhost,5050
        else if (tcp_server_sock > 0 && udp_client_sock > 0)
        {
            read_and_sendto(tcp_server_sock, udp_client_sock, udp_server_addr);
        }
        // ./mync -i UDPS6060 -o TCPClocalhost,5050
        else if (udp_server_sock > 0 && tcp_client_sock > 0)
        {
            recvfrom_and_write(udp_server_sock, buffer, buffer_size, 0, tcp_client_sock);
        }
    }
    else
    {
        // parent process to read from and write to the other party of the chat

        // ./mync -i UDPS6060
        if (udp_server_sock > 0 && udp_client_sock == 0 && tcp_client_sock == 0)
        {
            recvfrom_and_write(udp_server_sock, buffer, buffer_size, buffer_content, STDOUT_FILENO);
        }
        // ./mync -o UDPClocalhost,5050
        else if (udp_server_sock == 0 && udp_client_sock > 0 && tcp_server_sock == 0)
        {
            recvfrom_and_write(udp_client_sock, buffer, buffer_size, 0, STDOUT_FILENO);
        }
        // ./mync -i UDPS6060 -o UDPClocalhost,5050 or ./mync -b UDPS6060
        else if (udp_server_sock > 0 && udp_client_sock > 0)
        {
            recvfrom_and_sendto(udp_server_sock, buffer, buffer_size, buffer_content, udp_client_sock, udp_server_addr);
        }
        // ./mync -i TCPS6060
        else if (tcp_server_sock > 0 && udp_client_sock == 0 && tcp_client_sock == 0)
        {
            read_and_write(STDIN_FILENO, tcp_server_sock);
        }
        // ./mync -o TCPClocalhost,5050
        else if (tcp_server_sock == 0 && udp_server_sock == 0 && tcp_client_sock > 0)
        {
            read_and_write(tcp_client_sock, STDOUT_FILENO);
        }
        // ./mync -i TCPS6060 -o TCPClocalhost,5050
        else if (tcp_server_sock > 0 && tcp_client_sock > 0)
        {
            read_and_write(tcp_client_sock, tcp_server_sock);
        }
        // ./mync -i TCPS6060 -o UDPClocalhost,5050
        else if (tcp_server_sock > 0 && udp_client_sock > 0)
        {
            recvfrom_and_write(udp_client_sock, buffer, buffer_size, 0, tcp_server_sock);
        }
        // ./mync -i UDPS6060 -o TCPClocalhost,5050
        else if (udp_server_sock > 0 && tcp_client_sock > 0)
        {
            read_and_sendto(tcp_client_sock, udp_server_sock, udp_client_addr);
        }
    }
}

void run_program(const char *program, int udp_server_sock, int udp_client_sock, struct sockaddr_in *udp_server_addr, int tcp_server_sock, int tcp_client_sock)
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
    int out_pipe[2];
    int in_pipe[2];
    pid_t pid_output = -1;
    // create a pipe to capture STDOUT
    if ((udp_client_sock > 0 || tcp_client_sock > 0) && pipe(out_pipe) != 0)
    {
        exit(1);
    }
    // create a pipe to capture STDIN
    if ((udp_server_sock > 0 || tcp_server_sock > 0) && pipe(in_pipe) != 0)
    {
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        printErrorAndExit("fork");
    }
    else if (pid == 0)
    {
        // child process to execute the program
        setpgid(0, 0);
        if (udp_client_sock > 0 || tcp_client_sock > 0)
        {
            // we redirect the output of program to the writing side of the pipe
            close(out_pipe[0]);
            if (dup2(out_pipe[1], STDOUT_FILENO) == -1)
            {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            close(out_pipe[1]);
        }
        if (udp_server_sock > 0 || tcp_server_sock > 0)
        {
            // input will come from a connected client, redirect input to come from the reading side of the pipe
            if (dup2(in_pipe[0], STDIN_FILENO) == -1)
            {
                perror("dup2");
                exit(EXIT_FAILURE);
            }
            pid_t pid_input = fork();
            if (pid_input == -1)
            {
                printErrorAndExit("fork");
            }
            else if (pid_input == 0)
            {
                // child process to read from the connected client and write to STDIN in a loop
                close(in_pipe[0]);

                char buffer[1024];

                printf("waiting for a client to connect and transmit some data\n");
                if (udp_server_sock > 0)
                {
                    recvfrom_and_write(udp_server_sock, buffer, sizeof(buffer), 0, in_pipe[1]);
                }
                else if (tcp_server_sock > 0)
                {
                    read_and_write(tcp_server_sock, in_pipe[1]);
                }
            }
            else
            {
                printf("created child process to monitor input %d\n", pid_input);
            }
        }
        if (execvp(args[0], args) == -1)
        {
            printErrorAndExit("execvp");
        }
    }
    else
    {
        if (udp_client_sock > 0 || tcp_client_sock > 0)
        {
            pid_output = fork();
            if (pid_output == -1)
            {
                printErrorAndExit("fork");
            }
            else if (pid_output == 0)
            {
                // child process to read from the standard output and transmit to the output (-o option)
                close(out_pipe[1]);
                if (udp_client_sock > 0)
                {
                    read_and_sendto(out_pipe[0], udp_client_sock, udp_server_addr);
                }
                else if (tcp_client_sock > 0)
                {
                    read_and_write(out_pipe[0], tcp_client_sock);
                }
            }
            else
            {
                printf("output process %d\n", pid_output);
            }
        }
        else
        {
            printf("not udp_client_sock > 0 || tcp_client_sock > 0\n");
        }
        // wait for the program child process to exit
        waitpid(pid, NULL, 0);
        printf("executing process completed\n");
        if ((udp_client_sock > 0 || tcp_client_sock > 0))
        {
            close(out_pipe[0]);
            close(out_pipe[1]);
        }
        if ((udp_server_sock > 0 || tcp_server_sock > 0))
        {
            close(in_pipe[0]);
            close(in_pipe[1]);
        }

        // signal all child processes of pid to terminate
        kill(-pid, SIGTERM);
        if (pid_output != -1)
        {
            // signal pid_output to terminate
            kill(pid_output, SIGTERM);
        }
    }
}
