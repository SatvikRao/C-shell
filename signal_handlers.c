#include "signal_handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

// External functions and variables from execute.c
extern void check_background_jobs();
extern struct {
    pid_t pid;
    char command[4096];
    int job_number;
    int running;
    int stopped;
} bg_jobs[];
extern int bg_job_count;

// Current foreground job info
static ForegroundJob fg_job = {0, 0, -1, ""};
static int has_fg_job = 0;

/**
 * Handler for SIGINT (Ctrl-C)
 */
static void sigint_handler(int sig) {
    // If there's a foreground job, send SIGINT to its process group
    if (has_fg_job) {
        // Print a newline to make the output cleaner
        write(STDOUT_FILENO, "\n", 1);
        
        // Send signal to the foreground process group
        if (kill(-fg_job.pgid, SIGINT) < 0) {
            perror("kill (SIGINT)");
        }
    } else {
        // No foreground job, just print a new prompt
        write(STDOUT_FILENO, "\n", 1);
    }
}

/**
 * Handler for SIGTSTP (Ctrl-Z)
 */
static void sigtstp_handler(int sig) {
    if (has_fg_job) {
        // Print a newline to make the output cleaner
        write(STDOUT_FILENO, "\n", 1);
        
        // Send SIGTSTP to the foreground process group
        if (kill(-fg_job.pgid, SIGTSTP) < 0) {
            perror("kill (SIGTSTP)");
        }
    } else {
        // No foreground job, just print a new prompt
        write(STDOUT_FILENO, "\n", 1);
    }
}

void initialize_signal_handlers() {
    // Set up the SIGINT handler (Ctrl-C)
    struct sigaction sa_int;
    memset(&sa_int, 0, sizeof(sa_int));
    sa_int.sa_handler = sigint_handler;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa_int, NULL) < 0) {
        perror("sigaction (SIGINT)");
        exit(EXIT_FAILURE);
    }
    
    // Set up the SIGTSTP handler (Ctrl-Z)
    struct sigaction sa_tstp;
    memset(&sa_tstp, 0, sizeof(sa_tstp));
    sa_tstp.sa_handler = sigtstp_handler;
    sigemptyset(&sa_tstp.sa_mask);
    sa_tstp.sa_flags = SA_RESTART;
    if (sigaction(SIGTSTP, &sa_tstp, NULL) < 0) {
        perror("sigaction (SIGTSTP)");
        exit(EXIT_FAILURE);
    }
    
    // Ignore SIGTTOU to prevent the shell from stopping when we change terminal process groups
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
}

void handle_eof() {
    // Print "logout" message
    printf("\nlogout\n");
    
    // Scan through all background processes and send SIGKILL
    for (int i = 0; i < bg_job_count; i++) {
        if (bg_jobs[i].running) {
            kill(bg_jobs[i].pid, SIGKILL);
        }
    }
    
    // Exit with status 0
    exit(0);
}

void set_foreground_job_info(pid_t pid, pid_t pgid, int job_number, const char *command) {
    fg_job.pid = pid;
    fg_job.pgid = pgid;
    fg_job.job_number = job_number;
    strncpy(fg_job.command, command, sizeof(fg_job.command) - 1);
    fg_job.command[sizeof(fg_job.command) - 1] = '\0';
    has_fg_job = 1;
}

void clear_foreground_job_info() {
    has_fg_job = 0;
}

int has_foreground_job() {
    return has_fg_job;
}
