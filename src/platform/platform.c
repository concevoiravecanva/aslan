#include "platform.h"
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

void platform_init(void) {
    // Init OS specific stuff
}

void platform_shutdown(void) {
    // Cleanup OS specific stuff
}

void platform_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void platform_sleep(int ms) {
    #ifdef _WIN32
    Sleep(ms);
    #else
    usleep(ms * 1000);
    #endif
}

bool platform_create_window(const char* title, int width, int height) {
    platform_log("Window created: '%s' (%dx%d) [STUB]", title, width, height);
    return true;
}

void platform_poll_events(void) {
    // Ici, on traiterait les messages OS (PeekMessage, SDL_PollEvent...)
}
