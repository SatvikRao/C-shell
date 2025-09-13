#include "hop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
// #include <dirent.h>
// #include <sys/stat.h>

//LLM generated
// External function from shell.c to get the home directory
extern char* get_home_directory();

// Store the previous working directory for the '-' argument
static char previous_cwd[PATH_MAX] = "";
static int has_previous_cwd = 0;

// // Helper function to check if a file is hidden
// static int is_hidden(const char *name) {
//     return name[0] == '.';
// }

// // Helper function to compare two strings for sorting
// static int compare_strings(const void *a, const void *b) {
//     return strcmp(*(const char **)a, *(const char **)b);
// }

/**
 * Updates the previous_cwd variable with the current working directory
 */
static void update_previous_cwd() {
    char current[PATH_MAX];
    if (getcwd(current, PATH_MAX) != NULL) {
        strcpy(previous_cwd, current);
        has_previous_cwd = 1;
    }
}

void initialize_commands() {
    // Initialize the previous_cwd to empty string
    previous_cwd[0] = '\0';
    has_previous_cwd = 0;
}

/**
 * Changes directory to the given path and updates previous_cwd
 * Returns 0 on success, -1 on failure
 */
static int change_directory(const char *path) {
    // Save current directory before changing
    update_previous_cwd();
    
    // Change directory
    if (chdir(path) != 0) {
        perror("chdir failed");
        return -1;
    }
    
    return 0;
}

int handle_hop_command(int argc, char *argv[]) {
    // If no arguments, change to home directory
    if (argc == 1) {
        return change_directory(get_home_directory());
    }
    
    // Process each argument sequentially
    for (int i = 1; i < argc; i++) {
        char *arg = argv[i];
        
        if (strcmp(arg, "~") == 0) {
            // Change to home directory
            if (change_directory(get_home_directory()) != 0) {
                return 1;
            }
        } 
        else if (strcmp(arg, ".") == 0) {
            // Stay in the same directory (do nothing)
            continue;
        } 
        else if (strcmp(arg, "..") == 0) {
            // Change to parent directory
            if (change_directory("..") != 0) {
                return 1;
            }
        } 
        else if (strcmp(arg, "-") == 0) {
            // Change to previous directory
            if (has_previous_cwd) {
                char temp[PATH_MAX];
                getcwd(temp, PATH_MAX);
                
                if (chdir(previous_cwd) != 0) {
                    perror("chdir failed");
                    return 1;
                }
                
                // Update previous_cwd to the directory we just left
                strcpy(previous_cwd, temp);
            }
            // If no previous directory, do nothing
        } 
        else {
            // Change to the specified path
            if (chdir(arg) != 0) {
                printf("No such directory!\n");
                return 1;
            }
            update_previous_cwd();
        }
    }
    
    return 0;
}

