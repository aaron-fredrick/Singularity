# Configuration
CC = gcc
CFLAGS = -Wall -O2 -Iinclude $(shell sdl2-config --cflags)
LDFLAGS = -lm $(shell sdl2-config --libs)

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
EXE_NAME = singularity
EXE = $(BIN_DIR)/$(EXE_NAME)

# Get source files and corresponding object files
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Default target
all: $(EXE)

# Create bin and build directories if missing
$(EXE): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $@ $^ $(LDFLAGS)

# Compile source to object
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean
