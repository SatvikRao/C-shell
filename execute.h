#ifndef EXECUTE_H
#define EXECUTE_H

/**
 * Execute a command with potential redirection and piping
 * @param command The command string to execute
 * @return 0 on success, non-zero on failure
 */
int execute_command_line(const char *command);

/**
 * Check for and report completed background jobs
 * This is automatically called by execute_command_line,
 * but can also be called separately to check for completed jobs
 */
void check_background_jobs();

#endif // EXECUTE_H
