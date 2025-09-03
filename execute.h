#ifndef EXECUTE_H
#define EXECUTE_H

/**
 * Execute a command with potential redirection and piping
 * @param command The command string to execute
 * @return 0 on success, non-zero on failure
 */
int execute_command_line(const char *command);

#endif // EXECUTE_H
