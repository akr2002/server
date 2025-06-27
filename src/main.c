#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* --- Constants --- */
const int BUFFER_SIZE = 1024;

/* --- HTTP Status Strings --- */
const char *HTTP_200_OK = "200 OK";
const char *HTTP_400_BAD_REQUEST = "400 Bad Request";
const char *HTTP_404_NOT_FOUND = "404 Not Found";
const char *HTTP_500_INTERNAL_SERVER_ERROR = "500 Internal Server Error";
const char *HTTP_501_NOT_IMPLEMENTED = "501 Not Implemented";

/* --- Request Struct --- */
struct request {
  char method[16]; /* HTTP method */
  char path[256];  /* Requested file path */
  char http_version[16];
  char raw_request_copy[BUFFER_SIZE];
};

/* --- ServerConfig Struct --- */
typedef struct {
  int port;
  char server_root[PATH_MAX];
  char default_file[256];
  int max_backlog; /* Max connections */
} ServerConfig;

ServerConfig g_server_config;

/**
 * @brief Converts string to upper case.
 *
 * @param str Pointer to a string.
 * return Pointer to string.
 */
char *to_upper(char *str) {
  for (char *p = str; *p; ++p) {
    *p = (char)toupper((unsigned char)*p);
  }

  return str;
}

/**
 * @brief Converts string to lower case.
 *
 * @param str Pointer to a string.
 * return Pointer to string.
 */
char *to_lower(char *str) {
  for (char *p = str; *p; ++p) {
    *p = (char)tolower((unsigned char)*p);
  }

  return str;
}

/**
 * @brief Returns file size.
 *
 * @param Path to file.
 * @return size of file on success, -1 otherwise.
 */
off_t fsize(const char *filename) {
  struct stat st;

  if (stat(filename, &st) == 0)
    return st.st_size;

  fprintf(stderr, "[ERROR] %s: Cannot determine file size.\n", filename);
  perror("[ERROR] stat");
  return -1;
}

/**
 * @brief Trims leading/trailing whitespace.
 *
 * @param str Location of configuration file.
 * @return Pointer to trimmed string.
 */
char *trim_whitespace(char *str) {
  char *end = NULL;

  /* Trim leading space */
  while (isspace((unsigned char)*str))
    ++str;

  if (*str == 0) /* We are returning all spaces */
    return str;

  /* Trim trailing space */
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    --end;

  *(end + 1) = 0;

  return str;
}

int load_server_config(const char *config_filename, ServerConfig *config) {
  FILE *fp = fopen(config_filename, "r");
  if (fp == NULL) {
    perror("[ERROR] Failed to open configuration file");
    return -1;
  }

  char line[BUFFER_SIZE * 2];
  /* Flag to indicate if we are in [Server] section. */
  bool in_server_section = false;

  /* Set default values in case some settings are missing in the config file. */
  config->port = 8080;
  strncpy(config->server_root, "/var/www/html",
          sizeof(config->server_root) - 1);
  config->server_root[sizeof(config->server_root) - 1] = '\0';
  strncpy(config->default_file, "index.html", sizeof(config->default_file) - 1);
  config->default_file[sizeof(config->default_file) - 1] = '\0';
  config->max_backlog = 10;

  while (fgets(line, sizeof(line), fp) != NULL) {
    /* Trim newline character (if present) and other whitespace */
    line[strcspn(line, "\r\n")] = '\0';
    char *trimmed_line = trim_whitespace(line);

    /* Skip empty lines or comments */
    if (strlen(trimmed_line) == 0 || trimmed_line[0] == ';' ||
        trimmed_line[0] == '#') {
      continue;
    }

    /* Check for sections */
    if (trimmed_line[0] == '[' &&
        trimmed_line[sizeof(trimmed_line) - 1] == ']') {
      /* Extract section name (e.g., "[Server]" -> "Server") */
      char section_name[64];
      strncpy(section_name, trimmed_line + 1, sizeof(section_name) - 2);
      section_name[strlen(section_name) - 1] = '\0';
      section_name[sizeof(section_name) - 1] = '\0';

      if (strcmp(section_name, "Server") == 0) {
        in_server_section = true;
        printf("[INFO] Parsinf [Server] section.\n");
      } else {
        in_server_section = false;
      }
      continue;
    }

    /* Process key-value pairs ONLY if we are in the [Server] section. */
    if (in_server_section) {
      char *equal_sign = strchr(trimmed_line, '=');
      if (equal_sign != NULL) {
        *equal_sign = '\0'; /* Null-terminate the key part */

        char *key = trim_whitespace(trimmed_line);
        char *value = trim_whitespace(equal_sign + 1);

        if (strlen(key) == 0 || strlen(value) == 0) {
          fprintf(stderr, "[WARN] Skipping malformed key-value pair: '%s'\n",
                  trimmed_line);
          continue;
        }

        /* Match keys and assign values */
        if (strcmp(key, "Port") == 0) {
          config->port = atoi(value);
          if (config->port <= 0 || config->port > 65535) {
            fprintf(stderr,
                    "[WARN] Invalid port number '%s'. Using default %d.\n",
                    value, 8080);
            config->port = 8080;
          }
          printf("[INFO] Config: Port = %d\n", config->port);
        } else if (strcmp(key, "RootDirectory") == 0) {
          strncpy(config->server_root, value, sizeof(config->server_root) - 1);
          config->server_root[sizeof(config->server_root) - 1] = '\0';
          printf("[INFO] Config: RootDirectory = %s\n", config->server_root);
        } else if (strcmp(key, "DefaultFile") == 0) {
          strncpy(config->default_file, value,
                  sizeof(config->default_file) - 1);
          config->default_file[sizeof(config->default_file) - 1] = '\0';
          printf("[INFO] Config: DefaultFile = %s\n", config->default_file);
        } else if (strcmp(key, "MaxConnections") == 0) {
          config->max_backlog = atoi(value);
          if (config->max_backlog <= 0) {
            fprintf(stderr,
                    "[WARN] Invalid max connections '%s'. Using default %d.\n",
                    value, 10);
            config->max_backlog = 10;
          }
          printf("[INFO] Config: MaxConnections = %d\n", config->max_backlog);
        } else {
          fprintf(stderr, "[WARN] Unrecognized config key: '%s'\n", key);
        }
      }
    }
  }

  fclose(fp);
  return 0;
}

/**
 * @brief Sets up the server listening socket.
 *
 * @param port The port number to listen on.
 * @param max_backlog The maximum number of pending connections.
 * @return The server file descriptor, or -1 on error.
 */
int setup_server_socket(const int port, const int max_backlog) {
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("[ERROR] Cannot create socket");
    return -1;
  }

  /* Set SO_REUSEADDR to allow immediate reuse of the port */
  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("[ERROR] setsockopt SO_REUSEADDR failed");
    close(server_fd);
    return -1;
  }

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(port);
  server_address.sin_addr.s_addr =
      htonl(INADDR_ANY); // Listen on all available interfaces.

  if (bind(server_fd, (struct sockaddr *)&server_address,
           sizeof(server_address)) == -1) {
    perror("[ERROR] bind failed");
    close(server_fd);
    return -1;
  }

  printf("[INFO] Address bound successfully to port %d.\n", port);

  if (listen(server_fd, max_backlog) < 0) {
    perror("[ERROR] listen failed");
    close(server_fd);
    return -1;
  }

  printf("[INFO] Listening for connections...\n");

  return server_fd;
}

/**
 * @brief Parses the raw HTTP request intoa structured request object.
 *
 * @param buffer The raw request buffer received from the client.
 * @param req A pointer to the request struct to populate.
 * @return 0 on success, -1 on parsing error.
 */
int parse_http_request(char *buffer, struct request *req) {
  /* Copy the buffer because strtok modifies the string in place, and we want to
   * keep the original for optential debug/logging or future use. Also, strtok
   * is not thread-safe; for multi-threaded server, use strtok_r. For now, using
   * strtok on a copy is fine for single_threaded.
   */
  strncpy(req->raw_request_copy, buffer, BUFFER_SIZE - 1);
  req->raw_request_copy[BUFFER_SIZE - 1] = '\0';

  char *token = NULL;
  char *rest = req->raw_request_copy;

  /* Parse method */
  token = strtok_r(rest, "  ", &rest);
  if (!token || strlen(token) >= sizeof(req->method)) {
    fprintf(stderr, "[ERROR] Malformed or too long method.\n");
    return -1; /* Bad Request */
  }
  strncpy(req->method, token, sizeof(req->method) - 1);
  req->method[sizeof(req->method) - 1] = '\0';

  /* Parse path */
  token = strtok_r(rest, " ", &rest);
  if (!token || strlen(token) >= sizeof(req->path)) {
    fprintf(stderr, "[ERROR] Malformed or too long path.\n");
    return -1; /* Bad Request */
  }
  strncpy(req->path, token, sizeof(req->path) - 1);
  req->path[sizeof(req->path) - 1] = '\0';

  /* Parse HTTP version */
  token = strtok_r(rest, "\r\n", &rest);
  if (!token || strlen(token) >= sizeof(req->http_version)) {
    fprintf(stderr, "[ERROR] Malformed or to long HTTP version.\n");
    return -1; /* Bad Request */
  }
  strncpy(req->http_version, token, sizeof(req->http_version) - 1);
  req->http_version[sizeof(req->http_version) - 1] = '\0';

  printf("[DEBUG] Parsed Request: Method='%s', Path='%s', HTTP-Version='%s'\n",
         req->method, req->path, req->http_version);

  return 0; /* Success */
}

/**
 * @brief Determines the Content-Type (MIME type) based on the file extension.
 *
 * @param file_path The full path to the requested file.
 * @return A const char* string literal for the Content-Type or
 * "application/octet-stream" if unknwon.
 */
const char *get_mime_type(const char *file_path) {
  const char *ext = strrchr(file_path, '.');
  if (!ext) {
    return "application/octet-stream"; /* Default for unknown/no extension */
  }
  ext = to_lower((char *)ext);

  if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
    return "text/html";
  if (strcmp(ext, ".css") == 0)
    return "text/css";
  if (strcmp(ext, ".js") == 0)
    return "application/javascript";
  if (strcmp(ext, ".json") == 0)
    return "application/json";
  if (strcmp(ext, ".txt") == 0)
    return "text/plain";
  if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
    return "image/png";
  if (strcmp(ext, ".png") == 0)
    return "image/png";
  if (strcmp(ext, ".gif") == 0)
    return "image/gif";
  if (strcmp(ext, ".ico") == 0)
    return "image/x-icon";
  if (strcmp(ext, ".pdf") == 0)
    return "application/pdf";

  return "application/octet-stream";
}

/**
 * @brief Sends an HTTP response (headers and optional body) to the client.
 *
 * This function handles both successful file transfers and error responses.
 *
 * @param client_socket The socket connected to the client.
 * @param http_version The HTTP version (e.g., "HTTP/1.1").
 * @param status The HTTP status string (e.g., "200 OK", "404 not found").
 * @param content_type The MOME type of the content (e.g., "text/html").
 * @param content_length The size of the content body.
 * @param file_ptr A FILE pointer to the contnet to send (NULL for error pages).
 * @param error_message An optional message to include in the body for error
 * pages.
 */
void send_http_response(int client_socket, const char *http_version,
                        const char *status, const char *content_type,
                        long content_length, FILE *file_ptr,
                        const char *error_message) {
  char header_buffer[BUFFER_SIZE *
                     2]; /* Larger buffer for headers + error body */
  int header_len = 0;

  if (file_ptr != NULL && strcmp(status, HTTP_200_OK) == 0) {
    /* Case: Serving a file successfully */
    header_len = sprintf(header_buffer,
                         "%s %s\r\nContent-Type: %s\r\nContent-Length: "
                         "%ld\r\nConnection: close\r\n\r\n",
                         http_version, status, content_type, content_length);
    if (header_len < 0 || write(client_socket, header_buffer, header_len) < 0) {
      perror("[ERROR] Failed to send 200 OK headers");
      return;
    }

    char file_read_buffer[BUFFER_SIZE];
    ssize_t bytes_read_from_file = 0;
    while ((bytes_read_from_file =
                fread(file_read_buffer, 1, BUFFER_SIZE, file_ptr)) > 0) {
      if (write(client_socket, file_read_buffer, bytes_read_from_file) < 0) {
        perror("[ERROR] Failed to send file content");
        break; /* Stop sending if write fails */
      }
    }
    printf("[INFO] Sent %ld bytes of file content.\n", content_length);
  } else {
    /* Case: Sending an error response (e.g., 404, 501, 400)
     * Construct a simple HTML page
     */
    char default_error_body[512];
    if (error_message) {
      strncpy(default_error_body, error_message,
              sizeof(default_error_body) - 1);
      default_error_body[sizeof(default_error_body) - 1] = '\0';
    } else {
      /* Generic error message if specific one not privided */
      sprintf(default_error_body,
              "<html><body><h1>%s</h1><p>The requested resource could noot be "
              "found or processed.</p></body></html>",
              status);
    }

    long error_body_len = strlen(default_error_body);
    header_len =
        sprintf(header_buffer,
                "%s %s\r\nContent-Type: text/html\r\nContent-Length: "
                "%ld\r\nConnection: cosr\r\n\r\n%s",
                http_version, status, error_body_len, default_error_body);

    if (header_len < 0 || write(client_socket, header_buffer, header_len) < 0) {
      perror("[ERROR] Failed to send error response headers/body");
      return;
    }
    printf("[INFO] Sent error response: %s\n", status);
  }
}

/**
 * @brief Handles a single client connection.
 *
 * Reads the request, parses it, finds the requested file, and sends the
 * response.
 *
 * @param client_socket The socket connected to the client.
 */
void handle_client(int client_socket) {
  char buffer[BUFFER_SIZE] = {0};
  ssize_t read_status = read(client_socket, buffer, BUFFER_SIZE - 1);
  buffer[BUFFER_SIZE - 1] = '\0';

  if (read_status <= 0) {
    /* read_status can be 0 (client closed connection) or -1 (error) */
    if (read_status < 0) {
      perror("[ERROR] reading data from client");
    } else {
      printf("[INFO] Client disconnected without sending data.\n");
    }
    close(client_socket);
    return;
  }
  printf("[INFO] Received request:\n---\n%s---\n", buffer);

  struct request rq;
  const char *response_status =
      HTTP_500_INTERNAL_SERVER_ERROR; /* Default to server error */
  const char *content_type = "text/html";
  long content_length = 0;
  FILE *fptr = NULL;
  char full_path[PATH_MAX];

  if (parse_http_request(buffer, &rq) == -1) {
    response_status = HTTP_400_BAD_REQUEST; /* Parsing failed */
    send_http_response(
        client_socket, "HTTP/1.1", response_status, content_type,
        strlen("<h1>Bad Request</h1>"), NULL,
        "<h1>400 Bad Request</h1><p>Yor request could not be parsed.</p>");
    close(client_socket);
    return;
  }

  /* Check HTTP method */
  if (strcmp(to_upper(rq.method), "GET") != 0) {
    response_status = HTTP_501_NOT_IMPLEMENTED;
    send_http_response(
        client_socket, rq.http_version, response_status, content_type,
        strlen("<h1>Not Implemented</h1>"), NULL,
        "<h1>501 Not Implemented</h1><p>Only GET method is supported.</p>");
    close(client_socket);
    return;
  }

  /* Construct fill file path
   * Basic security: prevent directory traversal by cleaning path, though this
   * is a very simple chevk. Consider robust path sanitization in future
   * implementation.
   */
  if (strstr(rq.path, "..") != NULL) {
    response_status = HTTP_400_BAD_REQUEST;
    send_http_response(client_socket, rq.http_version, response_status,
                       content_type,
                       strlen("<h1>Bad Request - Invalid Path</h1>"), NULL,
                       "<h1>400 Bad Request</h1><p>Invalid path.</p>");
    close(client_socket);
    return;
  }

  /* Handle root path ("/") */
  if (strcmp(rq.path, "/") == 0) {
    snprintf(full_path, sizeof(full_path), "%s%s", g_server_config.server_root,
             g_server_config.default_file);
  } else {
    snprintf(full_path, sizeof(full_path), "%s%s", g_server_config.server_root,
             rq.path);
  }

  printf("[DEBUG] Attempting to open file: %s\n", full_path);
  fptr = fopen(full_path, "rb"); /* Open in binary read mode */

  if (fptr == NULL) {
    response_status = HTTP_404_NOT_FOUND;
    perror("[ERROR] fopen"); /* Log the specific file on error */
    send_http_response(client_socket, rq.http_version, response_status,
                       content_type, strlen("<h1>Not Found</h1>"), NULL,
                       "<h1>404 Not Found</h1><p>The requested resource was "
                       "not found on this server.</p>");
  } else {
    content_length = fsize(full_path);
    if (content_length == -1) {
      response_status = HTTP_500_INTERNAL_SERVER_ERROR;
      send_http_response(client_socket, rq.http_version, response_status,
                         content_type, strlen("<h1>Internal Server Error</h1>"),
                         NULL,
                         "<h1>500 Internal Server Error</h1><p>Could not "
                         "determimne file size.</p>");
      fclose(fptr); /* Close file if size determination failed */
    } else {
      /* All good, send the file */
      response_status = HTTP_200_OK;
      content_type = get_mime_type(full_path);
      printf("[DEBUG] Preparing to send %ld bytes of '%s' (Content-Type: %s). "
             "Sending %s.\n",
             content_length, full_path, content_type, response_status);
      send_http_response(client_socket, rq.http_version, response_status,
                         content_type, content_length, fptr, NULL);
      fclose(fptr);
    }
  }

  close(client_socket);
  printf("[INFO] Client socket closed.\n");
}

int main(int argc, char **argv) {
  const char *config_file = "/usr/share/server/config.ini";

  /* Allow config file to be specified as command-line argument */
  if (argc > 1) {
    config_file = argv[1];
  }

  /* Load configuration settings. */
  if (load_server_config(config_file, &g_server_config) != 0) {
    fprintf(stderr, "[FATAL] Failed to load server configuration. Exiting.\n");
    return EXIT_FAILURE;
  }
  printf("[INFO] Server root set to %s\n", g_server_config.server_root);

  int server_fd =
      setup_server_socket(g_server_config.port, g_server_config.max_backlog);
  if (server_fd == -1) {
    return EXIT_FAILURE;
  }

  struct sockaddr_in client_address;
  socklen_t length_of_address = sizeof(client_address);

  /* Main server loop: accept connections and handle them */
  while (1) {
    printf("\n[INFO] Waiting for a new connection...\n");
    int client_socket = accept(server_fd, (struct sockaddr *)&client_address,
                               &length_of_address);
    if (client_socket < 0) {
      perror("[ERROR] accept failed");
      continue;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_address.sin_addr), client_ip, INET_ADDRSTRLEN);
    printf("[INFO] Connection accepted from %s:%d\n", client_ip,
           ntohs(client_address.sin_port));

    handle_client(client_socket); /* Handle the client request */
  }

  close(server_fd);
  printf("[INFO] Server shutting down.\n");
  return EXIT_SUCCESS;
}
