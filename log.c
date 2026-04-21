#include "log.h"
#include "execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>  // For PATH_MAX

//LLM generated
// Define PATH_MAX if it's not already defined
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_HISTORY_SIZE 15
#define MAX_COMMAND_LENGTH 4096
#define HISTORY_FILE ".shell_history"

// Array to store command history
static char command_history[MAX_HISTORY_SIZE][MAX_COMMAND_LENGTH];
static int history_count = 0;

// Forward declaration for execute_command
static int execute_command(const char *command);

/**
 * Save command history to file
 */
static void save_history() {
    char history_path[PATH_MAX];
    
    // Get the home directory
    char *home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "Could not get HOME directory\n");
        return;
    }
    
    // Construct the path to the history file
    snprintf(history_path, PATH_MAX, "%s/%s", home, HISTORY_FILE);
    
    // Open the file for writing
    FILE *fp = fopen(history_path, "w");
    if (fp == NULL) {
        perror("Could not open history file");
        return;
    }
    
    // Write each command to the file
    for (int i = 0; i < history_count; i++) {
        fprintf(fp, "%s\n", command_history[i]);
    }
    
    fclose(fp);
}

/**
 * Load command history from file
 */
static void load_history() {
    char history_path[PATH_MAX];
    
    // Get the home directory
    char *home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "Could not get HOME directory\n");
        return;
    }
    
    // Construct the path to the history file
    snprintf(history_path, PATH_MAX, "%s/%s", home, HISTORY_FILE);
    
    // Open the file for reading
    FILE *fp = fopen(history_path, "r");
    if (fp == NULL) {
        // It's okay if the file doesn't exist yet
        return;
    }
    
    // Read each line from the file
    char line[MAX_COMMAND_LENGTH];
    history_count = 0;
    
    while (fgets(line, MAX_COMMAND_LENGTH, fp) != NULL && history_count < MAX_HISTORY_SIZE) {
        // Remove trailing newline
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        
        // Add to history
        strcpy(command_history[history_count++], line);
    }
    
    fclose(fp);
}

void initialize_history() {
    // Clear the history array
    for (int i = 0; i < MAX_HISTORY_SIZE; i++) {
        command_history[i][0] = '\0';
    }
    
    history_count = 0;
    
    // Load history from file
    load_history();
}

void add_to_history(const char *command) {
    // Don't add empty commands
    if (command == NULL || command[0] == '\0') {
        return;
    }
    
    // Don't add log commands
    if (is_log_command(command)) {
        return;
    }
    
    // Don't add if identical to the previous command
    if (history_count > 0 && strcmp(command_history[history_count - 1], command) == 0) {
        return;
    }
    
    // If history is full, shift everything down to make room
    if (history_count == MAX_HISTORY_SIZE) {
        for (int i = 0; i < MAX_HISTORY_SIZE - 1; i++) {
            strcpy(command_history[i], command_history[i + 1]);
        }
        history_count--;
    }
    
    // Add the new command
    strncpy(command_history[history_count], command, MAX_COMMAND_LENGTH - 1);
    command_history[history_count][MAX_COMMAND_LENGTH - 1] = '\0';
    history_count++;
    
    // Save the updated history
    save_history();
}

/**
 * Purge the command history
 */
static void purge_history() {
    // Clear the history array
    for (int i = 0; i < MAX_HISTORY_SIZE; i++) {
        command_history[i][0] = '\0';
    }
    
    history_count = 0;
    
    // Save the empty history
    save_history();
}

/**
 * Execute a command from history
 * @param index One-indexed position in history (newest to oldest)
 * @return 0 on success, non-zero on failure
 */
static int execute_from_history(int index) {
    // Check if index is valid
    if (index < 1 || index > history_count) {
        fprintf(stderr, "Invalid history index\n");
        return 1;
    }
    
    // Convert from one-indexed (newest to oldest) to zero-indexed (oldest to newest)
    int actual_index = history_count - index;
    
    // Execute the command
    return execute_command(command_history[actual_index]);
}

/**
 * Execute a command string
 */
static int execute_command(const char *command) {
    return execute_command_line(command);
}

int handle_log_command(int argc, char *argv[]) {
    // No arguments - print history
    if (argc == 1) {
        for (int i = 0; i < history_count; i++) {
            printf("%s\n", command_history[i]);
        }
        return 0;
    }
    
    // Check for purge command
    if (argc == 2 && strcmp(argv[1], "purge") == 0) {
        purge_history();
        return 0;
    }
    
    // Check for execute command
    if (argc == 3 && strcmp(argv[1], "execute") == 0) {
        // Convert the index argument to an integer
        char *endptr;
        int index = strtol(argv[2], &endptr, 10);
        
        // Check if the conversion was successful
        if (*endptr != '\0' || index <= 0) {
            fprintf(stderr, "Invalid index\n");
            return 1;
        }
        
        return execute_from_history(index);
    }
    
    // Invalid syntax
    fprintf(stderr, "log: Invalid syntax\n");
    return 1;
}

int is_log_command(const char *command) {
    // Create a copy of the command for parsing
    char cmd_copy[MAX_COMMAND_LENGTH];
    strncpy(cmd_copy, command, MAX_COMMAND_LENGTH - 1);
    cmd_copy[MAX_COMMAND_LENGTH - 1] = '\0';
    
    // Get the first token (command name)
    char *cmd_name = strtok(cmd_copy, " \t\n");
    
    // Check if it's the log command
    return (cmd_name != NULL && strcmp(cmd_name, "log") == 0);
}
