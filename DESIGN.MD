# HTTP Server Design Document

## Architecture Overview

### Core Components

1. **Socket Management**
   - Server socket initialization with `AF_INET` and `SOCK_STREAM`
   - `SO_REUSEADDR` configuration for rapid server restarts
   - Port 4221 binding with `INADDR_ANY`

2. **Process Management**
   - Fork-based concurrency model
   - One child process per connection
   - SIGCHLD handler for zombie process cleanup
   - Proper file descriptor management between parent/child

3. **Request Handling**
   - HTTP request parsing
   - Path extraction and routing
   - Header parsing (case-insensitive)
   - Request body extraction for POST

### Request Flow

```
Client Request → Server Socket → Fork Process → Parse Request → 
Route Handler → Generate Response → Compression (if supported) → Send Response
```

## Detailed Component Design

### Request Parsing

1. **URL Parsing**
   - `extract_path()`: Extracts path from GET/POST requests
   - `path_starts_with()`: URL prefix validation
   - `extract_echo_string()`: Content parsing for echo endpoint
   - `extract_filename()`: Filename extraction for file operations

2. **Header Processing**
   - `extract_header_value()`: Case-insensitive header extraction
   - `client_supports_gzip()`: Compression support detection
   - Content-Length parsing for POST requests

### Response Generation

1. **Basic Responses**
   - 200 OK for root path
   - 404 Not Found for invalid paths
   - 400 Bad Request for malformed requests
   - 500 Internal Server Error for server failures

2. **Content Responses**
   - Echo endpoint: Returns path content
   - User-Agent: Returns browser information
   - Files: Serves static files

### Compression System

1. **Gzip Implementation**
   - CRC32 table generation and calculation
   - Gzip header construction (magic numbers, flags)
   - Uncompressed block storage (STORED method)
   - Footer with CRC32 and size information

2. **Compression Flow**
```
Check Support → Generate Content → Compress if Supported → 
Add Headers → Send Response
```

### File Operations

1. **GET Handling**
   - File existence check
   - Content streaming
   - Compression for supported clients
   - Proper error handling

2. **POST Handling**
   - Request body extraction
   - File creation/update
   - Permission management (0644)
   - Error handling and validation

## Data Structures

1. **Request Buffer**
   ```c
   char buffer[4096]  // HTTP request buffer
   ```

2. **Gzip Format**
   ```c
   struct {
       uint8_t header[10];     // Gzip header
       uint8_t data[];         // Compressed data
       uint32_t crc32;        // CRC32 checksum
       uint32_t input_size;   // Original size
   }
   ```

## Error Handling

1. **System Errors**
   - Socket operations
   - Process management
   - File operations
   - Memory allocation

2. **HTTP Errors**
   - Invalid requests
   - Missing resources
   - Server failures
   - Compression failures

## Performance Considerations

1. **Process Management**
   - Efficient child process creation
   - Proper resource cleanup
   - Socket descriptor management

2. **Memory Usage**
   - Buffer size optimization
   - Compression memory management
   - File streaming for large files

3. **Compression**
   - Selective compression based on content size
   - Fallback mechanism for failures
   - Buffer management for compressed data

## Security Considerations

1. **File Operations**
   - Path validation
   - Permission management
   - Resource limits

2. **Process Isolation**
   - Proper file descriptor closure
   - Memory cleanup
   - Process separation

## Future Improvements

1. **Feature Additions**
   - HTTP/2 support
   - SSL/TLS implementation
   - Advanced compression algorithms
   - Content caching

2. **Performance Optimization**
   - Connection pooling
   - Memory optimization
   - Compression efficiency

3. **Security Enhancements**
   - Request rate limiting
   - Advanced authentication
   - Enhanced file security