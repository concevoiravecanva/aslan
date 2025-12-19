CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./src -I./include

# Paths for Lua (Adjusted for Homebrew)
LUA_INC = -I/opt/homebrew/include/lua5.4
LUA_LIB = -L/opt/homebrew/lib -llua

# Paths for SDL2 (macOS Homebrew)
SDL_INC = -I/opt/homebrew/include
SDL_LIB = -L/opt/homebrew/lib -lSDL2

SRC = src/main.c \
      src/document/document.c \
      src/scripting/lua_host.c \
      src/platform/platform.c \
      src/platform/renderer.c \
      src/platform/ui_host.c \
      src/core/engine.c

OBJ = $(SRC:.c=.o)
TARGET = aslan_proto

# Default target tries to build with Lua
all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LUA_LIB) $(SDL_LIB) -lm

# Target to build without Lua (if libraries are missing)
no_lua: CFLAGS += -DNO_LUA
no_lua: $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(SDL_LIB)

%.o: %.c
	$(CC) $(CFLAGS) $(LUA_INC) $(SDL_INC) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)
