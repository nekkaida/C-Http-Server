#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h> // Add zlib for gzip compression

// Global variable to store the directory path
char *files_directory = NULL;

// Function to compress data using gzip
// Returns the size of compressed data and updates the provided buffer
unsigned long gzip_compress(char* dest, unsigned long* dest_len, const char* source, unsigned long source_len) {
    z_stream strm;
    
    // Initialize the z_stream struct
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    // Initialize gzip compression with default compression level
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 
                    31, // 15 + 16 for gzip format
                    8, Z_DEFAULT_STRATEGY) != Z_OK) {
        fprintf(stderr, "Failed to initialize zlib for compression\n");
        return 0;
    }
    
    strm.avail_in = source_len;
    strm.next_in = (Bytef*)source;
    strm.avail_out = *dest_len;
    strm.next_out = (Bytef*)dest;
    
    // Compress the data
    deflate(&strm, Z_FINISH);
    
    // Calculate size of compressed data
    unsigned long compressed_size = *dest_len - strm.avail_out;
    
    // Clean up
    deflateEnd(&strm);
    
    // Update the output buffer size
    *dest_len = compressed_size;
    
    return compressed_size;
}

// Function to extract the path from an HTTP request
char* extract_path(char* request) {
    static char path[1024];
    
    // Initialize path buffer
    memset(path, 0, sizeof(path));
    
    // Check if this is a GET or POST request
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
    } else if (strncmp(request, "POST ", 5) == 0) {
        // Find the end of the path (marked by space before HTTP version)
        char* path_end = strchr(request + 5, ' ');
        if (path_end) {
            // Calculate the path length
            int path_length = path_end - (request + 5);
            // Extract the path
            strncpy(path, request + 5, path_length);
            path[path_length] = '\0';
        }
    }
    
    return path;
}

// Function to determine if the request is a POST request
int is_post_request(char* request) {
    return strncmp(request, "POST ", 5) == 0;
}

// Function to check if a path starts with a specific prefix
int path_starts_with(const char* path, const char* prefix) {
    return strncmp(path, prefix, strlen(prefix)) == 0;
}

// Function to extract the echo string from path
char* extract_echo_string(const char* path) {
    static char echo_str[1024];
    
    // Skip "/echo/" prefix
    const char* start = path + 6; // 6 is the length of "/echo/"
    
    // Copy the rest of the path
    strcpy(echo_str, start);
    
    return echo_str;
}

// Function to extract the filename from a /files/ path
char* extract_filename(const char* path) {
    static char filename[1024];
    
    // Skip "/files/" prefix
    const char* start = path + 7; // 7 is the length of "/files/"
    
    // Copy the rest of the path
    strcpy(filename, start);
    
    return filename;
}

// Function to extract a header value from an HTTP request
char* extract_header_value(const char* request, const char* header_name) {
    static char value[1024];
    memset(value, 0, sizeof(value));
    
    // Create the header string to search for (case-insensitive)
    char search_header[1024];
    sprintf(search_header, "\r\n%s: ", header_name);
    
    // Convert search header to lowercase for case-insensitive search
    for (int i = 0; search_header[i]; i++) {
        search_header[i] = tolower(search_header[i]);
    }
    
    // Create lowercase version of request for searching
    char* lower_request = strdup(request);
    for (int i = 0; lower_request[i]; i++) {
        lower_request[i] = tolower(lower_request[i]);
    }
    
    // Look for the header
    char* header_pos = strstr(lower_request, search_header);
    if (header_pos) {
        // Calculate the position in the original request
        int offset = header_pos - lower_request;
        
        // Get position after the header name and colon
        const char* value_start = request + offset + strlen(search_header);
        
        // Find the end of the value (marked by CRLF)
        const char* value_end = strstr(value_start, "\r\n");
        if (value_end) {
            // Calculate the value length
            int value_length = value_end - value_start;
            
            // Extract the value
            strncpy(value, value_start, value_length);
            value[value_length] = '\0';
        }
    }
    
    // Free the temporary lowercase request
    free(lower_request);
    
    return value;
}

// Function to check if client supports gzip encoding
int client_supports_gzip(const char* request) {
    char* accept_encoding = extract_header_value(request, "Accept-Encoding");
    
    // Check if gzip is in the Accept-Encoding header
    if (strstr(accept_encoding, "gzip") != NULL) {
        return 1;
    }
    
    return 0;
}

// Function to extract the request body from an HTTP request
char* extract_request_body(char* request, int* body_length) {
    char* body_start = strstr(request, "\r\n\r\n");
    if (body_start) {
        body_start += 4; // Skip the \r\n\r\n
        
        // Get the Content-Length header
        char* content_length_str = extract_header_value(request, "Content-Length");
        if (content_length_str[0] != '\0') {
            *body_length = atoi(content_length_str);
            return body_start;
        }
    }
    
    *body_length = 0;
    return NULL;
}

// Handler for SIGCHLD to reap child processes
void handle_sigchld(int sig) {
    // Reap all dead processes
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Function to handle a client connection
void handle_client(int client_fd) {
    // Buffer to store the received HTTP request
    char buffer[4096] = {0};
    
    // Read the HTTP request
    read(client_fd, buffer, sizeof(buffer) - 1);
    printf("Received request:\n%s\n", buffer);
    
    // Extract the path from the request
    char* path = extract_path(buffer);
    printf("Extracted path: %s\n", path);
    
    // Check if client supports gzip
    int supports_gzip = client_supports_gzip(buffer);
    printf("Client supports gzip: %s\n", supports_gzip ? "Yes" : "No");
    
    // Check if it's a POST request
    int is_post = is_post_request(buffer);
    
    // Determine the appropriate response based on the path
    if (strcmp(path, "/") == 0) {
        // Root path - return 200 OK
        const char *response = "HTTP/1.1 200 OK\r\n\r\n";
        send(client_fd, response, strlen(response), 0);
        printf("PID %d: Sent 200 OK response for root path\n", getpid());
    } else if (path_starts_with(path, "/echo/")) {
        // Echo endpoint
        char* echo_str = extract_echo_string(path);
        int echo_len = strlen(echo_str);
        
        if (supports_gzip) {
            // Prepare buffers for compression
            unsigned long compressed_size = 4096; // Initial size guess
            char* compressed_data = malloc(compressed_size);
            
            if (compressed_data == NULL) {
                // Failed to allocate memory
                const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                send(client_fd, response, strlen(response), 0);
                printf("PID %d: Failed to allocate memory for compression\n", getpid());
            } else {
                // Compress the echo string
                compressed_size = gzip_compress(compressed_data, &compressed_size, echo_str, echo_len);
                
                if (compressed_size > 0) {
                    // Create response with Content-Type, Content-Encoding, and correct Content-Length headers
                    char response_headers[1024];
                    sprintf(response_headers, 
                            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Encoding: gzip\r\nContent-Length: %lu\r\n\r\n", 
                            compressed_size);
                    
                    // Send headers
                    send(client_fd, response_headers, strlen(response_headers), 0);
                    
                    // Send compressed data
                    send(client_fd, compressed_data, compressed_size, 0);
                    
                    printf("PID %d: Sent gzip-compressed echo response with string: %s (original size: %d, compressed: %lu)\n", 
                           getpid(), echo_str, echo_len, compressed_size);
                } else {
                    // Compression failed, fallback to uncompressed
                    char response[2048];
                    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", 
                            echo_len, echo_str);
                    send(client_fd, response, strlen(response), 0);
                    printf("PID %d: Compression failed, sent uncompressed echo response: %s\n", getpid(), echo_str);
                }
                
                // Free the compressed data buffer
                free(compressed_data);
            }
        } else {
            // Standard response without compression
            char response[2048];
            sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", 
                    echo_len, echo_str);
            send(client_fd, response, strlen(response), 0);
            printf("PID %d: Sent uncompressed echo response: %s\n", getpid(), echo_str);
        }
    } else if (strcmp(path, "/user-agent") == 0) {
        // User-Agent endpoint
        char* user_agent = extract_header_value(buffer, "User-Agent");
        int user_agent_len = strlen(user_agent);
        
        if (supports_gzip) {
            // Prepare buffers for compression
            unsigned long compressed_size = 4096; // Initial size guess
            char* compressed_data = malloc(compressed_size);
            
            if (compressed_data == NULL) {
                // Failed to allocate memory
                const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                send(client_fd, response, strlen(response), 0);
                printf("PID %d: Failed to allocate memory for compression\n", getpid());
            } else {
                // Compress the user agent string
                compressed_size = gzip_compress(compressed_data, &compressed_size, user_agent, user_agent_len);
                
                if (compressed_size > 0) {
                    // Create response with Content-Type, Content-Encoding, and correct Content-Length headers
                    char response_headers[1024];
                    sprintf(response_headers, 
                            "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Encoding: gzip\r\nContent-Length: %lu\r\n\r\n", 
                            compressed_size);
                    
                    // Send headers
                    send(client_fd, response_headers, strlen(response_headers), 0);
                    
                    // Send compressed data
                    send(client_fd, compressed_data, compressed_size, 0);
                    
                    printf("PID %d: Sent gzip-compressed user-agent response: %s (original size: %d, compressed: %lu)\n", 
                           getpid(), user_agent, user_agent_len, compressed_size);
                } else {
                    // Compression failed, fallback to uncompressed
                    char response[2048];
                    sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", 
                            user_agent_len, user_agent);
                    send(client_fd, response, strlen(response), 0);
                    printf("PID %d: Compression failed, sent uncompressed user-agent response: %s\n", getpid(), user_agent);
                }
                
                // Free the compressed data buffer
                free(compressed_data);
            }
        } else {
            // Standard response without compression
            char response[2048];
            sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n%s", 
                    user_agent_len, user_agent);
            send(client_fd, response, strlen(response), 0);
            printf("PID %d: Sent user-agent response: %s\n", getpid(), user_agent);
        }
    } else if (path_starts_with(path, "/files/") && files_directory != NULL) {
        // Files endpoint
        char* filename = extract_filename(path);
        
        // Create the full file path
        char filepath[2048];
        sprintf(filepath, "%s/%s", files_directory, filename);
        
        if (is_post) {
            // POST request - create a new file
            int body_length = 0;
            char* body = extract_request_body(buffer, &body_length);
            
            if (body && body_length > 0) {
                // Open file for writing
                int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    // Failed to create file
                    const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                    send(client_fd, response, strlen(response), 0);
                    printf("PID %d: Failed to create file: %s\n", getpid(), filename);
                } else {
                    // Write the request body to the file
                    write(fd, body, body_length);
                    close(fd);
                    
                    // Return 201 Created
                    const char *response = "HTTP/1.1 201 Created\r\n\r\n";
                    send(client_fd, response, strlen(response), 0);
                    printf("PID %d: Created file: %s (size: %d bytes)\n", getpid(), filename, body_length);
                }
            } else {
                // Bad request - missing or empty body
                const char *response = "HTTP/1.1 400 Bad Request\r\n\r\n";
                send(client_fd, response, strlen(response), 0);
                printf("PID %d: Bad request - missing or empty body\n", getpid());
            }
        } else {
            // GET request - read file
            int fd = open(filepath, O_RDONLY);
            if (fd == -1) {
                // File not found - return 404
                const char *response = "HTTP/1.1 404 Not Found\r\n\r\n";
                send(client_fd, response, strlen(response), 0);
                printf("PID %d: Sent 404 Not Found response for file: %s\n", getpid(), filename);
            } else {
                // Get file size
                struct stat file_stat;
                fstat(fd, &file_stat);
                off_t file_size = file_stat.st_size;
                
                if (supports_gzip && file_size > 0) {
                    // Read the file content into memory
                    char* file_content = malloc(file_size);
                    if (file_content == NULL) {
                        // Failed to allocate memory
                        const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                        send(client_fd, response, strlen(response), 0);
                        printf("PID %d: Failed to allocate memory for file: %s\n", getpid(), filename);
                        close(fd);
                        return;
                    }
                    
                    // Read file content
                    ssize_t bytes_read = read(fd, file_content, file_size);
                    close(fd);
                    
                    if (bytes_read != file_size) {
                        // Failed to read the entire file
                        const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                        send(client_fd, response, strlen(response), 0);
                        printf("PID %d: Failed to read entire file: %s\n", getpid(), filename);
                        free(file_content);
                        return;
                    }
                    
                    // Prepare buffers for compression
                    unsigned long compressed_size = file_size * 2; // Initial size guess (larger than file)
                    char* compressed_data = malloc(compressed_size);
                    
                    if (compressed_data == NULL) {
                        // Failed to allocate memory for compression
                        const char *response = "HTTP/1.1 500 Internal Server Error\r\n\r\n";
                        send(client_fd, response, strlen(response), 0);
                        printf("PID %d: Failed to allocate memory for compression\n", getpid());
                        free(file_content);
                        return;
                    }
                    
                    // Compress the file content
                    compressed_size = gzip_compress(compressed_data, &compressed_size, file_content, file_size);
                    
                    if (compressed_size > 0) {
                        // Create response with Content-Type, Content-Encoding, and correct Content-Length headers
                        char response_headers[1024];
                        sprintf(response_headers, 
                                "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Encoding: gzip\r\nContent-Length: %lu\r\n\r\n", 
                                compressed_size);
                        
                        // Send headers
                        send(client_fd, response_headers, strlen(response_headers), 0);
                        
                        // Send compressed data
                        send(client_fd, compressed_data, compressed_size, 0);
                        
                        printf("PID %d: Sent gzip-compressed file: %s (original size: %ld, compressed: %lu)\n", 
                               getpid(), filename, file_size, compressed_size);
                    } else {
                        // Compression failed, fallback to uncompressed
                        char response_headers[1024];
                        sprintf(response_headers, 
                                "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %ld\r\n\r\n", 
                                file_size);
                        
                        // Send headers
                        send(client_fd, response_headers, strlen(response_headers), 0);
                        
                        // Send file content
                        send(client_fd, file_content, file_size, 0);
                        
                        printf("PID %d: Compression failed, sent uncompressed file: %s (size: %ld bytes)\n", 
                               getpid(), filename, file_size);
                    }
                    
                    // Free memory
                    free(file_content);
                    free(compressed_data);
                } else {
                    // Standard response without compression
                    char headers[1024];
                    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: %ld\r\n\r\n", 
                            file_size);
                    
                    // Send headers
                    send(client_fd, headers, strlen(headers), 0);
                    
                    // Send file content
                    char file_buffer[4096];
                    ssize_t bytes_read;
                    
                    while ((bytes_read = read(fd, file_buffer, sizeof(file_buffer))) > 0) {
                        send(client_fd, file_buffer, bytes_read, 0);
                    }
                    
                    // Close file
                    close(fd);
                    
                    printf("PID %d: Sent file: %s (size: %ld bytes)\n", getpid(), filename, file_size);
                }
            }
        }
    } else {
        // Any other path - return 404 Not Found
        const char *response = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client_fd, response, strlen(response), 0);
        printf("PID %d: Sent 404 Not Found response\n", getpid());
    }
    
    // Close the client socket
    close(client_fd);
    exit(0);  // Child process exits after handling the request
}

int main(int argc, char *argv[]) {
    // Disable output buffering
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    // You can use print statements as follows for debugging, they'll be visible when running tests.
    printf("Logs from your program will appear here!\n");
    
    // Parse command line arguments for --directory flag
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--directory") == 0 && i + 1 < argc) {
            files_directory = argv[i + 1];
            printf("Directory for files set to: %s\n", files_directory);
            break;
        }
    }
    
    // Set up signal handler for SIGCHLD to reap zombie processes
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
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
    
    printf("Server started. Waiting for connections...\n");
    
    while (1) {
        client_addr_len = sizeof(client_addr);
        
        client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("Accept failed: %s \n", strerror(errno));
            continue;
        }
        
        printf("Client connected - spawning child process\n");
        
        // Fork a child process to handle the client
        pid_t pid = fork();
        
        if (pid < 0) {
            // Fork failed
            printf("Fork failed: %s\n", strerror(errno));
            close(client_fd);
            continue;
        } else if (pid == 0) {
            // Child process
            close(server_fd);  // Child doesn't need the server socket
            handle_client(client_fd);
            // Child process exits in handle_client function
        } else {
            // Parent process
            close(client_fd);  // Parent doesn't need the client socket
            printf("Created child process with PID: %d\n", pid);
            // Parent continues to accept new connections
        }
    }
    
    // Close the server socket
    close(server_fd);

    return 0;
}