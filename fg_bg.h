#ifndef FG_BG_H
#define FG_BG_H

/**
 * Handle the 'fg' command to bring a job to the foreground
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, non-zero on failure
 */
int handle_fg_command(int argc, char *argv[]);

/**
 * Handle the 'bg' command to continue a stopped background job
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return 0 on success, non-zero on failure
 */
int handle_bg_command(int argc, char *argv[]);

#endif // FG_BG_H
