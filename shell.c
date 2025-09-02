#include "shell.h"
#include "prompt.h"
#include "parser.h"
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Buffer size for reading input
#define INPUT_BUFFER_SIZE 4096
#define PATH_MAX 4096
// The shell's home directory
char home_directory[PATH_MAX];  

int initialize_shell() {
    // Store the current working directory as home directory
    if (getcwd(home_directory, PATH_MAX) == NULL) {
        perror("getcwd failed");
        return 1;
    }
    
    // Initialize command handlers
    initialize_commands();
    
    return 0;
}

/**
 * Parse input string into command arguments
 * Returns the number of arguments
 */
static int parse_args(char *input, char *args[], int max_args) {
    int argc = 0;
    char *token = strtok(input, " \t\n");
    
    while (token != NULL && argc < max_args) {
        args[argc++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    args[argc] = NULL;  // Null-terminate the array
    return argc;
}

void run_shell() {
    char input[INPUT_BUFFER_SIZE];
    char *args[64];  // Maximum number of arguments
    
    while (1) {
        // Display the prompt
        display_prompt();
        
        // Read user input
        if (fgets(input, INPUT_BUFFER_SIZE, stdin) == NULL) {
            // Handle EOF (Ctrl+D)
            printf("\nExiting shell\n");
            break;
        }
        
        // Remove trailing newline
        input[strcspn(input, "\n")] = '\0';
        
        // Skip empty inputs
        if (strlen(input) == 0) {
            continue;
        }
        
        // Process the command
        if (strcmp(input, "exit") == 0) {
            break;
        }
        
        // Parse the command to check syntax
        if (!parse_command(input)) {
            printf("Invalid Syntax!\n");
            continue;
        }
        
        // Parse input into arguments
        int argc = parse_args(input, args, 63);
        
        // Handle built-in commands
        if (argc > 0) {
            if (strcmp(args[0], "hop") == 0) {
                handle_hop_command(argc, args);
            }
            // Add more built-in commands here as needed
        }
        
        // Execute external commands (will be implemented later)
    }
}

// Function to get the home directory path
char* get_home_directory() {
    return home_directory;
}
