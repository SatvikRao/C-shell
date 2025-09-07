#ifndef SIGNAL_HANDLERS_H
#define SIGNAL_HANDLERS_H

#include <sys/types.h> // For pid_t type
#include <signal.h>    // For struct sigaction

/**
 * Initialize signal handlers for job control
 * Sets up handlers for SIGINT (Ctrl-C), SIGTSTP (Ctrl-Z), and other necessary signals
 */
void initialize_signal_handlers();

/**
 * Handle Ctrl-D (EOF) condition
 * Sends SIGKILL to all child processes and exits the shell
 */
void handle_eof();

/**
 * Structure to represent a foreground job
 */
typedef struct {
    pid_t pid;
    pid_t pgid;
    int job_number;
    char command[4096];
} ForegroundJob;

/**
 * Set the current foreground job
 * @param pid Process ID
 * @param pgid Process group ID
 * @param job_number Job number
 * @param command Command string
 */
void set_foreground_job_info(pid_t pid, pid_t pgid, int job_number, const char *command);

/**
 * Clear the current foreground job
 */
void clear_foreground_job_info();

/**
 * Get if there is a foreground job
 * @return 1 if there is a foreground job, 0 otherwise
 */
int has_foreground_job();

#endif // SIGNAL_HANDLERS_H
