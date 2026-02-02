#include "mac_image.h"
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

void *mac_load_image_rgba(const char *path, int *w, int *h) {
  if (!path)
    return NULL;
  NSString *nsPath = [[NSFileManager defaultManager]
      stringWithFileSystemRepresentation:path
                                  length:strlen(path)];
  if (!nsPath) {
    fprintf(stderr,
            "[ERROR] mac_load_image_rgba: failed to create NSString from %s\n",
            path);
    return NULL;
  }

  // Debug logging
  FILE *f = fopen("ui_debug.log", "a");
  if (f) {
    fprintf(f, "[DEBUG] mac_load_image_rgba called for: %s\n", path);
    fclose(f);
  }

  // Check if file exists
  if (![[NSFileManager defaultManager] fileExistsAtPath:nsPath]) {
    // Try to resolving relative path if needed
    if (![nsPath isAbsolutePath]) {
      NSString *cwd = [[NSFileManager defaultManager] currentDirectoryPath];
      nsPath = [cwd stringByAppendingPathComponent:nsPath];
    }
  }

  // No second log needed here as we logged at the start

  NSURL *url = [NSURL fileURLWithPath:nsPath];
  CGImageSourceRef source =
      CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
  if (!source)
    return NULL;

  CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
  CFRelease(source);
  if (!image)
    return NULL;

  size_t width = CGImageGetWidth(image);
  size_t height = CGImageGetHeight(image);
  if (w)
    *w = (int)width;
  if (h)
    *h = (int)height;

  size_t bytesPerPixel = 4;
  size_t bytesPerRow = width * bytesPerPixel;
  size_t bitsPerComponent = 8;

  void *data = calloc(1, height * bytesPerRow);
  if (!data) {
    CGImageRelease(image);
    return NULL;
  }

  // Use kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big for RGBA
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      data, width, height, bitsPerComponent, bytesPerRow, colorSpace,
      kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);

  if (context) {
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), image);
    CGContextRelease(context);
  } else {
    free(data);
    data = NULL;
  }

  CGColorSpaceRelease(colorSpace);
  CGImageRelease(image);

  return data;
}

int mac_get_image_size(const char *path, int *w, int *h) {
  if (!path)
    return 0;
  NSString *nsPath = [[NSFileManager defaultManager]
      stringWithFileSystemRepresentation:path
                                  length:strlen(path)];
  if (!nsPath)
    return 0;

  // Check if file exists
  if (![[NSFileManager defaultManager] fileExistsAtPath:nsPath]) {
    if (![nsPath isAbsolutePath]) {
      NSString *cwd = [[NSFileManager defaultManager] currentDirectoryPath];
      nsPath = [cwd stringByAppendingPathComponent:nsPath];
    }
  }

  NSURL *url = [NSURL fileURLWithPath:nsPath];
  CGImageSourceRef source =
      CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
  if (!source)
    return 0;

  CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
  CFRelease(source);
  if (!image)
    return 0;

  if (w)
    *w = (int)CGImageGetWidth(image);
  if (h)
    *h = (int)CGImageGetHeight(image);

  CGImageRelease(image);
  return 1;
}
