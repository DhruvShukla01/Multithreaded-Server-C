#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 128

// Struct to pass to the client handler thread
struct client_info {
    int socket;
    struct sockaddr_in address;
};

// Function to handle communication with a single client
void *handle_client(void *arg) {
    // Cast the argument back to the client_info struct
    struct client_info *info = (struct client_info *)arg;
    int client_socket = info->socket;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &info->address.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(info->address.sin_port);

    char buffer[BUFFER_SIZE];
    ssize_t read_size; // Use ssize_t for read/recv return values

    printf("Connection accepted from %s:%d\n", client_ip, client_port);

    // Welcome message to the client
    char *welcome_message = "Welcome to the C multithreaded server!\n";
    write(client_socket, welcome_message, strlen(welcome_message));

    // Receive messages from the client
    while ((read_size = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        
        // --- CHANGE: Robust Echo Logic ---
        // The original logic was flawed because strlen() stops at the first null byte,
        // which could be part of the client's message. Echoing back `read_size` bytes
        // ensures the echo is a perfect mirror of what was received.
        
        // Null-terminate for safe printing on the server side.
        // This is safe because our buffer is larger than what we read.
        if (read_size < BUFFER_SIZE) {
            buffer[read_size] = '\0';
        }
        
        // Use `%.*s` to print exactly `read_size` bytes to avoid reading past the received data.
        printf("Received %zd bytes from %s:%d: %.*s", read_size, client_ip, client_port, (int)read_size, buffer);
        
        // OLD CODE:
        // write(client_socket, buffer, strlen(buffer));

        // NEW CODE: Echo the exact number of bytes received.
        write(client_socket, buffer, read_size);
        // --- END CHANGE ---
        
        // Clear the buffer for the next message - this is not strictly necessary
        // as the next recv will overwrite it, but can be good practice.
        // memset(buffer, 0, BUFFER_SIZE);
    }

    if (read_size == 0) {
        printf("Client %s:%d disconnected\n", client_ip, client_port);
    } else if (read_size == -1) {
        perror("recv failed");
    }

    // Clean up
    close(client_socket);
    free(info); // Free the memory allocated for the struct
    pthread_exit(NULL);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t thread_id;

    // 1. Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Could not create socket");
        exit(EXIT_FAILURE);
    }
    printf("Socket created.\n");

    // Prepare the sockaddr_in structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on any network interface
    server_addr.sin_port = htons(PORT);

    // 2. Bind the socket to the address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Bind successful.\n");

    // 3. Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    
    printf("Server listening on port %d...\n", PORT);
    printf("Waiting for incoming connections...\n");

    // 4. Accept incoming connections and spawn threads
    while (1) {
        // Accept a new connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue; // Continue to the next iteration to accept other connections
        }

        // Allocate memory for client info to pass to the thread
        struct client_info *info = malloc(sizeof(struct client_info));
        if (info == NULL) {
            perror("Could not allocate memory for client info");
            close(client_socket);
            continue;
        }
        info->socket = client_socket;
        info->address = client_addr;

        // Create a new thread to handle the client
        if (pthread_create(&thread_id, NULL, handle_client, (void *)info) != 0) {
            perror("Could not create thread");
            free(info);
            close(client_socket);
        }

        // Detach the thread so its resources are automatically released on exit
        pthread_detach(thread_id);
    }

    // This part is not reached in this simple example
    close(server_socket);
    return 0;
}