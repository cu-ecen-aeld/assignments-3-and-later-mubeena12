/*
* Author: Mubeena Udyavar Kazi
* Course: ECEN 5713 - AESD
* Reference: 
*   1. ChatGPT with prompt "file I/O using system calls C"
*   2. Stack overflow           
*/

#include <stdio.h>
#include <stdlib.h>
#include<syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

// Function to write string to file
void write_str_to_file(char *writefile, char *writestr){
    openlog("writer.c", LOG_PID, LOG_USER);
    // Verify if the writefile and writestr is empty
    if (writefile == NULL  || writestr == NULL ){
        syslog(LOG_ERR, "ERROR: %s and %s are both not specified", writefile, writestr);
        closelog();
        exit(1);
    }
    // Open() System call is used to create the file
    int fd = open(writefile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH );
    if (fd == -1){
        syslog(LOG_ERR, "ERROR: Could not create %s file", writefile);
        closelog();
        exit(1);
    }
    // write() system call is used to write to file descriptor
    if (write(fd, writestr, strlen(writestr)) == -1) {
        syslog(LOG_ERR, "ERROR: Could not write to %s file", writefile);
        close(fd);
        closelog();
        exit(1);
    }
    close(fd);

    syslog(LOG_DEBUG, "Writing '%s' to %s", writestr, writefile);
    closelog();
}
int main(int argc, char *argv[]){
    openlog("writer.c", LOG_PID, LOG_USER);
    // Verify the number of CLI arguments
    if (argc < 3){
       syslog(LOG_ERR, "ERROR: Please specify two arguments"); 
        closelog();
        exit(1);
    }
    write_str_to_file(argv[1], argv[2]);
    closelog();
    return 0;
}
	
