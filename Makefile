CC = gcc
CROSS_CC = zig cc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lssl -lcrypto
TARGET = hid-gadget
MOCK_TARGET = hid-gadget-mock
WEBUI_TARGET = hid-webui
TEST_TARGET = tests/webui_test

# Directories
SRC_DIR = src
INC_DIR = include

# Track source files
SRC = $(SRC_DIR)/hid-gadget.c $(SRC_DIR)/tui.c $(SRC_DIR)/ducky.c
WEBUI_SRC = $(SRC_DIR)/webui.c
TEST_SRC = tests/webui_test.c

# Architectures to build
ARCHS = arm64 x86_64 arm x86

# Mapping names to Zig target triples
TARGET_arm64 = aarch64-linux-musl
TARGET_x86_64 = x86_64-linux-musl
TARGET_arm = arm-linux-musleabi
TARGET_x86 = x86-linux-musl

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

$(MOCK_TARGET): $(SRC)
	$(CC) $(CFLAGS) -DMOCK_HID -o $@ $(SRC) $(LDFLAGS)

# WebUI server
$(WEBUI_TARGET): $(WEBUI_SRC)
	$(CC) $(CFLAGS) -o $@ $(WEBUI_SRC) $(LDFLAGS)

# WebUI test suite
$(TEST_TARGET): $(TEST_SRC)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(LDFLAGS)

# Run integration tests
test-webui: $(WEBUI_TARGET) $(TEST_TARGET)
	@./tests/webui_integration.sh

# This rule handles directory creation and compilation in one go
static-%: $(SRC)
	@mkdir -p ./blobs/$*
	$(CROSS_CC) --target=$(TARGET_$(subst -,_,$*)) -static $(CFLAGS) -o hid-gadget-$*-static $(SRC) $(LDFLAGS)
	cp hid-gadget-$*-static ./blobs/$*/hid-gadget

static: $(addprefix static-, $(ARCHS))

mock-static: $(SRC)
	$(CC) $(CFLAGS) -static -DMOCK_HID -o hid-gadget-mock-static $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(MOCK_TARGET) $(WEBUI_TARGET) $(TEST_TARGET) *-static
	rm -rf ./blobs/*

.PHONY: all mock-static static clean test-webui
