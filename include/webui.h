#ifndef WEBUI_H
#define WEBUI_H

#include <stdbool.h>
#include <stdint.h>

// WebSocket server configuration
#define WEBUI_PORT 8080
#define WEBUI_MAX_CLIENTS 4
#define WEBUI_BUFFER_SIZE 4096

// WebSocket opcodes
#define WS_OPCODE_CONTINUATION 0x0
#define WS_OPCODE_TEXT 0x1
#define WS_OPCODE_BINARY 0x2
#define WS_OPCODE_CLOSE 0x8
#define WS_OPCODE_PING 0x9
#define WS_OPCODE_PONG 0xA

// WebSocket frame structure
typedef struct {
  uint8_t fin;
  uint8_t opcode;
  uint8_t mask;
  uint64_t payload_len;
  uint8_t masking_key[4];
  uint8_t *payload;
} ws_frame_t;

// Client connection structure
typedef struct {
  int fd;
  bool connected;
  bool handshake_done;
  char buffer[WEBUI_BUFFER_SIZE];
  size_t buffer_len;
} ws_client_t;

// WebSocket server functions
int webui_start_server(uint16_t port);
int webui_accept_client(int server_fd, ws_client_t *client);
int webui_handle_handshake(ws_client_t *client);
int webui_read_frame(ws_client_t *client, ws_frame_t *frame);
int webui_send_frame(ws_client_t *client, uint8_t opcode, const char *payload,
                     size_t len);
void webui_close_client(ws_client_t *client);
int webui_process_message(ws_client_t *client, const char *message);

// Utility functions
char *webui_generate_accept_key(const char *client_key);
void webui_unmask_payload(uint8_t *payload, size_t len, uint8_t *mask);

#endif // WEBUI_H
