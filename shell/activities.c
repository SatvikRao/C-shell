#include "activities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
// LLM generated
// Define the maximum command length to match execute.c
#define MAX_COMMAND_LENGTH 4096

// Define BackgroundJob structure to match the one in execute.c
typedef struct {
    pid_t pid;                      // Process ID
    char command[MAX_COMMAND_LENGTH]; // Command string
    int job_number;                 // Job number
    int running;                    // 1 if job is still running, 0 otherwise
    int stopped;                    // 1 if job is stopped, 0 if running
} BackgroundJob;

// Structure to hold process information for sorting
typedef struct {
    pid_t pid;
    char *command_name;
    int stopped;  // 1 if stopped, 0 if running
} ProcessInfo;

// External functions from execute.c
extern void check_background_jobs();
extern BackgroundJob bg_jobs[];
extern int bg_job_count;

// Compare function for qsort to sort by command name lexicographically
static int compare_process_info(const void *a, const void *b) {
    const ProcessInfo *p1 = (const ProcessInfo *)a;
    const ProcessInfo *p2 = (const ProcessInfo *)b;
    return strcmp(p1->command_name, p2->command_name);
}

// Extract the command name from a full command string
static char* get_command_name(const char *command) {
    char *cmd_copy = strdup(command);
    char *cmd_name = strtok(cmd_copy, " \t");
    
    if (cmd_name) {
        cmd_name = strdup(cmd_name);
    } else {
        cmd_name = strdup("unknown");
    }
    
    free(cmd_copy);
    return cmd_name;
}

// Add a function to check if a process is stopped by checking /proc/<pid>/stat
static int is_process_stopped(pid_t pid) {
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", pid);
    
    FILE *fp = fopen(proc_path, "r");
    if (!fp) {
        // Process doesn't exist anymore
        return 0;
    }
    
    // Format of /proc/<pid>/stat: pid (comm) state ...
    int proc_pid;
    char comm[256];
    char state;
    
    if (fscanf(fp, "%d %s %c", &proc_pid, comm, &state) != 3) {
        fclose(fp);
        return 0;
    }
    
    fclose(fp);
    
    // State 'T' means the process is stopped
    return (state == 'T');
}

int handle_activities_command(int argc, char *argv[]) {
    // Check for the correct number of arguments
    if (argc > 1) {
        fprintf(stderr, "Usage: activities\n");
        return 1;
    }
    
    // Update background jobs status
    check_background_jobs();
    
    // Count active jobs
    int active_count = 0;
    for (int i = 0; i < bg_job_count; i++) {
        if (bg_jobs[i].running) {
            active_count++;
        }
    }
    
    // If no active jobs, return early
    if (active_count == 0) {
        return 0;
    }
    
    // Create an array of process info for sorting
    ProcessInfo *processes = malloc(active_count * sizeof(ProcessInfo));
    if (!processes) {
        perror("malloc failed");
        return 1;
    }
    
    // Fill the array with active jobs
    int idx = 0;
    for (int i = 0; i < bg_job_count; i++) {
        if (bg_jobs[i].running) {

            processes[idx].pid = bg_jobs[i].pid;
            processes[idx].command_name = get_command_name(bg_jobs[i].command);
            
            // Double-check if the process is actually stopped
            processes[idx].stopped = is_process_stopped(bg_jobs[i].pid);
            
            idx++;
        }
    }
    
    // Sort processes by command name
    qsort(processes, active_count, sizeof(ProcessInfo), compare_process_info);
    
    // Display the processes
    for (int i = 0; i < active_count; i++) {
        printf("[%d] : %s - %s\n", 
               processes[i].pid, 
               processes[i].command_name, 
               processes[i].stopped ? "Stopped" : "Running");
        
        // Free the allocated command name
        free(processes[i].command_name);
    }
    
    // Free the processes array
    free(processes);
    
    return 0;
}
