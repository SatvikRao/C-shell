#include "fg_bg.h"
#include "signal_handlers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>

//LLM generated
// External declarations for the background job array
extern struct {
    pid_t pid;
    char command[4096];
    int job_number;
    int running;
    int stopped;
} bg_jobs[];
extern int bg_job_count;

/**
 * Get a job by its job number
 */
static int find_job_by_number(int job_number) {
    for (int i = 0; i < bg_job_count; i++) {
        if (bg_jobs[i].job_number == job_number && bg_jobs[i].running) {
            return i;
        }
    }
    return -1;
}

/**
 * Get the most recent job (highest job number)
 */
static int find_most_recent_job() {
    int max_job_number = -1;
    int index = -1;
    
    for (int i = 0; i < bg_job_count; i++) {
        if (bg_jobs[i].running && bg_jobs[i].job_number > max_job_number) {
            max_job_number = bg_jobs[i].job_number;
            index = i;
        }
    }
    
    return index;
}

/**
 * Parse a job number from an argument
 */
static int parse_job_number(const char *arg) {
    // Check if it's a job number with % prefix
    if (arg[0] == '%') {
        arg++; // Skip the % character
    }
    
    // Check if the argument is a valid number
    for (int i = 0; arg[i] != '\0'; i++) {
        if (!isdigit(arg[i])) {
            return -1;
        }
    }
    
    return atoi(arg);
}

/**
 * Handle the 'fg' command
 */
int handle_fg_command(int argc, char *argv[]) {
    int job_index;
    
    if (argc == 1) {
        // No job number provided, use the most recent job
        job_index = find_most_recent_job();
        if (job_index == -1) {
            fprintf(stderr, "No jobs available\n");
            return 1;
        }
    } else if (argc == 2) {
        // Parse job number
        int job_number = parse_job_number(argv[1]);
        if (job_number == -1) {
            fprintf(stderr, "Invalid job number\n");
            return 1;
        }
        
        // Find the job
        job_index = find_job_by_number(job_number);
        if (job_index == -1) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Usage: fg [job_number]\n");
        return 1;
    }
    
    // Get the job information
    pid_t pid = bg_jobs[job_index].pid;
    int job_number = bg_jobs[job_index].job_number;
    int stopped = bg_jobs[job_index].stopped;
    
    // Print the command
    printf("%s\n", bg_jobs[job_index].command);
    
    // If the job is stopped, send SIGCONT to resume it
    if (stopped) {
        if (kill(-pid, SIGCONT) < 0) {
            perror("kill (SIGCONT)");
            return 1;
        }
        bg_jobs[job_index].stopped = 0;
    }
    
    // Set as foreground job
    set_foreground_job_info(pid, pid, job_number, bg_jobs[job_index].command);
    
    // Give terminal control to the job
    tcsetpgrp(STDIN_FILENO, pid);
    
    // Wait for the job to complete or stop again
    int status;
    waitpid(pid, &status, WUNTRACED);
    
    // Check if the job was stopped
    if (WIFSTOPPED(status)) {
        bg_jobs[job_index].stopped = 1;
        printf("[%d] Stopped %s\n", job_number, bg_jobs[job_index].command);
    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
        // Job has terminated
        bg_jobs[job_index].running = 0;
    }
    
    // Take back terminal control
    tcsetpgrp(STDIN_FILENO, getpgrp());
    
    // Clear the foreground job
    clear_foreground_job_info();
    
    return 0;
}

/**
 * Handle the 'bg' command
 */
int handle_bg_command(int argc, char *argv[]) {
    int job_index;
    
    if (argc == 1) {
        // No job number provided, use the most recent job
        job_index = find_most_recent_job();
        if (job_index == -1) {
            fprintf(stderr, "No jobs available\n");
            return 1;
        }
    } else if (argc == 2) {
        // Parse job number
        int job_number = parse_job_number(argv[1]);
        if (job_number == -1) {
            fprintf(stderr, "Invalid job number\n");
            return 1;
        }
        
        // Find the job
        job_index = find_job_by_number(job_number);
        if (job_index == -1) {
            fprintf(stderr, "No such job\n");
            return 1;
        }
    } else {
        fprintf(stderr, "Usage: bg [job_number]\n");
        return 1;
    }
    
    // Get the job information
    pid_t pid = bg_jobs[job_index].pid;
    int job_number = bg_jobs[job_index].job_number;
    int stopped = bg_jobs[job_index].stopped;
    
    // Check if the job is already running
    if (!stopped) {
        fprintf(stderr, "Job already running\n");
        return 1;
    }
    
    // Send SIGCONT to resume the job
    if (kill(-pid, SIGCONT) < 0) {
        perror("kill (SIGCONT)");
        return 1;
    }
    
    // Update job status
    bg_jobs[job_index].stopped = 0;
    
    // Print message
    printf("[%d] %s &\n", job_number, bg_jobs[job_index].command);
    
    return 0;
}
