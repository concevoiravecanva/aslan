CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./src -I./include

# Paths for Lua (macOS Homebrew)
LUA_INC = 
LUA_LIB = 

# Paths for SDL2 (macOS Homebrew)
SDL_INC = -I/opt/homebrew/include/SDL2
SDL_LIB = -L/opt/homebrew/lib -lSDL2

# Core, Document & Platform sources
CORE_OBJS = src/core/engine.o
DOC_OBJS = src/document/document.o \
           src/document/spatial_index.o \
           src/document/aslan_table.o
PLATFORM_OBJS = src/platform/platform.o \
           src/platform/renderer.o \
           src/platform/ui_table.o \
           src/platform/ui_canvas.o \
           src/platform/ui_design.o \
           src/platform/ui_chart.o \
           src/platform/ui_core.o \
           src/platform/ui_host.o \
           src/platform/recents.o \
           src/platform/image_loader.o \
           src/platform/mac_image.o

OBJS = src/main.o src/vendor/tinyexpr.o $(CORE_OBJS) $(DOC_OBJS) $(PLATFORM_OBJS)

TARGET = aslan
LDFLAGS = -framework Cocoa -framework CoreGraphics -framework ImageIO -lm

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS) $(SDL_LIB)

no_lua: CFLAGS += -DNO_LUA
no_lua: $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(SDL_LIB) $(LDFLAGS)

$(TEST_TARGET): $(CORE_OBJS) $(TEST_OBJ)
	$(CC) $(CORE_OBJS) $(TEST_OBJ) -o $(TEST_TARGET) $(SDL_LIB) $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

%.o: %.c $(wildcard src/*.h) $(wildcard src/**/*.h) $(wildcard include/*.h)
	$(CC) $(CFLAGS) $(LUA_INC) $(SDL_INC) -c $< -o $@

%.o: %.m $(wildcard src/*.h) $(wildcard src/**/*.h) $(wildcard include/*.h)
	$(CC) $(CFLAGS) $(LUA_INC) $(SDL_INC) -c $< -o $@

clean:
	rm -f $(OBJS) $(CORE_OBJS) $(TEST_OBJ) $(TARGET) $(TEST_TARGET)

run: $(TARGET)
	./$(TARGET)
