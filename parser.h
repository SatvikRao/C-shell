#ifndef PARSER_H
#define PARSER_H

/**
 * Parse and validate shell command according to the grammar
 * Returns 1 if the syntax is valid, 0 otherwise
 */
int parse_command(const char* input);

#endif // PARSER_H
