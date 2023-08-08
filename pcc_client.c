#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Check the number of command line arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server IP address> <server port> <file path>\n", argv[0]);
        exit(1);
    }

    // Extract the command line arguments
    char *server_ip = argv[1];
    unsigned int server_port = atoi(argv[2]);
    char *file_path = argv[3];

    // Open the specified file for reading using file descriptors
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd == -1) {
        perror("Error opening file");
        exit(1);
    }

    // Create a TCP socket
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Error creating socket");
        exit(1);
    }

    // Set up the server address structure
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &(server_address.sin_addr)) <= 0) {
        perror("Error converting server IP address");
        exit(1);
    }

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error connecting to server");
        exit(1);
    }

    // Calculate the file size
    off_t file_size = lseek(file_fd, 0, SEEK_END);
    lseek(file_fd, 0, SEEK_SET);

    // Convert the file size to network byte order
    uint32_t file_size_network = htonl((uint32_t)file_size);

    // Send the file size to the server
    if (send(client_socket, &file_size_network, sizeof(file_size_network), 0) == -1) {
        perror("Error sending file size");
        exit(1);
    }

    // Read and send the file contents in chunks
    char buffer[MAX_BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        if (send(client_socket, buffer, bytes_read, 0) == -1) {
            perror("Error sending file contents");
            exit(1);
        }
    }

    // Receive the printable character count from the server
    uint32_t printable_count_network;
    if (recv(client_socket, &printable_count_network, sizeof(printable_count_network), 0) == -1) {
        perror("Error receiving printable character count");
        exit(1);
    }

    // Convert the printable character count to host byte order
    uint32_t printable_count = ntohl(printable_count_network);

    // Print the number of printable characters
    printf("# of printable characters: %u\n", printable_count);

    // Close the file and socket
    close(file_fd);
    close(client_socket);

    return 0;
}

