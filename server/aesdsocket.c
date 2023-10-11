/*
* Author: Mubeena Udyavar Kazi
* Course: ECEN 5713 - AESD
* Reference: 
*   1. ChatGPT with the following prompts.
*      - Example for socket based server to send and receive data in C
*      - Example in C to daemonize a process with signal handling
*   2. Stack overflow
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <syslog.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>

int sockfd, client_sockfd, datafd;

void cleanup(int exit_code) {

    // Close open sockets
    if (sockfd >= 0) close(sockfd);
    if (client_sockfd >=0) close(client_sockfd);

    // Close file descriptors
    if (datafd >= 0) close(datafd);

    // Delete the file
    remove("/var/tmp/aesdsocketdata");

    // Close syslog
    closelog();

    // Exit
    exit(exit_code);
}

void handle_signal(int sig) {
   if (sig == SIGINT || sig == SIGTERM) {
       syslog(LOG_INFO, "Caught signal, exiting");
       cleanup(EXIT_SUCCESS);
   }
}

void daemonize() {
    pid_t pid, sid;

    // Fork the parent process
    pid = fork();

    // Error occurred during fork
    if (pid < 0) {
        syslog(LOG_ERR, "ERROR: Failed to fork");
        cleanup(EXIT_FAILURE);
    }

    // Terminate parent process on successful fork
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    // Create a new SID for child process
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "ERROR: Failed to setsid");
        cleanup(EXIT_FAILURE);
    }

    // Change the current working directory
    if ((chdir("/")) < 0) {
        syslog(LOG_ERR, "ERROR: Failed to chdir");
        cleanup(EXIT_FAILURE);
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
        syslog(LOG_ERR, "ERROR: Failed to create socket");
        cleanup(EXIT_FAILURE);
    }

    // Allow for reuse of port 9000
    int enable_reuse = 1; // Set to 1 to enable reuse of port 9000
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable_reuse, sizeof(int)) == -1) {
        syslog(LOG_ERR, "ERROR: Failed to setsockopt");
        cleanup(EXIT_FAILURE);
    }

    // Bind to port 9000
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(9000);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "ERROR: Failed to bind");
        cleanup(EXIT_FAILURE);
    }

    close(-1);
    // Listen for connections
    if (listen(sockfd, 5) == -1) {
        syslog(LOG_ERR, "ERROR: Failed to listen");
        close(sockfd);
        return -1;
    }

    // Open file for aesdsocketdata
    char *aesddata_file = "/var/tmp/aesdsocketdata";
    datafd = open(aesddata_file, O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (datafd == -1){
        syslog(LOG_ERR, "ERROR: Failed to create file - %s", aesddata_file);
        closelog();
        exit(EXIT_FAILURE);
    }

    // Accept connections in a loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        client_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sockfd == -1) {
            syslog(LOG_WARNING, "WARNING: Failed to accept, retrying ...");
            continue; // Continue accepting connections
        }

        // Log accepted connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        // Receive and process data
        size_t buffer_size = 1024;
        char* buffer = (char *)malloc(buffer_size * sizeof(char));
        if (buffer == NULL) {
            syslog(LOG_ERR, "ERROR: Failed to malloc");
            cleanup(EXIT_FAILURE);
        }
        memset(buffer, 0, buffer_size * sizeof(char));
        ssize_t recv_size;
        while ((recv_size = recv(client_sockfd, buffer, buffer_size, 0)) > 0) {
            // Append data to file
            if (write(datafd, buffer, recv_size) == -1) {
                syslog(LOG_ERR, "ERROR: Failed to write to %s file", aesddata_file);
                cleanup(EXIT_FAILURE);
            }

            // Send data back to client if a complete packet is received (ends with newline)
            if (memchr(buffer, '\n', buffer_size) != NULL) {
                // Reset file offset to the beginning of the file
                lseek(datafd, 0, SEEK_SET);
                size_t bytes_read = read(datafd, buffer, buffer_size);
                if (bytes_read == -1) {
                    syslog(LOG_ERR, "ERROR: Failed to read from %s file", aesddata_file);
                    cleanup(EXIT_FAILURE);
                }
                while (bytes_read > 0) {
                    send(client_sockfd, buffer, bytes_read, 0);
                    bytes_read = read(datafd, buffer, buffer_size); 
                }
            }
            memset(buffer, 0, buffer_size * sizeof(char));
        }

        free(buffer);

        // Log closed connection
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        close(client_sockfd);
    }
    return 0;
}
