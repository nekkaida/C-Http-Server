#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

// Function to extract the path from an HTTP request
char* extract_path(char* request) {
    static char path[1024];
    
    // Initialize path buffer
    memset(path, 0, sizeof(path));
    
    // Check if this is a GET request
    if (strncmp(request, "GET ", 4) == 0) {
        // Find the end of the path (marked by space before HTTP version)
        char* path_end = strchr(request + 4, ' ');
        if (path_end) {
            // Calculate the path length
            int path_length = path_end - (request + 4);
            // Extract the path
            strncpy(path, request + 4, path_length);
            path[path_length] = '\0';
        }
    }
    
    return path;
}

int main() {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    printf("Logs from your program will appear here!\n");
    
    int server_fd, client_fd, client_addr_len;
    struct sockaddr_in client_addr;
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        printf("Socket creation failed: %s...\n", strerror(errno));
        return 1;
    }
    
    // Since the tester restarts your program quite often, setting SO_REUSEADDR
    // ensures that we don't run into 'Address already in use' errors
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        printf("SO_REUSEADDR failed: %s \n", strerror(errno));
        return 1;
    }
    
    struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
                                     .sin_port = htons(4221),
                                     .sin_addr = { htonl(INADDR_ANY) },
                                    };
    
    if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
        printf("Bind failed: %s \n", strerror(errno));
        return 1;
    }
    
    int connection_backlog = 5;
    if (listen(server_fd, connection_backlog) != 0) {
        printf("Listen failed: %s \n", strerror(errno));
        return 1;
    }
    
    printf("Waiting for a client to connect...\n");
    
    while (1) {
        client_addr_len = sizeof(client_addr);
        
        client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s \n", strerror(errno));
            continue;
        }
        
        printf("Client connected\n");
        
        // Buffer to store the received HTTP request
        char buffer[1024] = {0};
        
        // Read the HTTP request
        read(client_fd, buffer, sizeof(buffer) - 1);
        printf("Received request:\n%s\n", buffer);
        
        // Extract the path from the request
        char* path = extract_path(buffer);
        printf("Extracted path: %s\n", path);
        
        // Determine the appropriate response based on the path
        const char *response;
        if (strcmp(path, "/") == 0) {
            response = "HTTP/1.1 200 OK\r\n\r\n";
            printf("Sending 200 OK response\n");
        } else {
            response = "HTTP/1.1 404 Not Found\r\n\r\n";
            printf("Sending 404 Not Found response\n");
        }
        
        // Send the HTTP response
        send(client_fd, response, strlen(response), 0);
        
        // Close the client socket
        close(client_fd);
    }
    
    // Close the server socket
    close(server_fd);

    return 0;
}