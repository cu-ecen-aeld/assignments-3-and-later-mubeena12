/*
* Author: Mubeena Udyavar Kazi
* Course: ECEN 5713 - AESD
* Reference: 
*   1. ChatGPT with the following prompts.
*      - Example for socket based server to send and receive data in C
*      - Example in C to daemonize a process with signal handling
*      - Example in C to use SLIST to manage pthreads
*      - Example in C to add timestamp in RFC 2822 compliant strftime format every 10 seconds using a thread
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
#include <pthread.h>
#include "queue.h"
#include <time.h> 
#include <errno.h>
#include "aesd_ioctl.h"

int sockfd, datafd;

int signal_exit = 0;

#if USE_AESD_CHAR_DEVICE == 1
static char *aesddata_file = "/dev/aesdchar";
#else
static char *aesddata_file = "/var/tmp/aesdsocketdata";
#endif

typedef struct  client_info
{
    int client_sockfd;
    char client_ip[INET_ADDRSTRLEN]; 
} client_info_t;


struct thread_info_t {
    pthread_t thread_id;
    int work_done;
    client_info_t client_data;
    SLIST_ENTRY(thread_info_t) entries;
};

SLIST_HEAD(thread_list_t, thread_info_t) thread_list;

#if USE_AESD_CHAR_DEVICE != 1
pthread_mutex_t aesddata_file_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

void cleanup(int exit_code) {

    syslog(LOG_INFO, "performing cleanup");
    signal_exit = 1;

    // Clean up threads
    struct thread_info_t *thread;
    while (!SLIST_EMPTY(&thread_list)) {
        thread = SLIST_FIRST(&thread_list);
        SLIST_REMOVE_HEAD(&thread_list, entries);
        syslog(LOG_INFO, "cleanup - joining thread %ld", thread->thread_id);
        if (pthread_join(thread->thread_id, NULL) != 0) {
            syslog(LOG_ERR, "cleanup - error joining thread!");
            exit(EXIT_FAILURE);
        }
        free(thread);
    }

    // Close open sockets
    if (sockfd >= 0) close(sockfd);

    // Close file descriptors
    if (datafd >= 0) close(datafd);

#if USE_AESD_CHAR_DEVICE != 1
    // Delete the file
    remove("/var/tmp/aesdsocketdata");
#endif

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

void *handle_connection(void *arg)
{
    struct thread_info_t *thread_info = (struct thread_info_t *)arg;
    client_info_t client_data = thread_info->client_data;

    // Open file for aesdsocketdata
    datafd = open(aesddata_file, O_CREAT | O_RDWR | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (datafd == -1){
        syslog(LOG_ERR, "ERROR: Failed to create file - %s", aesddata_file);
        closelog();
        exit(EXIT_FAILURE);
    }

    // Receive and process data
    size_t buffer_size = 1024;
    char* buffer = (char *)malloc(buffer_size * sizeof(char));
    if (buffer == NULL) {
        syslog(LOG_ERR, "ERROR: Failed to malloc");
        cleanup(EXIT_FAILURE);
    }
    memset(buffer, 0, buffer_size * sizeof(char));
    ssize_t recv_size;

    while ((recv_size = recv(client_data.client_sockfd, buffer, buffer_size, 0)) > 0) {
        // Append data to file
#if USE_AESD_CHAR_DEVICE != 1
        // Lock the mutex before writing to the file
        if (pthread_mutex_lock(&aesddata_file_mutex) != 0) {
            syslog(LOG_ERR, "ERROR: Failed to acquire mutex!");
            cleanup(EXIT_FAILURE);
        }
#endif
#if USE_AESD_CHAR_DEVICE == 1
            const char *ioctl_id_string = "AESDCHAR_IOCSEEKTO:";
            if (strncmp(buffer, ioctl_id_string, strlen(ioctl_id_string)) == 0) {
                struct aesd_seekto seek_params;
                if (sscanf(buffer, "AESDCHAR_IOCSEEKTO:%d,%d", &seek_params.write_cmd, &seek_params.write_cmd_offset) != 2) {
                    syslog(LOG_WARNING, "WARNING: Incorrectly formatted AESDCHAR_IOCSEEKTO. Treating it as regular string to write.");
                    goto write;
                }
                else {
                    if (ioctl(datafd, AESDCHAR_IOCSEEKTO, &seek_params) != 0) {
                        syslog(LOG_ERR, "ERROR: Failed to perform ioctl write command");
                        cleanup(EXIT_FAILURE);
                    }
                }
            }
            else {
#endif
write:
                if (write(datafd, buffer, recv_size) == -1) {
                    syslog(LOG_ERR, "ERROR: Failed to write to %s file", aesddata_file);
                    cleanup(EXIT_FAILURE);
                }
#if USE_AESD_CHAR_DEVICE == 1
            }
#endif
#if USE_AESD_CHAR_DEVICE != 1
        // Unlock the mutex after writing to the file
        if (pthread_mutex_unlock(&aesddata_file_mutex) != 0) {
            syslog(LOG_ERR, "ERROR: Failed to release mutex!");
            cleanup(EXIT_FAILURE);
        }
#endif

        // Send data back to client if a complete packet is received (ends with newline)
        if (memchr(buffer, '\n', buffer_size) != NULL) {

#if USE_AESD_CHAR_DEVICE != 1
                lseek(datafd, 0, SEEK_SET);
#endif
                size_t bytes_read = read(datafd, buffer, buffer_size);
                if (bytes_read == -1) {
                    syslog(LOG_ERR, "ERROR: Failed to read from %s file", aesddata_file);
                    cleanup(EXIT_FAILURE);
                }
                while (bytes_read > 0) {
                    send(client_data.client_sockfd, buffer, bytes_read, 0);
                    bytes_read = read(datafd, buffer, buffer_size); 
                    if (bytes_read == -1) {
                        syslog(LOG_ERR, "ERROR: Failed to read from %s file", aesddata_file);
                        cleanup(EXIT_FAILURE);
                    }
                }
        }
        memset(buffer, 0, buffer_size * sizeof(char));
    }

    free(buffer);
    if (datafd >= 0) close(datafd);

    // Log closed connection
    syslog(LOG_INFO, "Closed connection from %s", client_data.client_ip);
    close(client_data.client_sockfd);

    thread_info->work_done = 1;
    return NULL;
}
#if USE_AESD_CHAR_DEVICE != 1
void *timestamp_handler(void *arg) {
    while (!signal_exit) {
        time_t current_time = time(NULL);
        struct tm *time_info = localtime(&current_time);

        char timestamp[100];
        strftime(timestamp, sizeof(timestamp), "timestamp: %a, %d %b %Y %H:%M:%S %z\n", time_info);

        // Append timestamp to /var/tmp/aesdsocketdata
        // Lock the mutex before writing to the file
        if (pthread_mutex_lock(&aesddata_file_mutex) != 0) {
            syslog(LOG_ERR, "ERROR: Failed to acquire mutex!");
            cleanup(EXIT_FAILURE);
        }
        write(datafd, timestamp, strlen(timestamp));
        // Unlock the mutex after writing to the file
        if (pthread_mutex_unlock(&aesddata_file_mutex) != 0) {
            syslog(LOG_ERR, "ERROR: Failed to release mutex!");
            cleanup(EXIT_FAILURE);
        }

        sleep(10); // Wait for 10 seconds before appending the next timestamp
    }

    return NULL;
}
#endif

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

    // Initialize thread list
    SLIST_INIT(&thread_list);

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

#if USE_AESD_CHAR_DEVICE != 1
    // Dedicated thread to append timestamps
    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, timestamp_handler, NULL) != 0) {
        syslog(LOG_ERR, "ERROR: Failed to create timestamp thread!");
        cleanup(EXIT_FAILURE);
    }
#endif

    // Accept connections in a loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sockfd == -1) {
            syslog(LOG_WARNING, "WARNING: Failed to accept, retrying ...");
            continue; // Continue accepting connections
        }

        struct thread_info_t *new_thread = malloc(sizeof(struct thread_info_t));
        if (new_thread == NULL) {
            syslog(LOG_ERR, "ERROR: Failed to malloc");
            cleanup(EXIT_FAILURE);
        }

        // Log accepted connection
        inet_ntop(AF_INET, &(client_addr.sin_addr), new_thread->client_data.client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", new_thread->client_data.client_ip);

        new_thread->client_data.client_sockfd = client_sockfd;
        new_thread->work_done = 0;

        // Handle connection
        if (pthread_create(&new_thread->thread_id, NULL, handle_connection, (void *)new_thread) != 0) {
            syslog(LOG_ERR, "ERROR: Failed to create thread!");
            cleanup(EXIT_FAILURE);
        }
        else {
            SLIST_INSERT_HEAD(&thread_list, new_thread, entries);
        }

        // Join complete threads
        struct thread_info_t *thread, *thread_tmp;
        SLIST_FOREACH_SAFE(thread, &thread_list, entries, thread_tmp) {
            if (thread->work_done == 1) {
                syslog(LOG_INFO, "main - joining thread %ld\n", thread->thread_id);
                if (pthread_join(thread->thread_id, NULL) != 0) {
                    syslog(LOG_ERR, "main - error joining thread!");
                    cleanup(EXIT_FAILURE);
                }
                SLIST_REMOVE(&thread_list, thread, thread_info_t, entries);
                free(thread);
            }
        }
    }
    return 0;
}
