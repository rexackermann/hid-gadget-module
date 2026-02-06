#include <arpa/inet.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define TEST_PORT 8080
#define TEST_HOST "127.0.0.1"
#define WS_MAGIC "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

// Test statistics
typedef struct {
  int total;
  int passed;
  int failed;
  double total_time_ms;
} test_stats_t;

test_stats_t stats = {0, 0, 0, 0.0};

// ANSI colors
#define COLOR_RESET "\x1b[0m"
#define COLOR_RED "\x1b[1;31m"
#define COLOR_GREEN "\x1b[1;32m"
#define COLOR_YELLOW "\x1b[1;33m"
#define COLOR_BLUE "\x1b[1;36m"
#define COLOR_MAGENTA "\x1b[1;35m"

// Base64 encoding table
static const char base64_table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Simple base64 encoding
static char *base64_encode(const unsigned char *input, int length) {
  int output_length = 4 * ((length + 2) / 3);
  char *output = malloc(output_length + 1);
  if (!output)
    return NULL;

  int i, j;
  for (i = 0, j = 0; i < length;) {
    uint32_t octet_a = i < length ? input[i++] : 0;
    uint32_t octet_b = i < length ? input[i++] : 0;
    uint32_t octet_c = i < length ? input[i++] : 0;
    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    output[j++] = base64_table[(triple >> 18) & 0x3F];
    output[j++] = base64_table[(triple >> 12) & 0x3F];
    output[j++] = base64_table[(triple >> 6) & 0x3F];
    output[j++] = base64_table[triple & 0x3F];
  }

  // Add padding
  for (int k = 0; k < (3 - length % 3) % 3; k++) {
    output[output_length - 1 - k] = '=';
  }

  output[output_length] = '\0';
  return output;
}

// WebSocket client
typedef struct {
  int fd;
  char buffer[4096];
} ws_client_t;

// Connect to WebSocket server
int ws_connect(ws_client_t *client) {
  struct sockaddr_in server_addr;

  client->fd = socket(AF_INET, SOCK_STREAM, 0);
  if (client->fd < 0) {
    return -1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(TEST_PORT);
  inet_pton(AF_INET, TEST_HOST, &server_addr.sin_addr);

  if (connect(client->fd, (struct sockaddr *)&server_addr,
              sizeof(server_addr)) < 0) {
    close(client->fd);
    return -1;
  }

  return 0;
}

// Perform WebSocket handshake
int ws_handshake(ws_client_t *client) {
  const char *key = "dGhlIHNhbXBsZSBub25jZQ==";

  snprintf(client->buffer, sizeof(client->buffer),
           "GET / HTTP/1.1\r\n"
           "Host: %s:%d\r\n"
           "Upgrade: websocket\r\n"
           "Connection: Upgrade\r\n"
           "Sec-WebSocket-Key: %s\r\n"
           "Sec-WebSocket-Version: 13\r\n\r\n",
           TEST_HOST, TEST_PORT, key);

  if (send(client->fd, client->buffer, strlen(client->buffer), 0) < 0) {
    return -1;
  }

  ssize_t bytes =
      recv(client->fd, client->buffer, sizeof(client->buffer) - 1, 0);
  if (bytes <= 0) {
    return -1;
  }

  client->buffer[bytes] = '\0';

  if (strstr(client->buffer, "101 Switching Protocols") == NULL) {
    return -1;
  }

  return 0;
}

// Send WebSocket frame
int ws_send(ws_client_t *client, const char *payload) {
  size_t len = strlen(payload);
  uint8_t header[10];
  size_t header_len = 0;

  // FIN + TEXT opcode
  header[0] = 0x81;
  header_len++;

  // Mask bit + payload length
  if (len < 126) {
    header[1] = 0x80 | len;
    header_len++;
  } else if (len < 65536) {
    header[1] = 0x80 | 126;
    header[2] = (len >> 8) & 0xFF;
    header[3] = len & 0xFF;
    header_len += 3;
  }

  // Masking key (simple for testing)
  uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
  memcpy(header + header_len, mask, 4);
  header_len += 4;

  // Send header
  if (send(client->fd, header, header_len, 0) < 0) {
    return -1;
  }

  // Mask and send payload
  uint8_t *masked = malloc(len);
  for (size_t i = 0; i < len; i++) {
    masked[i] = payload[i] ^ mask[i % 4];
  }

  int result = send(client->fd, masked, len, 0);
  free(masked);

  return result < 0 ? -1 : 0;
}

// Receive WebSocket frame
int ws_recv(ws_client_t *client, char *buffer, size_t max_len, int timeout_ms) {
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(client->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  uint8_t header[2];
  ssize_t bytes = recv(client->fd, header, 2, 0);
  if (bytes <= 0)
    return -1;

  uint64_t payload_len = header[1] & 0x7F;

  if (payload_len == 126) {
    uint8_t ext[2];
    recv(client->fd, ext, 2, 0);
    payload_len = (ext[0] << 8) | ext[1];
  }

  if (payload_len > max_len)
    return -1;

  bytes = recv(client->fd, buffer, payload_len, 0);
  if (bytes <= 0)
    return -1;

  buffer[bytes] = '\0';
  return bytes;
}

// Close WebSocket
void ws_close(ws_client_t *client) { close(client->fd); }

// Test helper macros
#define TEST_START(name)                                                       \
  do {                                                                         \
    printf("%s[TEST]%s %s... ", COLOR_BLUE, COLOR_RESET, name);                \
    fflush(stdout);                                                            \
    stats.total++;                                                             \
    struct timespec start, end;                                                \
    clock_gettime(CLOCK_MONOTONIC, &start);

#define TEST_END()                                                             \
  clock_gettime(CLOCK_MONOTONIC, &end);                                        \
  double elapsed = (end.tv_sec - start.tv_sec) * 1000.0 +                      \
                   (end.tv_nsec - start.tv_nsec) / 1000000.0;                  \
  stats.total_time_ms += elapsed;                                              \
  printf("%s✓ PASS%s (%.2fms)\n", COLOR_GREEN, COLOR_RESET, elapsed);          \
  stats.passed++;                                                              \
  }                                                                            \
  while (0)

#define TEST_FAIL(msg)                                                         \
  do {                                                                         \
    printf("%s✗ FAIL%s: %s\n", COLOR_RED, COLOR_RESET, msg);                   \
    stats.failed++;                                                            \
    return;                                                                    \
  } while (0)

#define ASSERT(cond, msg)                                                      \
  if (!(cond))                                                                 \
  TEST_FAIL(msg)

// ============================================================================
// TEST CASES
// ============================================================================

void test_connection() {
  TEST_START("WebSocket Connection");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Failed to connect to server");
  ws_close(&client);

  TEST_END();
}

void test_handshake() {
  TEST_START("WebSocket Handshake");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Connection failed");
  ASSERT(ws_handshake(&client) == 0, "Handshake failed");
  ws_close(&client);

  TEST_END();
}

void test_keyboard_single_key() {
  TEST_START("Keyboard: Single Key Press");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Connection failed");
  ASSERT(ws_handshake(&client) == 0, "Handshake failed");

  const char *msg =
      "{\"type\":\"keyboard\",\"action\":\"press\",\"key\":\"A\"}";
  ASSERT(ws_send(&client, msg) == 0, "Send failed");

  char response[1024];
  ASSERT(ws_recv(&client, response, sizeof(response), 1000) > 0, "No response");

  ws_close(&client);
  TEST_END();
}

void test_keyboard_modifiers() {
  TEST_START("Keyboard: Modifier Combinations");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Connection failed");
  ASSERT(ws_handshake(&client) == 0, "Handshake failed");

  const char *tests[] = {"{\"type\":\"keyboard\",\"action\":\"press\",\"key\":"
                         "\"C\",\"modifiers\":[\"CTRL\"]}",
                         "{\"type\":\"keyboard\",\"action\":\"press\",\"key\":"
                         "\"V\",\"modifiers\":[\"CTRL\"]}",
                         "{\"type\":\"keyboard\",\"action\":\"press\",\"key\":"
                         "\"A\",\"modifiers\":[\"CTRL\",\"SHIFT\"]}",
                         "{\"type\":\"keyboard\",\"action\":\"press\",\"key\":"
                         "\"ESC\",\"modifiers\":[\"ALT\"]}"};

  for (int i = 0; i < 4; i++) {
    ASSERT(ws_send(&client, tests[i]) == 0, "Send failed");
    usleep(10000); // 10ms delay
  }

  ws_close(&client);
  TEST_END();
}

void test_mouse_movement() {
  TEST_START("Mouse: Movement Commands");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Connection failed");
  ASSERT(ws_handshake(&client) == 0, "Handshake failed");

  const char *tests[] = {
      "{\"type\":\"mouse\",\"action\":\"move\",\"x\":10,\"y\":0}",
      "{\"type\":\"mouse\",\"action\":\"move\",\"x\":-10,\"y\":0}",
      "{\"type\":\"mouse\",\"action\":\"move\",\"x\":0,\"y\":10}",
      "{\"type\":\"mouse\",\"action\":\"move\",\"x\":0,\"y\":-10}",
      "{\"type\":\"mouse\",\"action\":\"move\",\"x\":10,\"y\":10}"};

  for (int i = 0; i < 5; i++) {
    ASSERT(ws_send(&client, tests[i]) == 0, "Send failed");
    usleep(10000);
  }

  ws_close(&client);
  TEST_END();
}

void test_mouse_clicks() {
  TEST_START("Mouse: Click Commands");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Connection failed");
  ASSERT(ws_handshake(&client) == 0, "Handshake failed");

  const char *tests[] = {
      "{\"type\":\"mouse\",\"action\":\"click\",\"button\":\"left\"}",
      "{\"type\":\"mouse\",\"action\":\"click\",\"button\":\"right\"}",
      "{\"type\":\"mouse\",\"action\":\"click\",\"button\":\"middle\"}"};

  for (int i = 0; i < 3; i++) {
    ASSERT(ws_send(&client, tests[i]) == 0, "Send failed");
    usleep(50000);
  }

  ws_close(&client);
  TEST_END();
}

void test_consumer_controls() {
  TEST_START("Consumer: Media Controls");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Connection failed");
  ASSERT(ws_handshake(&client) == 0, "Handshake failed");

  const char *tests[] = {"{\"type\":\"consumer\",\"action\":\"PLAY\"}",
                         "{\"type\":\"consumer\",\"action\":\"PAUSE\"}",
                         "{\"type\":\"consumer\",\"action\":\"NEXT\"}",
                         "{\"type\":\"consumer\",\"action\":\"PREV\"}",
                         "{\"type\":\"consumer\",\"action\":\"VOL+\"}",
                         "{\"type\":\"consumer\",\"action\":\"VOL-\"}",
                         "{\"type\":\"consumer\",\"action\":\"MUTE\"}"};

  for (int i = 0; i < 7; i++) {
    ASSERT(ws_send(&client, tests[i]) == 0, "Send failed");
    usleep(50000);
  }

  ws_close(&client);
  TEST_END();
}

void test_rapid_commands() {
  TEST_START("Stress: Rapid Command Burst (1000 commands)");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Connection failed");
  ASSERT(ws_handshake(&client) == 0, "Handshake failed");

  for (int i = 0; i < 1000; i++) {
    const char *msg =
        "{\"type\":\"mouse\",\"action\":\"move\",\"x\":1,\"y\":0}";
    ASSERT(ws_send(&client, msg) == 0, "Send failed");
  }

  ws_close(&client);
  TEST_END();
}

void test_invalid_json() {
  TEST_START("Edge Case: Invalid JSON");

  ws_client_t client;
  ASSERT(ws_connect(&client) == 0, "Connection failed");
  ASSERT(ws_handshake(&client) == 0, "Handshake failed");

  const char *msg = "{invalid json}";
  ws_send(&client, msg); // Should not crash server

  usleep(100000);
  ws_close(&client);
  TEST_END();
}

void test_multiple_clients() {
  TEST_START("Concurrent: Multiple Clients");

  ws_client_t clients[3];

  for (int i = 0; i < 3; i++) {
    ASSERT(ws_connect(&clients[i]) == 0, "Client connection failed");
    ASSERT(ws_handshake(&clients[i]) == 0, "Client handshake failed");
  }

  // All clients send commands
  for (int i = 0; i < 3; i++) {
    const char *msg =
        "{\"type\":\"keyboard\",\"action\":\"press\",\"key\":\"A\"}";
    ws_send(&clients[i], msg);
  }

  usleep(100000);

  for (int i = 0; i < 3; i++) {
    ws_close(&clients[i]);
  }

  TEST_END();
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main(int argc, char *argv[]) {
  printf("\n");
  printf("%s╔════════════════════════════════════════════════════════╗%s\n",
         COLOR_MAGENTA, COLOR_RESET);
  printf("%s║     HID WebUI - Automated Test Suite v1.0             ║%s\n",
         COLOR_MAGENTA, COLOR_RESET);
  printf("%s╚════════════════════════════════════════════════════════╝%s\n",
         COLOR_MAGENTA, COLOR_RESET);
  printf("\n");

  printf("%s[INFO]%s Testing server at %s:%d\n\n", COLOR_YELLOW, COLOR_RESET,
         TEST_HOST, TEST_PORT);

  // Connection tests
  printf("%s▶ Connection Tests%s\n", COLOR_BLUE, COLOR_RESET);
  test_connection();
  test_handshake();
  printf("\n");

  // Keyboard tests
  printf("%s▶ Keyboard Tests%s\n", COLOR_BLUE, COLOR_RESET);
  test_keyboard_single_key();
  test_keyboard_modifiers();
  printf("\n");

  // Mouse tests
  printf("%s▶ Mouse Tests%s\n", COLOR_BLUE, COLOR_RESET);
  test_mouse_movement();
  test_mouse_clicks();
  printf("\n");

  // Consumer tests
  printf("%s▶ Consumer Tests%s\n", COLOR_BLUE, COLOR_RESET);
  test_consumer_controls();
  printf("\n");

  // Stress tests
  printf("%s▶ Stress Tests%s\n", COLOR_BLUE, COLOR_RESET);
  test_rapid_commands();
  printf("\n");

  // Edge cases
  printf("%s▶ Edge Case Tests%s\n", COLOR_BLUE, COLOR_RESET);
  test_invalid_json();
  test_multiple_clients();
  printf("\n");

  // Summary
  printf("%s╔════════════════════════════════════════════════════════╗%s\n",
         COLOR_MAGENTA, COLOR_RESET);
  printf("%s║                    TEST SUMMARY                        ║%s\n",
         COLOR_MAGENTA, COLOR_RESET);
  printf("%s╚════════════════════════════════════════════════════════╝%s\n",
         COLOR_MAGENTA, COLOR_RESET);
  printf("\n");
  printf("  Total Tests:    %s%d%s\n", COLOR_BLUE, stats.total, COLOR_RESET);
  printf("  Passed:         %s%d%s\n", COLOR_GREEN, stats.passed, COLOR_RESET);
  printf("  Failed:         %s%d%s\n",
         stats.failed > 0 ? COLOR_RED : COLOR_GREEN, stats.failed, COLOR_RESET);
  printf("  Success Rate:   %s%.1f%%%s\n",
         stats.failed == 0 ? COLOR_GREEN : COLOR_YELLOW,
         (stats.passed * 100.0) / stats.total, COLOR_RESET);
  printf("  Total Time:     %.2fms\n", stats.total_time_ms);
  printf("  Avg Time/Test:  %.2fms\n", stats.total_time_ms / stats.total);
  printf("\n");

  if (stats.failed == 0) {
    printf("%s🎉 ALL TESTS PASSED! 🎉%s\n\n", COLOR_GREEN, COLOR_RESET);
    return 0;
  } else {
    printf("%s⚠️  SOME TESTS FAILED%s\n\n", COLOR_RED, COLOR_RESET);
    return 1;
  }
}
