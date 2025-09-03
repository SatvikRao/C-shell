#include "hop.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
// #include <dirent.h>
// #include <sys/stat.h>

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

/**
 * Resolves the target directory for reveal command
 * Returns 0 on success, -1 on failure
 */
// static int resolve_target_directory(const char *arg, char *resolved_path) {
//     char current_dir[PATH_MAX];
    
//     // Save current directory
//     if (getcwd(current_dir, PATH_MAX) == NULL) {
//         perror("getcwd failed");
//         return -1;
//     }
    
//     if (strcmp(arg, "~") == 0) {
//         // Home directory
//         strcpy(resolved_path, get_home_directory());
//     } 
//     else if (strcmp(arg, ".") == 0) {
//         // Current directory
//         strcpy(resolved_path, current_dir);
//     } 
//     else if (strcmp(arg, "..") == 0) {
//         // Parent directory
//         if (chdir("..") != 0) {
//             perror("chdir failed");
//             return -1;
//         }
        
//         if (getcwd(resolved_path, PATH_MAX) == NULL) {
//             perror("getcwd failed");
//             chdir(current_dir); // Restore original directory
//             return -1;
//         }
        
//         // Restore original directory
//         chdir(current_dir);
//     } 
//     else if (strcmp(arg, "-") == 0) {
//         // Previous directory
//         if (!has_previous_cwd) {
//             printf("No such directory!\n");
//             return -1;
//         }
//         strcpy(resolved_path, previous_cwd);
//     } 
//     else {
//         // Specified path
//         // First try as absolute path
//         if (arg[0] == '/') {
//             strcpy(resolved_path, arg);
//         } else {
//             // Try as relative path
//             snprintf(resolved_path, PATH_MAX, "%s/%s", current_dir, arg);
//         }
//     }
    
//     // Check if the resolved path is a directory
//     struct stat sb;
//     if (stat(resolved_path, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
//         printf("No such directory!\n");
//         return -1;
//     }
    
//     return 0;
// }

// int handle_reveal_command(int argc, char *argv[]) {
//     int show_hidden = 0;
//     int line_by_line = 0;
//     int target_arg_index = -1;
    
//     // Parse flags
//     for (int i = 1; i < argc; i++) {
//         if (argv[i][0] == '-' && argv[i][1] != '\0') {
//             // This is a flag argument
//             for (int j = 1; argv[i][j] != '\0'; j++) {
//                 if (argv[i][j] == 'a') {
//                     show_hidden = 1;
//                 } else if (argv[i][j] == 'l') {
//                     line_by_line = 1;
//                 } else {
//                     printf("reveal: Invalid Syntax!\n");
//                     return 1;
//                 }
//             }
//         } else {
//             // This is the target directory argument
//             if (target_arg_index != -1) {
//                 // We already have a target, too many arguments
//                 printf("reveal: Invalid Syntax!\n");
//                 return 1;
//             }
//             target_arg_index = i;
//         }
//     }
    
//     // Determine the target directory
//     char target_dir[PATH_MAX];
    
//     if (target_arg_index == -1) {
//         // No target specified, use current directory
//         if (getcwd(target_dir, PATH_MAX) == NULL) {
//             perror("getcwd failed");
//             return 1;
//         }
//     } else {
//         // Use the specified target
//         if (resolve_target_directory(argv[target_arg_index], target_dir) != 0) {
//             return 1;
//         }
//     }
    
//     // Open the directory
//     DIR *dir = opendir(target_dir);
//     if (dir == NULL) {
//         printf("No such directory!\n");
//         return 1;
//     }
    
//     // Read all entries and store them for sorting
//     struct dirent *entry;
//     char *file_names[1024]; // Assuming no more than 1024 files in a directory
//     int file_count = 0;
    
//     while ((entry = readdir(dir)) != NULL) {
//         // Skip . and .. entries
//         if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
//             continue;
//         }
        
//         // Skip hidden files if not showing hidden
//         if (!show_hidden && is_hidden(entry->d_name)) {
//             continue;
//         }
        
//         // Add to our array
//         file_names[file_count] = strdup(entry->d_name);
//         if (file_names[file_count] == NULL) {
//             perror("strdup failed");
//             closedir(dir);
//             return 1;
//         }
        
//         file_count++;
//     }
    
//     closedir(dir);
    
//     // Sort the file names
//     qsort(file_names, file_count, sizeof(char *), compare_strings);
    
//     // Display the files
//     if (file_count == 0) {
//         // Empty directory, nothing to display
//         return 0;
//     }
    
//     if (line_by_line) {
//         // Display one entry per line
//         for (int i = 0; i < file_count; i++) {
//             printf("%s\n", file_names[i]);
//             free(file_names[i]);
//         }
//     } else {
//         // Display in ls format (space-separated)
//         for (int i = 0; i < file_count; i++) {
//             printf("%s%s", file_names[i], (i < file_count - 1) ? " " : "");
//             free(file_names[i]);
//         }
//         printf("\n");
//     }
    
//     return 0;
// }
