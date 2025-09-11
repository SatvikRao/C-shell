#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "network.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port> [--chat] [loss_rate]\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    bool chat_mode = false;
    float loss_rate = 0.0;

    // Parse optional arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--chat") == 0) {
            chat_mode = true;
        } else {
            // Try to parse as loss_rate
            float temp = atof(argv[i]);
            if (temp >= 0.0 && temp <= 1.0) {
                loss_rate = temp;
            } else {
                fprintf(stderr, "Invalid loss rate: %s (must be between 0.0 and 1.0)\n", argv[i]);
                return 1;
            }
        }
    }

    // Seed random number generator for packet loss simulation
    srand(time(NULL));

    // Run the server
    return run_server(port, chat_mode, loss_rate);
}
