# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lz

# Detect OS
ifeq ($(OS),Windows_NT)
    TARGET = http_server.exe
    RM = del /Q
    MKDIR = mkdir
else
    TARGET = http_server
    RM = rm -f
    MKDIR = mkdir -p
endif

# Directories
SRC_DIR = .
BUILD_DIR = build
BIN_DIR = bin

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Targets
.PHONY: all clean dirs

all: dirs $(BIN_DIR)/$(TARGET)

dirs:
    @if not exist $(BUILD_DIR) $(MKDIR) $(BUILD_DIR)
    @if not exist $(BIN_DIR) $(MKDIR) $(BIN_DIR)

$(BIN_DIR)/$(TARGET): $(OBJS)
    $(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
    $(CC) $(CFLAGS) -c $< -o $@

clean:
    $(RM) $(BUILD_DIR)\*.o $(BIN_DIR)\$(TARGET)

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: all

# Release build
release: CFLAGS += -O3 -DNDEBUG
release: all

# Run the server
run: all
    $(BIN_DIR)/$(TARGET)

# Run with directory flag
run-with-dir: all
    $(BIN_DIR)/$(TARGET) --directory files