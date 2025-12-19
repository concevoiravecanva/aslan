#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

// Abstraction de la plateforme (OS)
// Permet de changer l'implémentation (Console, SDL, GLFW, Native Win32/Cocoa)
// sans toucher au cœur du moteur.

void platform_init(void);
void platform_shutdown(void);
void platform_log(const char* fmt, ...);
void platform_sleep(int ms);

// Gestion de fenêtre (Stub)
bool platform_create_window(const char* title, int width, int height);
void platform_poll_events(void);

#endif
