#ifndef PING_H
#define PING_H

/**
 * Handle the ping command to send signals to processes
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, non-zero on failure
 */
int handle_ping_command(int argc, char *argv[]);

#endif // PING_H
