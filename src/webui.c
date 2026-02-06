#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../include/hid_interface.h"
#include "../include/webui.h"

#define WS_MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Base64 encoding for WebSocket accept key
static char *base64_encode(const unsigned char *input, int length) {
  BIO *bmem, *b64;
  BUF_MEM *bptr;

  b64 = BIO_new(BIO_f_base64());
  bmem = BIO_new(BIO_s_mem());
  b64 = BIO_push(b64, bmem);
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
  BIO_write(b64, input, length);
  BIO_flush(b64);
  BIO_get_mem_ptr(b64, &bptr);

  char *buff = (char *)malloc(bptr->length + 1);
  memcpy(buff, bptr->data, bptr->length);
  buff[bptr->length] = 0;

  BIO_free_all(b64);
  return buff;
}

// Generate WebSocket accept key from client key
char *webui_generate_accept_key(const char *client_key) {
  char concat[256];
  snprintf(concat, sizeof(concat), "%s%s", client_key, WS_MAGIC_STRING);

  unsigned char hash[SHA_DIGEST_LENGTH];
  SHA1((unsigned char *)concat, strlen(concat), hash);

  return base64_encode(hash, SHA_DIGEST_LENGTH);
}

// Unmask WebSocket payload
void webui_unmask_payload(uint8_t *payload, size_t len, uint8_t *mask) {
  for (size_t i = 0; i < len; i++) {
    payload[i] ^= mask[i % 4];
  }
}

// Start WebSocket server
int webui_start_server(uint16_t port) {
  int server_fd;
  struct sockaddr_in address;
  int opt = 1;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    return -1;
  }

  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    close(server_fd);
    return -1;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    close(server_fd);
    return -1;
  }

  if (listen(server_fd, 3) < 0) {
    perror("listen");
    close(server_fd);
    return -1;
  }

  printf("\x1b[1;32m[WebUI] Server started on port %d\x1b[0m\n", port);
  return server_fd;
}

// Accept new client connection
int webui_accept_client(int server_fd, ws_client_t *client) {
  struct sockaddr_in address;
  socklen_t addrlen = sizeof(address);

  client->fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
  if (client->fd < 0) {
    return -1;
  }

  client->connected = true;
  client->handshake_done = false;
  client->buffer_len = 0;
  memset(client->buffer, 0, WEBUI_BUFFER_SIZE);

  printf("\x1b[1;36m[WebUI] Client connected from %s\x1b[0m\n",
         inet_ntoa(address.sin_addr));
  return 0;
}

// Handle WebSocket handshake
int webui_handle_handshake(ws_client_t *client) {
  char buffer[WEBUI_BUFFER_SIZE];
  ssize_t bytes_read = recv(client->fd, buffer, sizeof(buffer) - 1, 0);

  if (bytes_read <= 0) {
    return -1;
  }

  buffer[bytes_read] = '\0';

  // Extract Sec-WebSocket-Key
  char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
  if (!key_start) {
    return -1;
  }

  key_start += 19; // Length of "Sec-WebSocket-Key: "
  char *key_end = strstr(key_start, "\r\n");
  if (!key_end) {
    return -1;
  }

  char client_key[256];
  size_t key_len = key_end - key_start;
  strncpy(client_key, key_start, key_len);
  client_key[key_len] = '\0';

  // Generate accept key
  char *accept_key = webui_generate_accept_key(client_key);

  // Send handshake response
  char response[512];
  snprintf(response, sizeof(response),
           "HTTP/1.1 101 Switching Protocols\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Accept: %s\r\n\r\n",
           accept_key);

  send(client->fd, response, strlen(response), 0);
  free(accept_key);

  client->handshake_done = true;
  printf("\x1b[1;32m[WebUI] Handshake completed\x1b[0m\n");
  return 0;
}

// Read WebSocket frame
int webui_read_frame(ws_client_t *client, ws_frame_t *frame) {
  uint8_t header[14];
  ssize_t bytes_read = recv(client->fd, header, 2, 0);

  if (bytes_read <= 0) {
    return -1;
  }

  frame->fin = (header[0] >> 7) & 0x1;
  frame->opcode = header[0] & 0xF;
  frame->mask = (header[1] >> 7) & 0x1;
  frame->payload_len = header[1] & 0x7F;

  size_t header_offset = 2;

  // Extended payload length
  if (frame->payload_len == 126) {
    bytes_read = recv(client->fd, header + 2, 2, 0);
    if (bytes_read <= 0)
      return -1;
    frame->payload_len = (header[2] << 8) | header[3];
    header_offset += 2;
  } else if (frame->payload_len == 127) {
    bytes_read = recv(client->fd, header + 2, 8, 0);
    if (bytes_read <= 0)
      return -1;
    frame->payload_len = 0;
    for (int i = 0; i < 8; i++) {
      frame->payload_len = (frame->payload_len << 8) | header[2 + i];
    }
    header_offset += 8;
  }

  // Masking key
  if (frame->mask) {
    bytes_read = recv(client->fd, frame->masking_key, 4, 0);
    if (bytes_read <= 0)
      return -1;
  }

  // Payload
  if (frame->payload_len > 0) {
    frame->payload = malloc(frame->payload_len + 1);
    size_t total_read = 0;
    while (total_read < frame->payload_len) {
      bytes_read = recv(client->fd, frame->payload + total_read,
                        frame->payload_len - total_read, 0);
      if (bytes_read <= 0) {
        free(frame->payload);
        return -1;
      }
      total_read += bytes_read;
    }

    if (frame->mask) {
      webui_unmask_payload(frame->payload, frame->payload_len,
                           frame->masking_key);
    }

    frame->payload[frame->payload_len] = '\0';
  } else {
    frame->payload = NULL;
  }

  return 0;
}

// Send WebSocket frame
int webui_send_frame(ws_client_t *client, uint8_t opcode, const char *payload,
                     size_t len) {
  uint8_t header[10];
  size_t header_len = 0;

  // FIN + opcode
  header[0] = 0x80 | (opcode & 0xF);
  header_len++;

  // Payload length
  if (len < 126) {
    header[1] = len;
    header_len++;
  } else if (len < 65536) {
    header[1] = 126;
    header[2] = (len >> 8) & 0xFF;
    header[3] = len & 0xFF;
    header_len += 3;
  } else {
    header[1] = 127;
    for (int i = 7; i >= 0; i--) {
      header[2 + (7 - i)] = (len >> (i * 8)) & 0xFF;
    }
    header_len += 9;
  }

  // Send header
  if (send(client->fd, header, header_len, 0) < 0) {
    return -1;
  }

  // Send payload
  if (len > 0 && payload) {
    if (send(client->fd, payload, len, 0) < 0) {
      return -1;
    }
  }

  return 0;
}

// Process incoming JSON message
int webui_process_message(ws_client_t *client, const char *message) {
  // Simple JSON parsing (looking for "type" and "action" fields)
  // In production, use a proper JSON library

  printf("\x1b[1;33m[WebUI] Received: %s\x1b[0m\n", message);

  // Response message
  const char *response = "{\"status\":\"ok\"}";
  return webui_send_frame(client, WS_OPCODE_TEXT, response, strlen(response));
}

// Close client connection
void webui_close_client(ws_client_t *client) {
  if (client->connected) {
    webui_send_frame(client, WS_OPCODE_CLOSE, NULL, 0);
    close(client->fd);
    client->connected = false;
    printf("\x1b[1;31m[WebUI] Client disconnected\x1b[0m\n");
  }
}

// Main server loop
int main(int argc, char *argv[]) {
  int server_fd = webui_start_server(WEBUI_PORT);
  if (server_fd < 0) {
    return EXIT_FAILURE;
  }

  ws_client_t clients[WEBUI_MAX_CLIENTS];
  for (int i = 0; i < WEBUI_MAX_CLIENTS; i++) {
    clients[i].connected = false;
  }

  printf("\x1b[1;36m[WebUI] Waiting for connections...\x1b[0m\n");
  printf("\x1b[1;37m[WebUI] Open http://localhost:%d in your browser\x1b[0m\n",
         WEBUI_PORT);

  while (1) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(server_fd, &readfds);

    int max_fd = server_fd;
    for (int i = 0; i < WEBUI_MAX_CLIENTS; i++) {
      if (clients[i].connected) {
        FD_SET(clients[i].fd, &readfds);
        if (clients[i].fd > max_fd) {
          max_fd = clients[i].fd;
        }
      }
    }

    struct timeval timeout = {1, 0};
    int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

    if (activity < 0) {
      perror("select error");
      break;
    }

    // New connection
    if (FD_ISSET(server_fd, &readfds)) {
      for (int i = 0; i < WEBUI_MAX_CLIENTS; i++) {
        if (!clients[i].connected) {
          webui_accept_client(server_fd, &clients[i]);
          break;
        }
      }
    }

    // Handle client messages
    for (int i = 0; i < WEBUI_MAX_CLIENTS; i++) {
      if (clients[i].connected && FD_ISSET(clients[i].fd, &readfds)) {
        if (!clients[i].handshake_done) {
          if (webui_handle_handshake(&clients[i]) < 0) {
            webui_close_client(&clients[i]);
          }
        } else {
          ws_frame_t frame;
          if (webui_read_frame(&clients[i], &frame) < 0) {
            webui_close_client(&clients[i]);
            continue;
          }

          if (frame.opcode == WS_OPCODE_TEXT) {
            webui_process_message(&clients[i], (char *)frame.payload);
          } else if (frame.opcode == WS_OPCODE_CLOSE) {
            webui_close_client(&clients[i]);
          } else if (frame.opcode == WS_OPCODE_PING) {
            webui_send_frame(&clients[i], WS_OPCODE_PONG, (char *)frame.payload,
                             frame.payload_len);
          }

          if (frame.payload) {
            free(frame.payload);
          }
        }
      }
    }
  }

  close(server_fd);
  return EXIT_SUCCESS;
}
