#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

//LLM generated
// Token types
typedef enum {
    TOKEN_NAME,     // Any sequence not containing |&><;
    TOKEN_PIPE,     // |
    TOKEN_AMP,      // &
    TOKEN_DAMP,     // &&
    TOKEN_LT,       // <
    TOKEN_GT,       // >
    TOKEN_GTGT,     // >>
    TOKEN_SEMI,     // ;     // Added semicolon token
    TOKEN_EOI,      // End of input
    TOKEN_ERROR     // Invalid token
} TokenType;

typedef struct {
    TokenType type;
    char* value;    // Only used for TOKEN_NAME
} Token;

// Global variables for tokenizer
static const char* current_input;
static int current_position;
static Token current_token;

// Function declarations for recursive descent parser
static int parse_shell_cmd();
static int parse_cmd_group();
static int parse_atomic();
static int parse_input();
static int parse_output();
static int parse_name();

// Helper function to check if character is part of a name token
static int is_name_char(char c) {
    // In the grammar, name -> r"[^|&><;\s]+"
    // This means a name can be any character except |, &, >, <, ;, and whitespace
    return c != '\0' && c != '|' && c != '&' && c != '<' && c != '>' && c != ';' && 
           !isspace(c); // Exclude whitespace characters
}

// Skip whitespace characters
static void skip_whitespace() {
    while (current_input[current_position] != '\0' && isspace(current_input[current_position])) {
        current_position++;
    }
}

// Get the next token from input
static void get_next_token() {
    // Free previous token value if it exists
    if (current_token.type == TOKEN_NAME && current_token.value != NULL) {
        free(current_token.value);
        current_token.value = NULL;
    }

    skip_whitespace();
    
    if (current_input[current_position] == '\0') {
        current_token.type = TOKEN_EOI;
        return;
    }
    
    char c = current_input[current_position];
    
    switch (c) {
        case '|':
            current_token.type = TOKEN_PIPE;
            current_position++;
            break;
            
        case '&':
            current_position++;
            if (current_input[current_position] == '&') {
                current_token.type = TOKEN_DAMP;
                current_position++;
            } else {
                current_token.type = TOKEN_AMP;
            }
            break;
            
        case '<':
            current_token.type = TOKEN_LT;
            current_position++;
            break;
            
        case '>':
            current_position++;
            if (current_input[current_position] == '>') {
                current_token.type = TOKEN_GTGT;
                current_position++;
            } else {
                current_token.type = TOKEN_GT;
            }
            break;
            
        case ';':
            // Semicolon is part of the grammar
            current_token.type = TOKEN_SEMI;
            current_position++;
            break;
            
        default:
            // Handle name token
            if (is_name_char(c)) {
                int start = current_position;
                
                // Collect characters until we hit a special character or whitespace
                while (current_input[current_position] != '\0' && 
                       current_input[current_position] != '|' && 
                       current_input[current_position] != '&' && 
                       current_input[current_position] != '<' && 
                       current_input[current_position] != '>' && 
                       current_input[current_position] != ';' && 
                       !isspace(current_input[current_position])) {
                    current_position++;
                }
                
                int length = current_position - start;
                current_token.type = TOKEN_NAME;
                current_token.value = malloc(length + 1);
                if (current_token.value) {
                    strncpy(current_token.value, current_input + start, length);
                    current_token.value[length] = '\0';
                } else {
                    // Memory allocation failed
                    current_token.type = TOKEN_ERROR;
                }
            } else {
                // Unknown character
                current_token.type = TOKEN_ERROR;
                current_position++;
            }
            break;
    }
}

// Initialize the parser with input
static void init_parser(const char* input) {
    current_input = input;
    current_position = 0;
    current_token.type = TOKEN_ERROR;
    current_token.value = NULL;
    get_next_token();
}

// Clean up parser resources
static void cleanup_parser() {
    if (current_token.type == TOKEN_NAME && current_token.value != NULL) {
        free(current_token.value);
        current_token.value = NULL;
    }
}

// Parse shell_cmd  ->  cmd_group ((& | && | ;) cmd_group)* &?
static int parse_shell_cmd() {
    if (!parse_cmd_group()) {
        return 0;
    }
    
    while (current_token.type == TOKEN_AMP || current_token.type == TOKEN_DAMP || 
           current_token.type == TOKEN_SEMI) {
        TokenType op_type = current_token.type;
        get_next_token();
        
        if ((op_type == TOKEN_DAMP || op_type == TOKEN_SEMI) && !parse_cmd_group()) {
            return 0;
        }
        
        // For & operator, the right operand is optional
        if (op_type == TOKEN_AMP && current_token.type != TOKEN_EOI) {
            if (!parse_cmd_group()) {
                return 0;
            }
        }
    }
    
    // Optional trailing &
    if (current_token.type == TOKEN_AMP) {
        get_next_token();
    }
    
    return current_token.type == TOKEN_EOI;
}

// Parse cmd_group ->  atomic (\| atomic)*
static int parse_cmd_group() {
    if (!parse_atomic()) {
        return 0;
    }
    
    while (current_token.type == TOKEN_PIPE) {
        get_next_token();
        
        if (!parse_atomic()) {
            return 0;
        }
    }
    
    return 1;
}

// Parse atomic -> name (name | input | output)*
static int parse_atomic() {
    if (!parse_name()) {
        return 0;
    }
    
    while (1) {
        if (current_token.type == TOKEN_NAME) {
            if (!parse_name()) {
                return 0;
            }
        } else if (current_token.type == TOKEN_LT) {
            if (!parse_input()) {
                return 0;
            }
        } else if (current_token.type == TOKEN_GT || current_token.type == TOKEN_GTGT) {
            if (!parse_output()) {
                return 0;
            }
        } else {
            break;
        }
    }
    
    return 1;
}

// Parse input -> < name | <name
static int parse_input() {
    if (current_token.type != TOKEN_LT) {
        return 0;
    }
    
    get_next_token();
    
    // We must have a name after <
    if (current_token.type != TOKEN_NAME) {
        return 0;
    }
    
    get_next_token();
    return 1;
}

// Parse output -> > name | >name | >> name | >>name
static int parse_output() {
    if (current_token.type != TOKEN_GT && current_token.type != TOKEN_GTGT) {
        return 0;
    }
    
    get_next_token();
    
    // We must have a name after > or >>
    if (current_token.type != TOKEN_NAME) {
        return 0;
    }
    
    get_next_token();
    return 1;
}

// Parse name -> r"[^|&><;]+"
static int parse_name() {
    if (current_token.type != TOKEN_NAME) {
        return 0;
    }
    
    get_next_token();
    return 1;
}

// Public function to parse a command
int parse_command(const char* input) {
    init_parser(input);
    int result = parse_shell_cmd();
    cleanup_parser();
    return result;
}

