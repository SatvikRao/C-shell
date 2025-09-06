#include "ping.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

/**
 * Validate if a string is a valid number
 * @param str The string to validate
 * @return 1 if valid, 0 otherwise
 */
static int is_valid_number(const char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }
    
    // Check if first char is - (negative number)
    if (*str == '-') {
        str++;
    }
    
    // Check that all remaining characters are digits
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    
    return 1;
}

/**
 * Handle the ping command to send signals to processes
 */
int handle_ping_command(int argc, char *argv[]) {
    // Check arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: ping <pid> <signal_number>\n");
        return 1;
    }
    
    // Validate pid
    if (!is_valid_number(argv[1])) {
        fprintf(stderr, "Invalid PID format\n");
        return 1;
    }
    
    // Validate signal number
    if (!is_valid_number(argv[2])) {
        fprintf(stderr, "Invalid signal number format\n");
        return 1;
    }
    
    // Parse PID
    pid_t pid = (pid_t)atoi(argv[1]);
    if (pid <= 0) {
        fprintf(stderr, "Invalid PID: must be positive\n");
        return 1;
    }
    
    // Parse signal number
    int signal_number = atoi(argv[2]);
    
    // Calculate actual signal (modulo 32)
    int actual_signal = signal_number % 32;
    
    // Send the signal
    if (kill(pid, actual_signal) == -1) {
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("kill failed");
        }
        return 1;
    }
    
    // Signal sent successfully
    printf("Sent signal %d to process with pid %d\n", actual_signal, pid);
    return 0;
}
