#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <netdb.h>

// Struct to hold benchmark parameters for each thread
struct benchmark_args {
    const char *host;
    int port;
    int num_messages;
    int message_size;
    long *total_bytes_sent;
    long *total_messages_sent;
};

// Function for a single client thread to execute
void *client_thread_func(void *arg) {
    struct benchmark_args *params = (struct benchmark_args *)arg;
    int sock;
    struct sockaddr_in serv_addr;
    char *message_buffer;
    char *receive_buffer;

    // Allocate buffers
    message_buffer = malloc(params->message_size);
    receive_buffer = malloc(params->message_size + 1);
    if (!message_buffer || !receive_buffer) {
        perror("Failed to allocate memory for buffers");
        return NULL;
    }
    memset(message_buffer, 'A', params->message_size);

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        goto cleanup_buffers;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(params->port);

    if (inet_pton(AF_INET, params->host, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        goto cleanup_socket;
    }

    // Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        goto cleanup_socket;
    }

    // --- CHANGE: Robustly consume the server's welcome message ---
    // The old method was a single read(), which is unreliable. It might not read
    // the whole message, or it might read part of the first real echo,
    // causing the benchmark to fail. This loop reads until the newline
    // character is found, ensuring we are synchronized before starting the test.
    
    // OLD CODE:
    // read(sock, receive_buffer, params->message_size);

    // NEW CODE:
    char temp_char;
    while (read(sock, &temp_char, 1) > 0 && temp_char != '\n');
    // --- END CHANGE ---


    // Send messages and receive echoes
    for (int i = 0; i < params->num_messages; i++) {
        if (send(sock, message_buffer, params->message_size, 0) < 0) {
            perror("Send failed");
            break;
        }

        int bytes_received = 0;
        while (bytes_received < params->message_size) {
            int result = read(sock, receive_buffer + bytes_received, params->message_size - bytes_received);
            if (result <= 0) {
                perror("Read failed or connection closed");
                goto cleanup_socket;
            }
            bytes_received += result;
        }
        
        __sync_fetch_and_add(params->total_bytes_sent, params->message_size);
        __sync_fetch_and_add(params->total_messages_sent, 1);
    }

cleanup_socket:
    close(sock);
cleanup_buffers:
    free(message_buffer);
    free(receive_buffer);
    return NULL;
}

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s -h <host> -p <port> -c <clients> -n <messages> -s <size>\n", prog_name);
    // ... (rest of function is unchanged)
}

int main(int argc, char *argv[]) {
    const char *host = "127.0.0.1";
    int port = 8080;
    int num_clients = 10;
    int num_messages = 100;
    int message_size = 128;
    int opt;

    // Parse command-line arguments
    while ((opt = getopt(argc, argv, "h:p:c:n:s:")) != -1) {
        switch (opt) {
            case 'h': host = optarg; break;
            case 'p': port = atoi(optarg); break;
            case 'c': num_clients = atoi(optarg); break;
            case 'n': num_messages = atoi(optarg); break;
            case 's': message_size = atoi(optarg); break;
            default:
                print_usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    printf("--- Starting Benchmark ---\n");
    printf("Host: %s, Port: %d\n", host, port);
    printf("Concurrent Clients: %d\n", num_clients);
    printf("Messages per Client: %d\n", num_messages);
    printf("Message Size: %d bytes\n", message_size);
    printf("--------------------------\n");

    pthread_t *threads = malloc(num_clients * sizeof(pthread_t));
    if (!threads) {
        perror("Failed to allocate memory for threads");
        exit(EXIT_FAILURE);
    }

    long total_bytes_sent = 0;
    long total_messages_sent = 0;

    struct benchmark_args args = {
        .host = host,
        .port = port,
        .num_messages = num_messages,
        .message_size = message_size,
        .total_bytes_sent = &total_bytes_sent,
        .total_messages_sent = &total_messages_sent,
    };

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < num_clients; i++) {
        if (pthread_create(&threads[i], NULL, client_thread_func, &args) != 0) {
            perror("Failed to create thread");
        }
    }

    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("\n--- Benchmark Results ---\n");
    if (elapsed_time > 0) {
        double messages_per_second = total_messages_sent / elapsed_time;
        double mbps = (total_bytes_sent / elapsed_time) / (1024 * 1024);
        
        printf("Total Time:           %.2f seconds\n", elapsed_time);
        printf("Total Messages Sent:  %ld\n", total_messages_sent);
        printf("Total Data Sent:      %.2f MB\n", (double)total_bytes_sent / (1024 * 1024));
        printf("Throughput:           %.2f messages/sec\n", messages_per_second);
        printf("Data Rate:            %.2f MB/s\n", mbps);
    } else {
        printf("Benchmark completed too quickly to measure.\n");
    }
    printf("-------------------------\n");

    free(threads);
    return 0;
}