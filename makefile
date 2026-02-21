# Configuration
CC = gcc
CFLAGS = -Wall -O3 -march=native -std=c11 -Iinclude -I/mingw64/include/SDL2 -D_REENTRANT
LDFLAGS = -lm -L/mingw64/lib -lmingw32 -lSDL2main -lSDL2

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
	@echo "Build complete: $(EXE)"

# Compile source to object
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Bundle SDL2.dll with executable
bundle-sdl2: all
	@echo "Looking for SDL2.dll..."
	@cp /mingw64/bin/SDL2.dll $(BIN_DIR)/ 2>/dev/null && echo "SDL2.dll copied to $(BIN_DIR)/" || echo "Warning: SDL2.dll not found in /mingw64/bin/"

# Create distribution package
dist: bundle-sdl2
	@echo "Distribution package ready in $(BIN_DIR)/"
	@ls -lh $(BIN_DIR)/

# Clean target
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean bundle-sdl2 dist
