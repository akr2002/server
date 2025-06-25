#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const int BUFFER_SIZE = 1024;
/* Size of backlog; refuses connection request when queue is full */
const int max_backlog = 3;
const int PORT = 8080;
const char *server_root = "/home/user/dev/server";
const char *default_file = "/index.html";

/* Status messages */
const char *HTTP_200_OK = "200 OK";
const char *HTTP_404_NOT_FOUND = "404 Not Found";
const char *HTTP_501_NOT_IMPLEMENTED = "501 Not Implemented";
const char *HTTP_400_BAD_REQUEST = "400 Bad Request";

/* Struct for parsed request */
struct request {
  char *method; /* HTTP method */
  char *path;   /* Requested file path */
  char *http_version;
  char raw_request[BUFFER_SIZE];
};

/* Return string in upper case */
/**
 * @brief Converts string to upper case.
 *
 * @param str A string
 * return String converted to upper case
 */
char *to_upper(char *const str) {
  for (char *p = str; *p; ++p) {
    *p = toupper(*p);
  }
  return str;
}

/* Return string in lower case */
char *to_lower(char *const s) {
  for (char *p = s; *p; ++p) {
    *p = tolower(*p);
  }
  return s;
}

/* Return file size if exists, -1 otherwise */
off_t fsize(const char *filename) {
  struct stat st;

  if (stat(filename, &st) == 0)
    return st.st_size;

  fprintf(stderr, "[ERROR] %s: Cannot determine file size.\n", filename);
  perror("[ERROR] stat");
  return -1;
}

/**
 * @brief Sets up the server listening socket.
 *
 * @param port The port number to listen on.
 * @param max_backlog The maximum number of pending connections.
 * @return The server file descriptor, or -1 on error.
 */
int setup_server_socket(const int port, const int max_backlog);

/**
 * @brief Parses the raw HTTP request intoa structured request object.
 *
 * @param buffer The raw request buffer received from the client.
 * @param req A pointer to the request struct to populate.
 * @return 0 on success, -1 on parsing error.
 */
int parse_http_request(char *buffer, struct request *req);

/**
 * @brief Determines the Content-Type (MIME type) based on the file extension.
 *
 * @param file_path The full path to the requested file.
 * @return A const char* string literal for the Content-Type or
 * "application/octect-stream" if unknwon.
 */
const char *get_mime_type(const char *file_path);

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
 * @return 0 on success, -1 on error during sending.
 */
int send_http_response(int client_socket, const char *http_version,
                       const char *status, const char *content_type,
                       long content_length, FILE *file_ptr,
                       const char *error_message);

/**
 * @brief Handles a single client connection.
 *
 * Reads the request, parses it, finds the requested file, and sends the
 * response.
 *
 * @param client_socket The socket connected to the client.
 */
void handle_client(int client_socket);

int main() {
  printf("[INFO] Server root set to %s\n", server_root);
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("[ERROR] Cannot create socket");
    return -1;
  }
  struct sockaddr_in server_address;
  struct sockaddr_in client_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(PORT);
  server_address.sin_addr.s_addr = htonl(INADDR_ANY);

  int bind_status = bind(server_fd, (struct sockaddr *)&server_address,
                         sizeof(server_address));
  if (bind_status == -1) {
    perror("[ERROR] bind");
    return -1;
  } else {
    printf("[INFO] Address bound successfully.\n");
  }

  int listen_status = listen(server_fd, max_backlog);
  if (listen_status < 0) {
    perror("[ERROR] listening connection");
    return -1;
  } else {
    printf("[INFO] Listening at %s:%d\n", INADDR_ANY, PORT);
  }

  int length_of_address = sizeof(client_address);
  /* Returns a new socket; original socket keeps listening */
  int client_socket =
      accept(server_fd, (struct sockaddr *)&client_address, &length_of_address);
  printf("[INFO] Connection accepted\n");

  char buffer[BUFFER_SIZE] = {0};
  ssize_t read_status = read(client_socket, buffer, BUFFER_SIZE);
  if (read_status < 0) {
    perror("[ERROR] reading data from client");
    return -1;
  } else {
    printf("[INFO] Successfully read data from client\n");
  }

  struct request rq;
  rq.method = strtok(buffer, " ");
  if (*(rq.method) == '\0') {
    fprintf(stderr, "[ERROR] Received NULL method\n");
    return -1;
  }

  int method_len = 0;
  while (rq.method[method_len++])
    ;
  --method_len;

  bool check_path = false;
  switch (method_len) {
  case 3:
    if (!strncmp(to_upper(rq.method), "GET", 3)) {
      check_path = true;
    }
    break;

  case 0:
  case BUFFER_SIZE:
    fprintf(stderr, "[ERROR] Malformed method\n");
    return -1;
    break;

  default:
    fprintf(stderr, "[ERROR] 501 Not implemented\n");
    return -1;
    break;
  }

  rq.path = (strtok(NULL, " "));

  int path_len = 0;
  while (rq.path[path_len++])
    ;
  --path_len;
  if (path_len == 0) {
    fprintf(stderr, "[ERROR] Malformed path\n");
    return -1;
  }

  rq.http_version = (strtok(NULL, "\r\n"));
  printf("[DEBUG] rq.http_version: %s\n", rq.http_version);

  char content_type[20] = "\0";
  const char connection[] = "open";

  FILE *fptr = NULL;
  char status[20] = {0};
  char full_path[100] = {0};
  printf("[DEBUG] rq.path: %s\n", rq.path);
  if (check_path) {
    strncat(full_path, server_root, strlen(server_root));
    if (!(strncmp(rq.path, "/\0", 2))) {
      strncat(full_path, default_file, strlen(default_file));
    } else {
      strncat(full_path, rq.path, path_len);
    }
    if ((fptr = fopen(full_path, "rb")) == NULL ||
        (strrchr(full_path, '.') == NULL)) {
      strncpy(status, "404 not found", 20);
      fprintf(stderr, "[ERROR] %s\n", status);
    } else {
      strncpy(status, "200 OK", 20);
    }

    char *const ext = strrchr(full_path, '.');
    printf("[DEBUG] full_path: %s\n", full_path);
    printf("[DEBUG] ext: %s\n", ext);
    if (!(strncmp(to_lower(ext), ".html", strlen(ext)))) {
      strncpy(content_type, "text/html", 13);
    }
  }

  off_t content_length = fsize(full_path);

  char c_type[100] = {0};
  sprintf(c_type, "Content-Type: %s\r\n", content_type);
  char c_length[40] = {0};
  sprintf(c_length, "Content-Length: %ld\r\n", content_length);
  char conn[40] = {0};
  sprintf(conn, "Connection: %s\r\n", connection);
  char header[512] = {0};
  sprintf(header, "%s %s\r\n%s%s%s\r\n\r\n", rq.http_version, status, c_type,
          c_length, conn);
  write(client_socket, header, strlen(header));
  char *fbuf = (char *)malloc(content_length + 1);
  ssize_t bytes_read = 0;
  while ((bytes_read = fread(fbuf, 1, content_length, fptr)) > 0) {
    write(client_socket, fbuf, bytes_read);
  }
  fclose(fptr);
  free(fbuf);
  close(client_socket);
  return 0;
}
