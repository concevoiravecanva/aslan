CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./src -I./include

# Paths for Lua (Adjusted for Homebrew)
LUA_INC = -I/opt/homebrew/include/lua5.4
LUA_LIB = -L/opt/homebrew/lib -llua

# Paths for SDL2 (macOS Homebrew)
SDL_INC = -I/opt/homebrew/include/SDL2
SDL_LIB = -L/opt/homebrew/lib -lSDL2
FRAMEWORKS = -framework Cocoa -framework CoreGraphics -framework ImageIO

CORE_SRC = src/document/document.c \
           src/document/spatial_index.c \
           src/document/aslan_table.c \
           src/scripting/lua_host.c \
           src/scripting/lua_nk.c \
           src/platform/platform.c \
           src/platform/renderer.c \
           src/platform/ui_table.c \
           src/platform/ui_canvas.c \
           src/platform/ui_design.c \
           src/platform/ui_chart.c \
           src/platform/ui_core.c \
           src/platform/ui_host.c \
           src/platform/recents.c \
           src/core/engine.c

OBJC_SRC = src/platform/mac_image.m

SRC = src/main.c \
      $(CORE_SRC)

OBJ = $(SRC:.c=.o) $(OBJC_SRC:.m=.o)
CORE_OBJ = $(CORE_SRC:.c=.o) $(OBJC_SRC:.m=.o)

TARGET = aslan
TEST_TARGET = aslan_tests
TEST_SRC = src/tests/engine_smoke_test.c
TEST_OBJ = $(TEST_SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(LUA_LIB) $(SDL_LIB) $(FRAMEWORKS) -lm

no_lua: CFLAGS += -DNO_LUA
no_lua: $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) $(SDL_LIB) $(FRAMEWORKS)

$(TEST_TARGET): $(CORE_OBJ) $(TEST_OBJ)
	$(CC) $(CORE_OBJ) $(TEST_OBJ) -o $(TEST_TARGET) $(LUA_LIB) $(SDL_LIB) $(FRAMEWORKS) -lm

test: $(TEST_TARGET)
	./$(TEST_TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(LUA_INC) $(SDL_INC) -c $< -o $@

%.o: %.m
	$(CC) $(CFLAGS) $(LUA_INC) $(SDL_INC) -c $< -o $@

clean:
	rm -f $(OBJ) $(CORE_OBJ) $(TEST_OBJ) $(TARGET) $(TEST_TARGET)

run: $(TARGET)
	./$(TARGET)
