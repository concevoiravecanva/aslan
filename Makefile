CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I./src -I./include

# Paths for Lua (macOS Homebrew)
LUA_INC = 
LUA_LIB = 

# Paths for SDL2 + SDL_ttf (macOS Homebrew)
SDL_INC = -I/opt/homebrew/include/SDL2 -I/opt/homebrew/include
SDL_LIB = -L/opt/homebrew/lib -lSDL2 -lSDL2_ttf

CFLAGS = -Wall -Wextra -std=c99 -I./src -I./include -DFONS_NO_STB_TRUETYPE_IMPL

# Core, Document & Platform sources
CORE_OBJS = src/core/engine.o
DOC_OBJS = src/document/document.o \
           src/document/aslan_table.o
PLATFORM_OBJS = src/platform/platform.o \
           src/platform/renderer.o \
           src/platform/clay_host.o \
           src/platform/ui_table.o \
           src/platform/ui_canvas.o \
           src/platform/ui_design.o \
           src/platform/ui_chart.o \
           src/platform/ui_core.o \
           src/platform/ui_host.o \
           src/platform/nvg_widgets.o \
           src/platform/recents.o \
           src/platform/image_loader.o \
           src/platform/mac_image.o
VENDOR_OBJS = src/vendor/tinyexpr.o \
              src/vendor/stb_image_write.o \
              src/vendor/nanovg/nanovg.o \
              src/vendor/nanovg/nanovg_gl_impl.o

OBJS = src/main.o $(VENDOR_OBJS) $(CORE_OBJS) $(DOC_OBJS) $(PLATFORM_OBJS)

TARGET = aslan
LDFLAGS = -framework Cocoa -framework CoreGraphics -framework ImageIO -framework OpenGL -lm -lhpdf -lz -lpng

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
