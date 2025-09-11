#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>

// Server main function
int run_server(int port, bool chat_mode_enabled, float packet_loss_rate);

// Client main function
int run_client(const char *server_ip, int server_port, const char *input_file, 
               const char *output_file, bool chat_mode_enabled, float packet_loss_rate);

#endif // NETWORK_H
