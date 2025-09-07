#ifndef LOG_H
#define LOG_H

/**
 * Add a command to the command history
 * @param command The command to add
 */
void add_to_history(const char *command);

/**
 * Handle the log command (display, purge, or execute history)
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, non-zero on failure
 */
int handle_log_command(int argc, char *argv[]);

/**
 * Initialize the command history
 */
void initialize_history();

/**
 * Check if the command is a log command
 * @param command The command to check
 * @return 1 if it's a log command, 0 otherwise
 */
int is_log_command(const char *command);

#endif // LOG_H
