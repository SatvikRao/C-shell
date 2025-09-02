#include "prompt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>

// External function from shell.c to get the home directory
extern char* get_home_directory();

// Function to get username
static char* get_username() {
    struct passwd *pw = getpwuid(getuid());
    if (pw) {
        return pw->pw_name;
    }
    return "unknown";
}

// Function to get system name
static char* get_system_name() {
    static char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, HOST_NAME_MAX) != 0) {
        strcpy(hostname, "unknown");
    }
    // Truncate at first dot to get just the system name
    char *dot = strchr(hostname, '.');
    if (dot) {
        *dot = '\0';
    }
    return hostname;
}

// Function to format the current path, replacing home directory with ~
static void format_current_path(char *formatted_path, size_t size) {
    char cwd[PATH_MAX];
    char *home = get_home_directory();
    
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strncpy(formatted_path, "unknown", size);
        return;
    }
    
    // Check if current directory is under home directory
    if (strncmp(cwd, home, strlen(home)) == 0 && 
        (cwd[strlen(home)] == '/' || cwd[strlen(home)] == '\0')) {
        // Replace home directory with tilde
        snprintf(formatted_path, size, "~%s", &cwd[strlen(home)]);
    } else {
        // Use absolute path
        strncpy(formatted_path, cwd, size);
    }
}

void display_prompt() {
    char formatted_path[PATH_MAX];
    format_current_path(formatted_path, PATH_MAX);
    
    printf("<%s@%s:%s> ", get_username(), get_system_name(), formatted_path);
    fflush(stdout);
}
