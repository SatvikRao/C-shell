#include <stdio.h>
#include <stdlib.h>
#include "shell.h"

int main() {
    // Initialize shell
    if (initialize_shell() != 0) {
        fprintf(stderr, "Failed to initialize shell\n");
        return EXIT_FAILURE;
    }
    
    // Run the shell's main loop
    run_shell();
    
    return EXIT_SUCCESS;
}
