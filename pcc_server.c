#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Constants
#define MAX_BUFFER_SIZE 1024
#define NUM_CHARACTERS 95

uint32_t pcc_total[NUM_CHARACTERS] = {0};
volatile sig_atomic_t sigint_received = 0;

// Function to handle SIGINT signal
void handle_sigint(int sig) {
    sigint_received = 1;
}

// Function to handle client connections
void handle_client(int client_socket) {
    // Buffer to store received data from the client
    char buffer[MAX_BUFFER_SIZE];

    // Receive the number of bytes to be transferred (file size)
    uint32_t file_size;
    ssize_t bytes_received = read(client_socket, &file_size, sizeof(file_size));
    if (bytes_received == -1) {
        perror("Error receiving file size from client");
        close(client_socket);
        return;
    }

    // Convert the file size from network byte order to host byte order
    file_size = ntohl(file_size);

    // Receive the file content from the client
    uint32_t printable_count = 0;
    while (file_size > 0) {
        ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer));
        if (bytes_read == -1) {
            perror("Error receiving file content from client");
            close(client_socket);
            return;
        } else if (bytes_read == 0) {
            fprintf(stderr, "Unexpected connection close by the client\n");
            break;
        }

        // Compute the printable character count in the received buffer
        for (int i = 0; i < bytes_read; i++) {
            if (buffer[i] >= 32 && buffer[i] <= 126) {
                printable_count++;
                // Update the global data structure to maintain overall statistics
                pcc_total[buffer[i] - 32]++;
            }
        }

        // Decrease the remaining file size
        file_size -= bytes_read;
    }

    // Send the count of printable characters back to the client
    uint32_t printable_count_network = htonl(printable_count);
    ssize_t bytes_sent = write(client_socket, &printable_count_network, sizeof(printable_count_network));
    if (bytes_sent == -1) {
        perror("Error sending printable character count to client");
    }

    // Close the client socket
    close(client_socket);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    unsigned int server_port = atoi(argv[1]);

    // Set up signal handler for SIGINT
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    // Create the server socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Set socket options to reuse the address and port
    int reuse = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        perror("Setsockopt failed");
        exit(1);
    }

    // Bind the server socket to the specified port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) == -1) { // Set the listen queue size to 10
        perror("Listen failed");
        exit(1);
    }

    while (!sigint_received) {
        // Accept a client connection
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            if (errno == EINTR && sigint_received) {
                // SIGINT received, terminate the server gracefully
                break;
            } else {
                perror("Accept failed");
                continue;
            }
        }

        // Reset the character counts
        memset(pcc_total, 0, sizeof(pcc_total));

        // Handle the client connection in a separate function
        handle_client(client_socket);
    }

    // Print the count of each printable character
    for (int i = 0; i < NUM_CHARACTERS; i++) {
        printf("char '%c' : %u times\n", (char)(i + 32), pcc_total[i]);
    }

    // Close the server socket
    close(server_socket);

    return 0;
}

