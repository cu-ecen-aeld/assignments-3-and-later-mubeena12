#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>

int sockfd, client_sockfd;

void cleanup() {
    // Close open sockets
    close(sockfd);
    close(client_sockfd);

    // Delete the file
    remove("/var/tmp/aesdsocketdata");

    // Close syslog
    closelog();

    // Exit
    exit(0);
}

void handle_signal(int sig) {
   if (sig == SIGINT || sig == SIGTERM) {
       syslog(LOG_INFO, "Caught signal, exiting");
       cleanup();
   }
}

void daemonize() {
    pid_t pid, sid;

    // Fork the parent process
    pid = fork();

    // Error occurred
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    // Success: Let the parent terminate
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // On success: The child process becomes session leader
    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }

    // Change the current working directory
    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    // Close out the standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main(int argc, char *argv[]) {
    int daemon_mode = 0;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    if (daemon_mode) {
        daemonize();
    }

    // Set up signal handlers
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Set up syslog
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        return -1;
    }

    // Bind to port 9000
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(9000);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        close(sockfd);
        return -1;
    }

    // Listen for connections
    if (listen(sockfd, 5) == -1) {
        perror("listen");
        close(sockfd);
        return -1;
    }

    // Accept connections in a loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        client_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sockfd == -1) {
            perror("accept");
            continue; // Continue accepting connections
        }

        // Log accepted connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Receive and process data
        size_t max_size = 24576;
        //char buffer[24576]; // Assuming max packet size is 1024
        char* buffer = (char *)malloc(max_size * sizeof(char));
        memset(buffer, 0, max_size * sizeof(char));
        ssize_t recv_size;
        while ((recv_size = recv(client_sockfd, buffer, sizeof(buffer), 0)) > 0) {
            // Append data to file
            FILE *file = fopen("/var/tmp/aesdsocketdata", "a");
            if (file != NULL) {
                fprintf(file, "%s", buffer);
                fclose(file);
            }

            // Send data back to client if a complete packet is received (ends with newline)
            if (strchr(buffer, '\n') != NULL) {
                FILE *read_file = fopen("/var/tmp/aesdsocketdata", "r");
                if (read_file != NULL) {
                    fseek(read_file, 0, SEEK_END);
                    long file_size = ftell(read_file);
                    fseek(read_file, 0, SEEK_SET);
                    fread(buffer, 1, file_size, read_file);
                    fclose(read_file);
                    send(client_sockfd, buffer, strlen(buffer), 0);
                }
            }

            memset(buffer, 0, max_size * sizeof(char));
        }

        free(buffer);

        // Log closed connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);

        close(client_sockfd);
    }

    // This part will only be reached on SIGINT or SIGTERM
    // Perform cleanup and exit
    cleanup();

    return 0;
}
