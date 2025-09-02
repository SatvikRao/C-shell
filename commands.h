#ifndef COMMANDS_H
#define COMMANDS_H

/**
 * Handles the hop command to change the current working directory
 * Returns 0 on success, non-zero on failure
 */
int handle_hop_command(int argc, char *argv[]);

/**
 * Initializes the command handler
 */
void initialize_commands();

#endif // COMMANDS_H
