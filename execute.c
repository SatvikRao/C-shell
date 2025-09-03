#include "execute.h"
#include "hop.h"
#include "reveal.h"
#include "log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_ARGS 64
#define MAX_PIPES 16
#define MAX_COMMAND_LENGTH 4096

// Structure to hold a command with its redirections
typedef struct {
    char *args[MAX_ARGS];  // Command and arguments
    int argc;              // Number of arguments
    char *input_file;      // Input redirection file
    char *output_file;     // Output redirection file
    int append_output;     // 1 if output should be appended, 0 otherwise
} Command;

// Forward declarations
static int execute_cmd_group(const char *cmd_group);
static int execute_pipeline(Command *commands, int cmd_count);
static int execute_builtin(Command *cmd);

/**
 * Execute a command line, handling sequential and background operators
 * For now, only the first command group is executed
 */
int execute_command_line(const char *command) {
    char cmd_copy[MAX_COMMAND_LENGTH];
    strncpy(cmd_copy, command, MAX_COMMAND_LENGTH - 1);
    cmd_copy[MAX_COMMAND_LENGTH - 1] = '\0';
    
    // Find the first sequential or background operator
    char *semicolon = strchr(cmd_copy, ';');
    char *amp = strchr(cmd_copy, '&');
    
    // Find the earliest operator
    char *first_op = NULL;
    if (semicolon && amp) {
        first_op = (semicolon < amp) ? semicolon : amp;
    } else if (semicolon) {
        first_op = semicolon;
    } else if (amp) {
        // Check if it's a single & or &&
        if (amp[1] == '&') {
            first_op = amp;
        } else {
            first_op = amp;
        }
    }
    
    // Extract the first command group
    if (first_op) {
        *first_op = '\0';
    }
    
    // Execute the first command group
    return execute_cmd_group(cmd_copy);
}

/**
 * Execute a command group (commands separated by pipes)
 */
static int execute_cmd_group(const char *cmd_group) {
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
    return execute_pipeline(commands, cmd_count);
}

/**
 * Check if a command is a built-in command
 */
static int is_builtin(const char *cmd) {
    return (strcmp(cmd, "hop") == 0 ||
            strcmp(cmd, "reveal") == 0 ||
            strcmp(cmd, "log") == 0);
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
    }
    
    return 1; // Not a built-in command
}

/**
 * Execute a pipeline of commands
 */
static int execute_pipeline(Command *commands, int cmd_count) {
    // If only one command and it's a built-in, execute it directly
    if (cmd_count == 1 && is_builtin(commands[0].args[0])) {
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
    
    // Create child processes for each command
    pid_t pids[MAX_PIPES + 1];
    
    for (int i = 0; i < cmd_count; i++) {
        pids[i] = fork();
        
        if (pids[i] == -1) {
            perror("fork");
            return 1;
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
    
    // Wait for all child processes
    int status;
    for (int i = 0; i < cmd_count; i++) {
        waitpid(pids[i], &status, 0);
    }
    
    return 0;
}
