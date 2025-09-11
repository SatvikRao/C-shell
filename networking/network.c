#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/select.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <openssl/md5.h>
#include <stdarg.h>

#define MAX_PACKET_SIZE 1024
#define MAX_WINDOW_SIZE 10
#define TIMEOUT_MS 500
#define BUFFER_SIZE 65535

// S.H.A.M. Header Structure
struct sham_header {
    uint32_t seq_num;    // Sequence Number
    uint32_t ack_num;    // Acknowledgment Number
    uint16_t flags;      // Control flags (SYN, ACK, FIN)
    uint16_t window_size; // Flow control window size
};

// Control flags
#define SYN_FLAG 0x1
#define ACK_FLAG 0x2
#define FIN_FLAG 0x4

// Full packet structure
struct packet {
    struct sham_header header;
    char data[MAX_PACKET_SIZE];
    size_t data_len;
};

// Track sent packets for retransmission
struct sent_packet {
    struct packet pkt;
    struct timeval sent_time;
    bool acked;
};

// Global variables
FILE* log_file = NULL;
bool verbose_logging = false;
float loss_rate = 0.0;

// Function to initialize logging
void init_logging(const char* role) {
    char* log_env = getenv("RUDP_LOG");
    if (log_env && strcmp(log_env, "1") == 0) {
        char log_filename[20];
        sprintf(log_filename, "%s_log.txt", role);
        log_file = fopen(log_filename, "w");
        verbose_logging = true;
    }
}

// Function to write log messages
void log_message(const char* format, ...) {
    if (!verbose_logging || !log_file) return;
    
    char time_buffer[30];
    struct timeval tv;
    time_t curtime;
    
    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    
    strftime(time_buffer, 30, "%Y-%m-%d %H:%M:%S", localtime(&curtime));
    
    fprintf(log_file, "[%s.%06ld] [LOG] ", time_buffer, tv.tv_usec);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fflush(log_file);
}

// Simulate packet loss
bool should_drop_packet() {
    return ((float)rand() / RAND_MAX) < loss_rate;
}

// Send a packet with potential simulated loss
int send_packet(int sockfd, const struct packet *pkt, const struct sockaddr *addr, socklen_t addr_len) {
    if (should_drop_packet()) {
        if (verbose_logging) {
            log_message("DROP DATA SEQ=%u", pkt->header.seq_num);
        }
        return 0;  // Simulate successful send but packet is actually dropped
    }
    
    // Create a temporary packet with network byte order for transmission
    struct packet temp_pkt = *pkt;
    temp_pkt.header.seq_num = htonl(pkt->header.seq_num);
    temp_pkt.header.ack_num = htonl(pkt->header.ack_num);
    temp_pkt.header.flags = htons(pkt->header.flags);
    temp_pkt.header.window_size = htons(pkt->header.window_size);
    
    // Combine header and data into a single buffer
    char buffer[sizeof(struct sham_header) + MAX_PACKET_SIZE];
    memcpy(buffer, &temp_pkt.header, sizeof(struct sham_header));
    memcpy(buffer + sizeof(struct sham_header), pkt->data, pkt->data_len);
    
    return sendto(sockfd, buffer, sizeof(struct sham_header) + pkt->data_len, 0, addr, addr_len);
}

// Receive a packet
int recv_packet(int sockfd, struct packet *pkt, struct sockaddr *addr, socklen_t *addr_len) {
    char buffer[sizeof(struct sham_header) + MAX_PACKET_SIZE];
    
    int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0, addr, addr_len);
    if (bytes_received <= 0) return bytes_received;
    
    // Extract header
    memcpy(&pkt->header, buffer, sizeof(struct sham_header));
    
    // Convert header fields from network to host byte order
    pkt->header.seq_num = ntohl(pkt->header.seq_num);
    pkt->header.ack_num = ntohl(pkt->header.ack_num);
    pkt->header.flags = ntohs(pkt->header.flags);
    pkt->header.window_size = ntohs(pkt->header.window_size);
    
    // Extract data
    pkt->data_len = bytes_received - sizeof(struct sham_header);
    if (pkt->data_len > 0) {
        memcpy(pkt->data, buffer + sizeof(struct sham_header), pkt->data_len);
    }
    
    return bytes_received;
}

// Prepare a packet for sending
void prepare_packet(struct packet *pkt, uint32_t seq_num, uint32_t ack_num, 
                   uint16_t flags, uint16_t window_size, const char *data, size_t data_len) {
    // Store values in host byte order first (for logging purposes)
    pkt->header.seq_num = seq_num;
    pkt->header.ack_num = ack_num;
    pkt->header.flags = flags;
    pkt->header.window_size = window_size;
    
    if (data && data_len > 0) {
        memcpy(pkt->data, data, data_len);
        pkt->data_len = data_len;
    } else {
        pkt->data_len = 0;
    }
}

// Client-side three-way handshake
bool client_handshake(int sockfd, struct sockaddr *server_addr, socklen_t addr_len, uint32_t *client_seq, uint32_t *server_seq) {
    struct packet pkt;
    *client_seq = rand() % 10000;  // Random initial sequence number
    
    // Set socket to non-blocking to handle timeouts manually
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // Step 1: Send SYN with retries
    int retry_count = 0;
    const int max_retries = 5;
    
    while (retry_count < max_retries) {
        // Prepare and send SYN packet
        prepare_packet(&pkt, *client_seq, 0, SYN_FLAG, BUFFER_SIZE, NULL, 0);
        if (send_packet(sockfd, &pkt, server_addr, addr_len) < 0) {
            perror("Failed to send SYN packet");
            fcntl(sockfd, F_SETFL, flags); // Restore flags
            return false;
        }
        log_message("SND SYN SEQ=%u", *client_seq);
        
        // Wait for SYN-ACK with timeout
        fd_set read_fds;
        struct timeval timeout;
        timeout.tv_sec = 2;  // 2 seconds timeout
        timeout.tv_usec = 0;
        
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        
        int select_result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result > 0) {
            // Socket is ready for reading
            if (recv_packet(sockfd, &pkt, server_addr, &addr_len) > 0) {
                if ((pkt.header.flags & (SYN_FLAG | ACK_FLAG)) == (SYN_FLAG | ACK_FLAG)) {
                    *server_seq = pkt.header.seq_num;
                    log_message("RCV SYN-ACK SEQ=%u ACK=%u", *server_seq, pkt.header.ack_num);
                    
                    if (pkt.header.ack_num != (*client_seq + 1)) {
                        fprintf(stderr, "Handshake failed: Incorrect ACK number\n");
                        fcntl(sockfd, F_SETFL, flags);  // Restore original socket flags
                        return false;
                    }
                    
                    // Step 3: Send ACK
                    prepare_packet(&pkt, *client_seq + 1, *server_seq + 1, ACK_FLAG, BUFFER_SIZE, NULL, 0);
                    send_packet(sockfd, &pkt, server_addr, addr_len);
                    log_message("SND ACK=%u", *server_seq + 1);
                    
                    // Update client sequence number
                    (*client_seq)++;
                    
                    // Restore original socket flags
                    fcntl(sockfd, F_SETFL, flags);
                    return true;
                }
            }
        }
        
        // Timeout or error, retry
        printf("Retrying handshake (attempt %d/%d)...\n", retry_count + 1, max_retries);
        retry_count++;
    }
    
    // Restore original socket flags
    fcntl(sockfd, F_SETFL, flags);
    perror("Handshake failed: No SYN-ACK received after multiple attempts");
    return false;
}

// Server-side three-way handshake
bool server_handshake(int sockfd, struct sockaddr *client_addr, socklen_t *addr_len, uint32_t *server_seq, uint32_t *client_seq) {
    struct packet pkt;
    
    // Make socket non-blocking for select()
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
    
    // Wait for SYN with timeout
    fd_set read_fds;
    struct timeval timeout;
    
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    
    timeout.tv_sec = 30;  // 30 seconds timeout for initial connection
    timeout.tv_usec = 0;
    
    printf("Server waiting for client connection...\n");
    
    int select_result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (select_result <= 0) {
        perror("Server timed out waiting for connection");
        fcntl(sockfd, F_SETFL, flags); // Restore flags
        return false;
    }
    
    // Step 1: Wait for SYN
    if (recv_packet(sockfd, &pkt, client_addr, addr_len) <= 0) {
        perror("Failed to receive SYN packet");
        fcntl(sockfd, F_SETFL, flags); // Restore flags
        return false;
    }
    
    if ((pkt.header.flags & SYN_FLAG) != SYN_FLAG) {
        fprintf(stderr, "Expected SYN flag in initial packet\n");
        fcntl(sockfd, F_SETFL, flags); // Restore flags
        return false;
    }
    
    *client_seq = pkt.header.seq_num;
    log_message("RCV SYN SEQ=%u", *client_seq);
    
    // Step 2: Send SYN-ACK
    *server_seq = rand() % 10000;  // Random initial sequence number
    prepare_packet(&pkt, *server_seq, *client_seq + 1, SYN_FLAG | ACK_FLAG, BUFFER_SIZE, NULL, 0);
    if (send_packet(sockfd, &pkt, client_addr, *addr_len) < 0) {
        perror("Failed to send SYN-ACK packet");
        fcntl(sockfd, F_SETFL, flags); // Restore flags
        return false;
    }
    log_message("SND SYN-ACK SEQ=%u ACK=%u", *server_seq, *client_seq + 1);
    
    // Step 3: Wait for ACK with timeout
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    
    timeout.tv_sec = 5;  // 5 seconds timeout for ACK
    timeout.tv_usec = 0;
    
    select_result = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (select_result <= 0) {
        perror("Timed out waiting for ACK");
        fcntl(sockfd, F_SETFL, flags); // Restore flags
        return false;
    }
    
    if (recv_packet(sockfd, &pkt, client_addr, addr_len) <= 0) {
        perror("Failed to receive ACK packet");
        fcntl(sockfd, F_SETFL, flags); // Restore flags
        return false;
    }
    
    if ((pkt.header.flags & ACK_FLAG) != ACK_FLAG) {
        fprintf(stderr, "Expected ACK flag in response\n");
        fcntl(sockfd, F_SETFL, flags); // Restore flags
        return false;
    }
    
    log_message("RCV ACK FOR SYN");
    
    if (pkt.header.ack_num != (*server_seq + 1)) {
        fprintf(stderr, "Incorrect ACK number received\n");
        fcntl(sockfd, F_SETFL, flags); // Restore flags
        return false;
    }
    
    // Update server sequence number
    (*server_seq)++;
    
    // Restore socket flags
    fcntl(sockfd, F_SETFL, flags);
    
    return true;
}

// Client-side four-way termination
bool client_terminate(int sockfd, struct sockaddr *server_addr, socklen_t addr_len, uint32_t seq_num, uint32_t ack_num) {
    struct packet pkt;
    
    // Step 1: Send FIN
    prepare_packet(&pkt, seq_num, ack_num, FIN_FLAG, BUFFER_SIZE, NULL, 0);
    send_packet(sockfd, &pkt, server_addr, addr_len);
    log_message("SND FIN SEQ=%u", seq_num);
    
    // Step 2: Wait for ACK
    if (recv_packet(sockfd, &pkt, server_addr, &addr_len) <= 0) {
        perror("Termination failed: No ACK received");
        return false;
    }
    
    if ((pkt.header.flags & ACK_FLAG) != ACK_FLAG) {
        fprintf(stderr, "Termination failed: Expected ACK flag\n");
        return false;
    }
    
    log_message("RCV ACK FOR FIN");
    
    // Step 3: Wait for FIN from server
    if (recv_packet(sockfd, &pkt, server_addr, &addr_len) <= 0) {
        perror("Termination failed: No FIN received");
        return false;
    }
    
    if ((pkt.header.flags & FIN_FLAG) != FIN_FLAG) {
        fprintf(stderr, "Termination failed: Expected FIN flag\n");
        return false;
    }
    
    log_message("RCV FIN SEQ=%u", pkt.header.seq_num);
    
    // Step 4: Send ACK
    prepare_packet(&pkt, seq_num + 1, pkt.header.seq_num + 1, ACK_FLAG, BUFFER_SIZE, NULL, 0);
    send_packet(sockfd, &pkt, server_addr, addr_len);
    log_message("SND ACK=%u", pkt.header.seq_num + 1);
    
    return true;
}

// Server-side four-way termination
bool server_terminate(int sockfd, struct sockaddr *client_addr, socklen_t addr_len, uint32_t seq_num, uint32_t ack_num) {
    struct packet pkt;
    
    // Step 1: Wait for FIN
    if (recv_packet(sockfd, &pkt, client_addr, &addr_len) <= 0) {
        return false;
    }
    
    if ((pkt.header.flags & FIN_FLAG) != FIN_FLAG) {
        return false;
    }
    
    log_message("RCV FIN SEQ=%u", pkt.header.seq_num);
    
    // Step 2: Send ACK
    prepare_packet(&pkt, seq_num, pkt.header.seq_num + 1, ACK_FLAG, BUFFER_SIZE, NULL, 0);
    send_packet(sockfd, &pkt, client_addr, addr_len);
    log_message("SND ACK FOR FIN");
    
    // Step 3: Send FIN
    prepare_packet(&pkt, seq_num, ack_num, FIN_FLAG, BUFFER_SIZE, NULL, 0);
    send_packet(sockfd, &pkt, client_addr, addr_len);
    log_message("SND FIN SEQ=%u", seq_num);
    
    // Step 4: Wait for ACK
    if (recv_packet(sockfd, &pkt, client_addr, &addr_len) <= 0) {
        perror("Termination failed: No ACK received");
        return false;
    }
    
    if ((pkt.header.flags & ACK_FLAG) != ACK_FLAG) {
        fprintf(stderr, "Termination failed: Expected ACK flag\n");
        return false;
    }
    
    log_message("RCV ACK=%u", pkt.header.ack_num);
    
    return true;
}

// Calculate time difference in milliseconds
long time_diff_ms(struct timeval *start, struct timeval *end) {
    return (end->tv_sec - start->tv_sec) * 1000 + (end->tv_usec - start->tv_usec) / 1000;
}

// Send a file using reliable UDP
bool send_file(int sockfd, struct sockaddr *dest_addr, socklen_t addr_len, 
               const char *filename, uint32_t *seq_num, uint32_t *ack_num) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return false;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    printf("File size: %ld bytes\n", file_size);
    
    char buffer[MAX_PACKET_SIZE];
    struct packet pkt;
    struct sent_packet window[MAX_WINDOW_SIZE];
    int window_start = 0;
    int window_end = 0;
    uint16_t receiver_window = BUFFER_SIZE;
    bool file_ended = false;
    
    // Initialize window
    for (int i = 0; i < MAX_WINDOW_SIZE; i++) {
        window[i].acked = true;  // Mark as acked initially (not in use)
    }
    
    fd_set read_fds;
    struct timeval timeout;
    
    while (1) {
        int window_size = (window_end - window_start + MAX_WINDOW_SIZE) % MAX_WINDOW_SIZE;
        if (window_size < 0 || window_size > MAX_WINDOW_SIZE) {
            window_size = 0;  // Safety check for corrupt window size
        }
        
        // Fill window with new packets if space available and more data to send
        while (window_size < MAX_WINDOW_SIZE && !file_ended && 
               (window_size * MAX_PACKET_SIZE) < receiver_window) {
            
            int idx = window_end;
            
            size_t bytes_read = fread(buffer, 1, MAX_PACKET_SIZE, file);
            if (bytes_read == 0) {
                file_ended = true;
                break;
            }
            
            prepare_packet(&pkt, *seq_num, *ack_num, 0, BUFFER_SIZE, buffer, bytes_read);
            
            if (send_packet(sockfd, &pkt, dest_addr, addr_len) < 0) {
                perror("Failed to send data packet");
                // Continue anyway - may be temporary
            } else {
                log_message("SND DATA SEQ=%u LEN=%zu", *seq_num, bytes_read);
            }
            
            // Store packet in window
            window[idx].pkt = pkt;
            gettimeofday(&window[idx].sent_time, NULL);
            window[idx].acked = false;
            
            // Update sequence number and window
            *seq_num += bytes_read;
            window_end = (window_end + 1) % MAX_WINDOW_SIZE;
            window_size = (window_end - window_start + MAX_WINDOW_SIZE) % MAX_WINDOW_SIZE;
        }
        
        // Check for acks and timeouts
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;  // 10ms
        
        int ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ret > 0) {
            // Read acknowledgment
            if (recv_packet(sockfd, &pkt, dest_addr, &addr_len) > 0) {
                if (pkt.header.flags & ACK_FLAG) {
                    log_message("RCV ACK=%u", pkt.header.ack_num);
                    
                    // Update receiver window
                    uint16_t new_window = pkt.header.window_size;
                    if (new_window != receiver_window) {
                        log_message("FLOW WIN UPDATE=%u", new_window);
                        receiver_window = new_window;
                    }
                    
                    // Mark packets as acknowledged
                    int idx = window_start;
                    while (idx != window_end) {
                        if (!window[idx].acked) {
                            uint32_t packet_seq = window[idx].pkt.header.seq_num;
                            uint32_t packet_len = window[idx].pkt.data_len;
                            
                            if (packet_seq + packet_len <= pkt.header.ack_num) {
                                window[idx].acked = true;
                                if (idx == window_start) {
                                    // Move window start forward
                                    do {
                                        window_start = (window_start + 1) % MAX_WINDOW_SIZE;
                                        if (window_start == window_end) break; // Prevent overrun
                                    } while (window[window_start].acked);
                                }
                            }
                        }
                        idx = (idx + 1) % MAX_WINDOW_SIZE;
                        if (idx == window_end) break; // Prevent infinite loop
                    }
                }
            }
        }
        
        // Check for timeouts and retransmit
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        
        int idx = window_start;
        while (idx != window_end) {
            if (!window[idx].acked) {
                if (time_diff_ms(&window[idx].sent_time, &current_time) > TIMEOUT_MS) {
                    // Timeout occurred, retransmit
                    uint32_t packet_seq = window[idx].pkt.header.seq_num;
                    log_message("TIMEOUT SEQ=%u", packet_seq);
                    
                    if (send_packet(sockfd, &window[idx].pkt, dest_addr, addr_len) >= 0) {
                        log_message("RETX DATA SEQ=%u LEN=%zu", packet_seq, window[idx].pkt.data_len);
                    }
                    
                    // Update sent time
                    gettimeofday(&window[idx].sent_time, NULL);
                }
            }
            idx = (idx + 1) % MAX_WINDOW_SIZE;
            if (idx == window_end) break; // Prevent infinite loop
        }
        
        // Check if we're done
        if (file_ended && window_start == window_end) {
            printf("File transfer complete\n");
            break;
        }
    }
    
    fclose(file);
    return true;
}

// Receive a file using reliable UDP
bool receive_file(int sockfd, struct sockaddr *src_addr, socklen_t *addr_len, 
                 const char *output_filename, uint32_t *seq_num, uint32_t *ack_num) {
    FILE *file = fopen(output_filename, "wb");
    if (!file) {
        perror("Failed to create output file");
        return false;
    }
    
    struct packet pkt;
    // Define a safe size for the buffer array - previously was using BUFFER_SIZE which could be too large
    const int buffer_array_size = 1024;
    struct packet *buffer = malloc(buffer_array_size * sizeof(struct packet));
    bool *received = calloc(buffer_array_size, sizeof(bool));
    
    if (!buffer || !received) {
        perror("Memory allocation failed");
        fclose(file);
        free(buffer);
        free(received);
        return false;
    }
    
    uint32_t expected_seq = *ack_num;  // Next expected sequence number
    uint16_t window_size = BUFFER_SIZE;  // Flow control window size
    
    fd_set read_fds;
    struct timeval timeout;
    bool connection_active = true;
    
    while (connection_active) {
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        timeout.tv_sec = 5;  // 5 seconds timeout
        timeout.tv_usec = 0;
        
        int ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (ret <= 0) {
            // Timeout or error - assume connection ended if no data for 5 seconds
            connection_active = false;
            continue;
        }
        
        if (recv_packet(sockfd, &pkt, src_addr, addr_len) > 0) {
            if (pkt.header.flags & FIN_FLAG) {
                // Connection termination initiated
                *ack_num = pkt.header.seq_num + 1;
                connection_active = false;
                break;
            }
            
            uint32_t recv_seq = pkt.header.seq_num;
            log_message("RCV DATA SEQ=%u LEN=%zu", recv_seq, pkt.data_len);
            
            if (recv_seq == expected_seq) {
                // In-order packet, write to file
                fwrite(pkt.data, 1, pkt.data_len, file);
                expected_seq += pkt.data_len;
                
                // Check if we have any buffered packets that can now be written
                uint32_t next_seq = expected_seq;
                while (next_seq < expected_seq + buffer_array_size && 
                       received[next_seq % buffer_array_size] && 
                       buffer[next_seq % buffer_array_size].header.seq_num == next_seq) {
                    
                    fwrite(buffer[next_seq % buffer_array_size].data, 1, 
                           buffer[next_seq % buffer_array_size].data_len, file);
                    received[next_seq % buffer_array_size] = false;
                    next_seq += buffer[next_seq % buffer_array_size].data_len;
                }
                
                expected_seq = next_seq;
            } else if (recv_seq > expected_seq && recv_seq < expected_seq + buffer_array_size) {
                // Out-of-order packet, buffer it if within range
                uint32_t idx = recv_seq % buffer_array_size;
                buffer[idx] = pkt;
                received[idx] = true;
            }
            
            // Send ACK for the highest contiguous byte received
            *ack_num = expected_seq;
            prepare_packet(&pkt, *seq_num, *ack_num, ACK_FLAG, window_size, NULL, 0);
            send_packet(sockfd, &pkt, src_addr, *addr_len);
            log_message("SND ACK=%u WIN=%u", *ack_num, window_size);
        }
    }
    
    fclose(file);
    free(buffer);
    free(received);
    return true;
}

// Calculate MD5 hash of a file
void calculate_md5(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file for MD5 calculation");
        return;
    }
    
    MD5_CTX md5_context;
    MD5_Init(&md5_context);
    
    unsigned char buffer[1024];
    size_t bytes;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        MD5_Update(&md5_context, buffer, bytes);
    }
    
    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5_context);
    
    fclose(file);
    
    printf("MD5: ");
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        printf("%02x", result[i]);
    }
    printf("\n");
}

// Chat mode function
void chat_mode(int sockfd, struct sockaddr *addr, socklen_t addr_len, 
               uint32_t *seq_num, uint32_t *ack_num, bool is_client) {
    char input_buffer[MAX_PACKET_SIZE];
    struct packet pkt;
    fd_set read_fds;
    bool chat_active = true;
    
    printf("Chat mode active. Type /quit to exit.\n");
    
    while (chat_active) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);  // Monitor keyboard input
        FD_SET(sockfd, &read_fds);       // Monitor socket
        
        // Wait for input or network data
        if (select(sockfd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("select() failed");
            break;
        }
        
        // Handle keyboard input
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (!fgets(input_buffer, MAX_PACKET_SIZE, stdin)) {
                break;
            }
            
            // Remove newline character
            size_t len = strlen(input_buffer);
            if (len > 0 && input_buffer[len - 1] == '\n') {
                input_buffer[len - 1] = '\0';
                len--;
            }
            
            if (strcmp(input_buffer, "/quit") == 0) {
                // Initiate termination
                if (is_client) {
                    client_terminate(sockfd, addr, addr_len, *seq_num, *ack_num);
                } else {
                    server_terminate(sockfd, addr, addr_len, *seq_num, *ack_num);
                }
                chat_active = false;
                break;
            }
            
            // Send the message
            prepare_packet(&pkt, *seq_num, *ack_num, 0, BUFFER_SIZE, input_buffer, len + 1);
            send_packet(sockfd, &pkt, addr, addr_len);
            log_message("SND DATA SEQ=%u LEN=%zu", *seq_num, len + 1);
            *seq_num += len + 1;
        }
        
        // Handle network data
        if (FD_ISSET(sockfd, &read_fds)) {
            if (recv_packet(sockfd, &pkt, addr, &addr_len) > 0) {
                if (pkt.header.flags & FIN_FLAG) {
                    // Peer wants to terminate
                    if (is_client) {
                        // Client responds to server's FIN
                        prepare_packet(&pkt, *seq_num, pkt.header.seq_num + 1, ACK_FLAG, BUFFER_SIZE, NULL, 0);
                        send_packet(sockfd, &pkt, addr, addr_len);
                        log_message("SND ACK FOR FIN");
                        
                        prepare_packet(&pkt, *seq_num, *ack_num, FIN_FLAG, BUFFER_SIZE, NULL, 0);
                        send_packet(sockfd, &pkt, addr, addr_len);
                        log_message("SND FIN SEQ=%u", *seq_num);
                        
                        // Wait for final ACK
                        recv_packet(sockfd, &pkt, addr, &addr_len);
                    } else {
                        // Server responds to client's FIN
                        prepare_packet(&pkt, *seq_num, pkt.header.seq_num + 1, ACK_FLAG, BUFFER_SIZE, NULL, 0);
                        send_packet(sockfd, &pkt, addr, addr_len);
                        log_message("SND ACK FOR FIN");
                        
                        prepare_packet(&pkt, *seq_num, *ack_num, FIN_FLAG, BUFFER_SIZE, NULL, 0);
                        send_packet(sockfd, &pkt, addr, addr_len);
                        log_message("SND FIN SEQ=%u", *seq_num);
                        
                        // Wait for final ACK
                        recv_packet(sockfd, &pkt, addr, &addr_len);
                    }
                    
                    chat_active = false;
                    break;
                } else if (pkt.data_len > 0) {
                    // Regular chat message
                    printf("Peer: %s\n", pkt.data);
                    
                    // Update ack number and send ACK
                    *ack_num = pkt.header.seq_num + pkt.data_len;
                    prepare_packet(&pkt, *seq_num, *ack_num, ACK_FLAG, BUFFER_SIZE, NULL, 0);
                    send_packet(sockfd, &pkt, addr, addr_len);
                    log_message("SND ACK=%u WIN=%u", *ack_num, BUFFER_SIZE);
                }
            }
        }
    }
}

// Server main function
int run_server(int port, bool chat_mode_enabled, float packet_loss_rate) {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    // Initialize logging
    init_logging("server");
    loss_rate = packet_loss_rate;
    
    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    // Set socket options to allow reuse of address/port
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    memset(&client_addr, 0, sizeof(client_addr));
    
    // Fill server information
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    // Bind the socket
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return -1;
    }
    
    printf("Server listening on port %d\n", port);
    
    // Initialize sequence numbers
    uint32_t server_seq, client_seq;
    uint32_t ack_num;
    
    // Perform handshake
    if (!server_handshake(sockfd, (struct sockaddr *)&client_addr, &client_addr_len, &server_seq, &client_seq)) {
        fprintf(stderr, "Handshake failed\n");
        close(sockfd);
        return -1;
    }
    
    printf("Connection established with client %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    ack_num = client_seq + 1;
    
    // Handle data according to mode
    if (chat_mode_enabled) {
        chat_mode(sockfd, (struct sockaddr *)&client_addr, client_addr_len, &server_seq, &ack_num, false);
    } else {
        // Receive file
        if (receive_file(sockfd, (struct sockaddr *)&client_addr, &client_addr_len, "received_file", &server_seq, &ack_num)) {
            // Calculate and print MD5
            calculate_md5("received_file");
        }
        
        // Wait for connection termination
        server_terminate(sockfd, (struct sockaddr *)&client_addr, client_addr_len, server_seq, ack_num);
    }
    
    if (log_file) {
        fclose(log_file);
    }
    close(sockfd);
    return 0;
}

// Client main function
int run_client(const char *server_ip, int server_port, const char *input_file, 
               const char *output_file, bool chat_mode_enabled, float packet_loss_rate) {
    int sockfd;
    struct sockaddr_in server_addr;
    
    // Initialize logging
    init_logging("client");
    loss_rate = packet_loss_rate;
    
    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    
    // Fill server information
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sockfd);
        return -1;
    }
    
    printf("Connecting to server %s:%d\n", server_ip, server_port);
    
    // Initialize sequence numbers
    uint32_t client_seq, server_seq;
    uint32_t ack_num;
    
    // Perform handshake
    if (!client_handshake(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr), &client_seq, &server_seq)) {
        fprintf(stderr, "Handshake failed\n");
        close(sockfd);
        return -1;
    }
    
    printf("Connection established with server\n");
    
    ack_num = server_seq + 1;
    
    // Handle data according to mode
    if (chat_mode_enabled) {
        chat_mode(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr), &client_seq, &ack_num, true);
    } else {
        // Send file
        if (!send_file(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr), input_file, &client_seq, &ack_num)) {
            fprintf(stderr, "File transfer failed\n");
        }
        
        // Initiate connection termination
        client_terminate(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr), client_seq, ack_num);
    }
    
    if (log_file) {
        fclose(log_file);
    }
    close(sockfd);
    return 0;
}
