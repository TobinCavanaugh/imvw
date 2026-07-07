#ifndef IMAGE_DECODERS_H
#define IMAGE_DECODERS_H

#include <stddef.h>
#include <stdint.h>

// Minimal Image struct matching raylib's layout (used when raylib headers
// aren't included in this compilation unit).
#ifndef RAYLIB_H
typedef struct {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
} Image;
#define PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 7
#endif

#ifndef u8
typedef uint8_t u8;
#endif

// Register built-in decoders (WebP, TIFF, ICO).  Safe to call multiple times.
void imvw_init_loaders(void);

// Try each registered decoder probe in order; if none match, fall back to
// stb_image.  Returns a zeroed Image on failure.
Image imvw_load_image_extended(const char *filepath);

#endif // IMAGE_DECODERS_H
