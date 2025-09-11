#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "network.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage for file transfer: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", argv[0]);
        fprintf(stderr, "Usage for chat mode: %s <server_ip> <server_port> --chat [loss_rate]\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    bool chat_mode = false;
    float loss_rate = 0.0;
    const char *input_file = NULL;
    const char *output_file = NULL;

    // Check for chat mode
    if (argc >= 4 && strcmp(argv[3], "--chat") == 0) {
        chat_mode = true;
        
        // Check if loss_rate is provided
        if (argc >= 5) {
            loss_rate = atof(argv[4]);
            if (loss_rate < 0.0 || loss_rate > 1.0) {
                fprintf(stderr, "Invalid loss rate: %s (must be between 0.0 and 1.0)\n", argv[4]);
                return 1;
            }
        }
    } else {
        // File transfer mode
        if (argc < 5) {
            fprintf(stderr, "Usage for file transfer: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n", argv[0]);
            return 1;
        }
        
        input_file = argv[3];
        output_file = argv[4];
        
        // Check if loss_rate is provided
        if (argc >= 6) {
            loss_rate = atof(argv[5]);
            if (loss_rate < 0.0 || loss_rate > 1.0) {
                fprintf(stderr, "Invalid loss rate: %s (must be between 0.0 and 1.0)\n", argv[5]);
                return 1;
            }
        }
    }

    // Seed random number generator for packet loss simulation
    srand(time(NULL));

    // Run the client
    return run_client(server_ip, server_port, input_file, output_file, chat_mode, loss_rate);
}
