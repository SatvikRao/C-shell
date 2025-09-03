#ifndef HOP_H
#define HOP_H

/**
 * Handles the hop command to change the current working directory
 * Returns 0 on success, non-zero on failure
 */
int handle_hop_command(int argc, char *argv[]);

// /**
//  * Handles the reveal command to list directory contents
//  * Returns 0 on success, non-zero on failure
//  */
// int handle_reveal_command(int argc, char *argv[]);

/**
 * Initializes the command handler
 */
void initialize_commands();

#endif // HOP_H
