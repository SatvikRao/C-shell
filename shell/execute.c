#include "execute.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include "activities.h"
#include "ping.h"
#include "fg_bg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include "signal_handlers.h"
#include <termios.h>

#define MAX_ARGS 64
#define MAX_PIPES 16
#define MAX_COMMAND_LENGTH 4096
#define MAX_BG_JOBS 32
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE

//LLM generated
// Structure to hold a command with its redirections
typedef struct {
    char *args[MAX_ARGS];  // Command and arguments
    int argc;              // Number of arguments
    char *input_file;      // Input redirection file
    char *output_file;     // Output redirection file
    int append_output;     // 1 if output should be appended, 0 otherwise
} Command;

// Structure to track background jobs
typedef struct {
    pid_t pid;                      // Process ID
    char command[MAX_COMMAND_LENGTH]; // Command string
    int job_number;                 // Job number
    int running;                    // 1 if job is still running, 0 otherwise
    int stopped;                    // 1 if job is stopped, 0 if running
} BackgroundJob;

// Global array to track background jobs
BackgroundJob bg_jobs[MAX_BG_JOBS];
int bg_job_count = 0;
static int next_job_number = 1;

// Forward declarations
static int execute_cmd_group(const char *cmd_group, int run_in_background);
static int execute_pipeline(Command *commands, int cmd_count, int run_in_background);
static int execute_builtin(Command *cmd);
static void add_background_job(pid_t pid, const char *command);
static char* get_command_name(const char *command);
static void add_to_background_jobs(pid_t pid, pid_t pgid, int job_number, const char *command, int stopped);

/**
 * Execute a command line, handling sequential and background operators
 */
int execute_command_line(const char *command) {
    // Check for completed background jobs first
    check_background_jobs();
    
    char cmd_copy[MAX_COMMAND_LENGTH];
    strncpy(cmd_copy, command, MAX_COMMAND_LENGTH - 1);
    cmd_copy[MAX_COMMAND_LENGTH - 1] = '\0';
    
    // Split the command by both semicolons and ampersands for sequential execution
    char *commands[MAX_ARGS];
    int cmd_count = 0;
    int background_flags[MAX_ARGS] = {0}; // Track which commands should run in background
    
    // Start with the first command
    commands[0] = cmd_copy;
    cmd_count = 1;
    
    // Process the command string to find delimiters (both ; and &)
    char *current = cmd_copy;
    while (*current) {
        if (*current == ';') {
            *current = '\0';
            commands[cmd_count++] = current + 1;
            background_flags[cmd_count - 2] = 0; // Previous command is not background
        } 
        else if (*current == '&') {
            // Check if it's a single & (not &&)
            if (current == cmd_copy || *(current-1) != '&') {
                if (*(current+1) != '&') {
                    *current = '\0';
                    commands[cmd_count++] = current + 1;
                    background_flags[cmd_count - 2] = 1; // Previous command is background
                }
            }
        }
        current++;
    }
    
    // Execute each command sequentially
    for (int i = 0; i < cmd_count; i++) {
        char *cmd = commands[i];
        
        // Skip leading whitespace
        while (*cmd == ' ' || *cmd == '\t') {
            cmd++;
        }
        
        // Skip empty commands
        if (*cmd == '\0') {
            continue;
        }
        
        // Check if the last command should run in background (trailing &)
        int run_in_background = background_flags[i];
        
        // Also check for trailing & in the last command
        if (i == cmd_count - 1) {
            char *amp = strrchr(cmd, '&'); // Find the last '&'
            
            if (amp != NULL) {
                // Make sure it's not part of && operator
                if ((amp == cmd || *(amp-1) != '&') && *(amp+1) == '\0') {
                    run_in_background = 1;
                    *amp = '\0'; // Remove the '&'
                }
            }
        }
        
        // Execute the command group
        execute_cmd_group(cmd, run_in_background);
    }
    
    return 0;
}

/**
 * Check for completed background jobs
 * This is now a non-static function to match the header
 */
void check_background_jobs() {
    int status;
    pid_t pid;
    
    // Check if any background processes have completed using non-blocking waitpid
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        // Find the job in our array
        for (int i = 0; i < bg_job_count; i++) {
            if (bg_jobs[i].pid == pid) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    // Job has terminated
                    bg_jobs[i].running = 0;
                    bg_jobs[i].stopped = 0;
                    
                    // Get the command name (first word)
                    char *cmd_name = get_command_name(bg_jobs[i].command);
                    
                    // Print message based on exit status
                    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
                        printf("%s with pid %d exited normally\n", cmd_name, pid);
                    } else {
                        printf("%s with pid %d exited abnormally\n", cmd_name, pid);
                    }
                    
                    free(cmd_name); // Free the allocated memory
                } else if (WIFSTOPPED(status)) {
                    // Job has been stopped
                    bg_jobs[i].running = 1;  // Job is still running but in stopped state
                    bg_jobs[i].stopped = 1;  // Mark as stopped
                    
                    // Print a message that the job is stopped
                    printf("[%d] Stopped %s\n", bg_jobs[i].job_number, bg_jobs[i].command);
                } else if (WIFCONTINUED(status)) {
                    // Job has been continued
                    bg_jobs[i].running = 1;
                    bg_jobs[i].stopped = 0;
                }
                break;
            }
        }
    }
}

/**
 * Extract the command name from a command string
 */
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

/**
 * Add a new background job to the tracking array
 */
static void add_background_job(pid_t pid, const char *command) {
    if (bg_job_count < MAX_BG_JOBS) {
        bg_jobs[bg_job_count].pid = pid;
        bg_jobs[bg_job_count].job_number = next_job_number++;
        bg_jobs[bg_job_count].running = 1;
        bg_jobs[bg_job_count].stopped = 0;  // Initialize as running
        strncpy(bg_jobs[bg_job_count].command, command, MAX_COMMAND_LENGTH - 1);
        bg_jobs[bg_job_count].command[MAX_COMMAND_LENGTH - 1] = '\0';
        
        // Print job information
        printf("[%d] %d\n", bg_jobs[bg_job_count].job_number, pid);
        
        bg_job_count++;
    }
}

/**
 * Execute a command group (commands separated by pipes)
 */
static int execute_cmd_group(const char *cmd_group, int run_in_background) {
    // Split the command group by pipes
    char cmd_copy[MAX_COMMAND_LENGTH];
    strncpy(cmd_copy, cmd_group, MAX_COMMAND_LENGTH - 1);
    cmd_copy[MAX_COMMAND_LENGTH - 1] = '\0';
    
    // Count pipes and split the string
    int pipe_count = 0;
    char *pipe_cmds[MAX_PIPES + 1];
    
    pipe_cmds[0] = cmd_copy;
    char *pipe_pos = cmd_copy;
    
    while ((pipe_pos = strchr(pipe_pos, '|')) != NULL && pipe_count < MAX_PIPES) {
        *pipe_pos = '\0';
        pipe_cmds[pipe_count + 1] = pipe_pos + 1;
        pipe_count++;
        pipe_pos++;
    }
    
    int cmd_count = pipe_count + 1;
    
    // Parse each command and add to commands array
    Command commands[MAX_PIPES + 1];
    memset(commands, 0, sizeof(commands));
    
    for (int i = 0; i < cmd_count; i++) {
        char *cmd_str = pipe_cmds[i];
        
        // Skip leading whitespace
        while (*cmd_str == ' ' || *cmd_str == '\t') {
            cmd_str++;
        }
        
        // Parse the command
        Command *cmd = &commands[i];
        cmd->argc = 0;
        cmd->input_file = NULL;
        cmd->output_file = NULL;
        cmd->append_output = 0;
        
        // Tokenize the command
        char *token = strtok(cmd_str, " \t");
        while (token != NULL && cmd->argc < MAX_ARGS - 1) {
            // Check for input redirection
            if (strcmp(token, "<") == 0) {
                token = strtok(NULL, " \t");
                if (token == NULL) {
                    fprintf(stderr, "Syntax error near unexpected token '<'\n");
                    return 1;
                }
                cmd->input_file = token;
            }
            // Check for output redirection
            else if (strcmp(token, ">") == 0) {
                token = strtok(NULL, " \t");
                if (token == NULL) {
                    fprintf(stderr, "Syntax error near unexpected token '>'\n");
                    return 1;
                }
                cmd->output_file = token;
                cmd->append_output = 0;
            }
            // Check for append output redirection
            else if (strcmp(token, ">>") == 0) {
                token = strtok(NULL, " \t");
                if (token == NULL) {
                    fprintf(stderr, "Syntax error near unexpected token '>>'\n");
                    return 1;
                }
                cmd->output_file = token;
                cmd->append_output = 1;
            }
            // Regular argument
            else {
                cmd->args[cmd->argc++] = token;
            }
            
            token = strtok(NULL, " \t");
        }
        
        // Null-terminate the arguments array
        cmd->args[cmd->argc] = NULL;
        
        // Check if we have a valid command
        if (cmd->argc == 0) {
            fprintf(stderr, "Invalid command\n");
            return 1;
        }
    }
    
    // Execute the pipeline
    return execute_pipeline(commands, cmd_count, run_in_background);
}

/**
 * Check if a command is a built-in command
 */
static int is_builtin(const char *cmd) {
    return (strcmp(cmd, "hop") == 0 ||
            strcmp(cmd, "reveal") == 0 ||
            strcmp(cmd, "log") == 0 ||
            strcmp(cmd, "activities") == 0 ||
            strcmp(cmd, "fg") == 0 ||
            strcmp(cmd, "bg") == 0 ||
            strcmp(cmd, "ping") == 0);
}

/**
 * Execute a built-in command
 */
static int execute_builtin(Command *cmd) {
    // Convert command structure to argc/argv format for built-ins
    char **argv = cmd->args;
    int argc = cmd->argc;
    
    // Handle built-in commands
    if (strcmp(argv[0], "hop") == 0) {
        return handle_hop_command(argc, argv);
    } else if (strcmp(argv[0], "reveal") == 0) {
        return handle_reveal_command(argc, argv);
    } else if (strcmp(argv[0], "log") == 0) {
        return handle_log_command(argc, argv);
    } else if (strcmp(argv[0], "activities") == 0) {
        return handle_activities_command(argc, argv);
    } else if (strcmp(argv[0], "fg") == 0) {
        return handle_fg_command(argc, argv);
    } else if (strcmp(argv[0], "bg") == 0) {
        return handle_bg_command(argc, argv);
    } else if (strcmp(argv[0], "ping") == 0) {
        return handle_ping_command(argc, argv);
    }

    
    return 1; // Not a built-in command
}

/**
 * Execute a pipeline of commands
 */
static int execute_pipeline(Command *commands, int cmd_count, int run_in_background) {
    // If only one command and it's a built-in, execute it directly (but not in background)
    if (cmd_count == 1 && is_builtin(commands[0].args[0]) && !run_in_background) {
        // Handle I/O redirection for built-ins
        int stdin_save = -1, stdout_save = -1;
        
        // Input redirection
        if (commands[0].input_file) {
            int fd = open(commands[0].input_file, O_RDONLY);
            if (fd == -1) {
                perror("No such file or directory");
                return 1;
            }
            stdin_save = dup(STDIN_FILENO);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        
        // Output redirection
        if (commands[0].output_file) {
            int flags = O_WRONLY | O_CREAT;
            if (commands[0].append_output) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }
            
            int fd = open(commands[0].output_file, flags, 0644);
            if (fd == -1) {
                perror("Failed to open output file");
                if (stdin_save != -1) {
                    dup2(stdin_save, STDIN_FILENO);
                    close(stdin_save);
                }
                return 1;
            }
            stdout_save = dup(STDOUT_FILENO);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        
        // Execute the built-in command
        int result = execute_builtin(&commands[0]);
        
        // Restore I/O
        if (stdin_save != -1) {
            dup2(stdin_save, STDIN_FILENO);
            close(stdin_save);
        }
        if (stdout_save != -1) {
            dup2(stdout_save, STDOUT_FILENO);
            close(stdout_save);
        }
        
        return result;
    }
    
    // Set up pipes
    int pipes[MAX_PIPES][2];
    for (int i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            return 1;
        }
    }
    
    // Create a main child process for the entire pipeline
    pid_t main_pid;
    if (run_in_background) {
        main_pid = fork();
        
        if (main_pid == -1) {
            perror("fork");
            return 1;
        }
        
        if (main_pid > 0) {
            // Parent process - add the job to our tracking array
            char command_str[MAX_COMMAND_LENGTH] = "";
            for (int i = 0; i < cmd_count; i++) {
                for (int j = 0; j < commands[i].argc; j++) {
                    strcat(command_str, commands[i].args[j]);
                    strcat(command_str, " ");
                }
                if (i < cmd_count - 1) {
                    strcat(command_str, "| ");
                }
            }
            add_background_job(main_pid, command_str);
            
            // Close all pipe file descriptors in the parent
            for (int i = 0; i < cmd_count - 1; i++) {
                close(pipes[i][0]);
                close(pipes[i][1]);
            }
            
            return 0; // Parent returns immediately for background execution
        }
        
        // Child process continues with execution
        // Detach from terminal for background execution
        setsid();
        
        // Redirect stdin to /dev/null for background processes
        int dev_null = open("/dev/null", O_RDONLY);
        if (dev_null != -1) {
            dup2(dev_null, STDIN_FILENO);
            close(dev_null);
        }
    }
    
    // Create child processes for each command in the pipeline
    pid_t pids[MAX_PIPES + 1];
    
    for (int i = 0; i < cmd_count; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        
        if (pids[i] == 0) {
            // Child process
            
            // Handle input redirection for the first command
            if (i == 0 && commands[i].input_file) {
                int fd = open(commands[i].input_file, O_RDONLY);
                if (fd == -1) {
                    perror("No such file or directory");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            
            // Set up input from previous pipe for non-first commands
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            
            // Handle output redirection for the last command
            if (i == cmd_count - 1 && commands[i].output_file) {
                int flags = O_WRONLY | O_CREAT;
                if (commands[i].append_output) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }
                
                int fd = open(commands[i].output_file, flags, 0644);
                if (fd == -1) {
                    perror("Failed to open output file");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            
            // Set up output to next pipe for non-last commands
            if (i < cmd_count - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // Close all pipe file descriptors
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            
            // Execute the command
            if (is_builtin(commands[i].args[0])) {
                exit(execute_builtin(&commands[i]));
            } else {
                execvp(commands[i].args[0], commands[i].args);
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
    }
    
    // Parent process
    // Close all pipe file descriptors
    for (int i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // If this is a background process child, exit after all commands complete
    if (run_in_background) {
        // Wait for all child processes in the pipeline
        int status;
        for (int i = 0; i < cmd_count; i++) {
            waitpid(pids[i], &status, 0);
        }
        exit(EXIT_SUCCESS);
    }
    
    // For foreground execution:
    if (!run_in_background) {
        // Store process group as foreground job
        char command_str[MAX_COMMAND_LENGTH] = "";
        for (int i = 0; i < cmd_count; i++) {
            for (int j = 0; j < commands[i].argc; j++) {
                strcat(command_str, commands[i].args[j]);
                strcat(command_str, " ");
            }
            if (i < cmd_count - 1) {
                strcat(command_str, "| ");
            }
        }
        
        // Create a process group for the first command
        setpgid(pids[0], pids[0]);
        
        // Set as foreground job for Ctrl-C and Ctrl-Z handling
        int job_number = next_job_number++;
        set_foreground_job_info(pids[0], pids[0], job_number, command_str);
        
        // Give control of the terminal to the foreground process group
        tcsetpgrp(STDIN_FILENO, pids[0]);
        
        // Wait for all processes in the pipeline
        int status;
        int any_stopped = 0;
        
        for (int i = 0; i < cmd_count; i++) {
            if (waitpid(pids[i], &status, WUNTRACED) > 0) {
                if (WIFSTOPPED(status)) {
                    any_stopped = 1;
                }
            }
        }
        
        // If any process was stopped, add the entire pipeline to the background jobs
        if (any_stopped) {
            add_to_background_jobs(pids[0], pids[0], job_number, command_str, 1);
            printf("[%d] Stopped %s\n", job_number, command_str);
        }
        
        // Take back terminal control
        tcsetpgrp(STDIN_FILENO, getpgrp());
        
        // Clear the foreground job
        clear_foreground_job_info();
        
        return 0;
    }
    
    // For foreground execution, wait for all child processes
    int status;
    for (int i = 0; i < cmd_count; i++) {
        waitpid(pids[i], &status, 0);
    }
    
    return 0;
}

/**
 * Add a process to the background jobs list
 */
static void add_to_background_jobs(pid_t pid, pid_t pgid, int job_number, const char *command, int stopped) {
    if (bg_job_count < MAX_BG_JOBS) {
        bg_jobs[bg_job_count].pid = pid;
        bg_jobs[bg_job_count].job_number = job_number;
        bg_jobs[bg_job_count].running = 1;
        bg_jobs[bg_job_count].stopped = stopped;
        strncpy(bg_jobs[bg_job_count].command, command, MAX_COMMAND_LENGTH - 1);
        bg_jobs[bg_job_count].command[MAX_COMMAND_LENGTH - 1] = '\0';
        
        bg_job_count++;
    }
}

