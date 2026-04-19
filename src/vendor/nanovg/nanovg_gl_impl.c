#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#elif defined(__APPLE__)
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif

#include "nanovg.h"
#ifdef __EMSCRIPTEN__
#  define NANOVG_GLES3_IMPLEMENTATION
#else
#  define NANOVG_GL3_IMPLEMENTATION
#endif
#include "nanovg_gl.h"
#define NANOVG_GL_UTILS_IMPLEMENTATION
#include "nanovg_gl_utils.h"
