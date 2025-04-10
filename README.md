# HTTP Server in C

A lightweight HTTP server implementation in C that supports basic HTTP/1.1 functionality with gzip compression.

## Features

- HTTP/1.1 protocol support
- Path-based routing (`/`, `/echo`, `/user-agent`, `/files`)
- File serving functionality with configurable root directory
- POST request handling for file uploads
- Gzip compression support with fallback
- Process-per-connection model for concurrent requests
- Proper error handling and logging

## Building

```sh
gcc -o http_server main.c
```

## Usage

Start the server:

```sh
./http_server [--directory <path>]
```

Options:
- `--directory <path>`: Set the root directory for serving files (optional)

## Endpoints

- `GET /`: Returns 200 OK
- `GET /echo/<string>`: Echoes back the <string> in response body
- `GET /user-agent`: Returns the client's User-Agent header value
- `GET /files/<filename>`: Serves files from configured directory
- `POST /files/<filename>`: Creates/updates files in configured directory

## Technical Details

- Uses `AF_INET` sockets with TCP (`SOCK_STREAM`)
- Listens on port 4221 on all interfaces
- Handles concurrent requests via process forking
- Implements proper zombie process reaping
- Supports gzip compression for responses
- Uses efficient buffer management for file operations

## Response Headers

- `Content-Type`: Appropriate MIME type for response
- `Content-Length`: Size of response body
- `Content-Encoding: gzip`: When compression is supported and used

## Error Handling

- 404 Not Found: For non-existent resources
- 400 Bad Request: For malformed requests
- 500 Internal Server Error: For server-side failures
- Proper error logging for debugging

## Security

- Basic file path security checks
- Proper file permissions (0644) for uploaded files
- Resource cleanup on process termination