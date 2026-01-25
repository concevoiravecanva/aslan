#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>
#include "mac_image.h"

void *mac_load_image_rgba(const char *path, int *w, int *h) {
    if (!path) return NULL;
    NSString *nsPath = [NSString stringWithUTF8String:path];
    if (!nsPath) return NULL;
    
    // Check if file exists
    if (![[NSFileManager defaultManager] fileExistsAtPath:nsPath]) {
        // Try to resolving relative path if needed
        if (![nsPath isAbsolutePath]) {
            NSString *cwd = [[NSFileManager defaultManager] currentDirectoryPath];
            nsPath = [cwd stringByAppendingPathComponent:nsPath];
        }
    }

    // Debug logging
    FILE *f = fopen("ui_debug.log", "a");
    if (f) {
        fprintf(f, "mac_load_image_rgba: %s\n", [nsPath UTF8String]);
        fclose(f);
    }

    NSURL *url = [NSURL fileURLWithPath:nsPath];
    CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)url, NULL);
    if (!source) return NULL;

    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
    CFRelease(source);
    if (!image) return NULL;

    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    if (w) *w = (int)width;
    if (h) *h = (int)height;

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
    CGContextRef context = CGBitmapContextCreate(data, width, height, bitsPerComponent, bytesPerRow, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    
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
