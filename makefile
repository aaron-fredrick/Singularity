# Configuration
CC = gcc
CFLAGS = -Wall -O3 -march=native -std=c11 -Iinclude -D_REENTRANT
LDFLAGS = -lm

# OS-specific settings
ifeq ($(OS),Windows_NT)
    CFLAGS += -I/mingw64/include/SDL2
    LDFLAGS += -L/mingw64/lib -lmingw32 -lSDL2main -lSDL2
    EXE_EXT = .exe
else
    # Linux (Ubuntu) / macOS
    CFLAGS += $(shell sdl2-config --cflags)
    LDFLAGS += $(shell sdl2-config --libs)
    EXE_EXT =
endif

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
EXE_NAME = singularity
EXE = $(BIN_DIR)/$(EXE_NAME)$(EXE_EXT)

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

# Bundle SDL2.dll with executable (Windows only)
bundle-sdl2: all
ifeq ($(OS),Windows_NT)
	@echo "Looking for SDL2.dll..."
	@cp /mingw64/bin/SDL2.dll $(BIN_DIR)/ 2>/dev/null && echo "SDL2.dll copied to $(BIN_DIR)/" || echo "Warning: SDL2.dll not found in /mingw64/bin/"
else
	@echo "Library bundling is not required on this OS (use dynamic linking)."
endif

# Create distribution package
dist: bundle-sdl2
	@echo "Distribution package ready in $(BIN_DIR)/"
	@ls -lh $(BIN_DIR)/

# Clean target
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean bundle-sdl2 dist
