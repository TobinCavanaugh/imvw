#ifndef IMAGE_DECODER_H
#define IMAGE_DECODER_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Extended format libraries
#include <webp/decode.h>
#include <tiffio.h>

// stb_image implementation
//#define STB_IMAGE_IMPLEMENTATION
//#include "stb_image.h"

// Raylib-compatible Image structure (if not using raylib.h)
#ifndef RAYLIB_H
typedef struct Image {
    void *data;             // Image raw data
    int width;              // Image base width
    int height;             // Image base height
    int mipmaps;            // Mipmap levels, 1 by default
    int format;             // Data format (7 for R8G8B8A8)
} Image;

// Define specific raylib format if header is missing
#define PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 7
#endif

// Windows compatibility for string comparison
#ifdef _WIN32
#define strcasecmp _stricmp
#endif

// Helper to get file extensions
static const char *imvw_get_file_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

/**
 * Loads an image from disk using stbi, libwebp, or libtiff.
 * Returns a raylib-compatible Image struct.
 * Memory for img.data is allocated via malloc/realloc.
 */
static Image imvw_load_image_extended(const char *filepath) {
    Image img = {0};
    const char *ext = imvw_get_file_ext(filepath);

    // ==========================================
    // 1. WebP Decoding (Manual via libwebp)
    // ==========================================
    if (strcasecmp(ext, "webp") == 0) {
        FILE *f = fopen(filepath, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);

            unsigned char *file_data = (unsigned char *) malloc(size);
            fread(file_data, 1, size, f);
            fclose(f);

            int width, height;
            uint8_t *webp_data = WebPDecodeRGBA(file_data, (int) size, &width, &height);
            free(file_data);

            if (webp_data) {
                img.width = width;
                img.height = height;
                img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                img.mipmaps = 1;

                size_t byte_size = (size_t) width * height * 4;
                img.data = malloc(byte_size);
                if (img.data) memcpy(img.data, webp_data, byte_size);

                WebPFree(webp_data);
                return img;
            }
        }
    }

        // ==========================================
        // 2. TIFF Decoding (Manual via libtiff)
        // ==========================================
    else if (strcasecmp(ext, "tif") == 0 || strcasecmp(ext, "tiff") == 0) {
        TIFF *tif = TIFFOpen(filepath, "r");
        if (tif) {
            uint32_t width, height;
            TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

            uint32_t *raster = (uint32_t *) _TIFFmalloc(width * height * sizeof(uint32_t));
            if (raster) {
                if (TIFFReadRGBAImageOriented(tif, width, height, raster, ORIENTATION_TOPLEFT, 0)) {
                    img.width = (int) width;
                    img.height = (int) height;
                    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    img.mipmaps = 1;

                    size_t byte_size = (size_t) width * height * 4;
                    img.data = malloc(byte_size);
                    if (img.data) memcpy(img.data, raster, byte_size);
                }
                _TIFFfree(raster);
            }
            TIFFClose(tif);
            if (img.data) return img;
        }
    }

    // ==========================================
    // 3. STBI Fallback (PNG, JPG, BMP, etc.)
    // ==========================================
    int channels;
    unsigned char *stbi_data = stbi_load(filepath, &img.width, &img.height, &channels, 4);

    if (stbi_data) {
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.mipmaps = 1;

        size_t byte_size = (size_t) img.width * img.height * 4;
        img.data = malloc(byte_size);
        if (img.data) {
            memcpy(img.data, stbi_data, byte_size);
        }

        stbi_image_free(stbi_data);
    } else {
        fprintf(stderr, "IMG_DEC_ERR: stbi failed to load %s (Reason: %s)\n", filepath, stbi_failure_reason());
    }

    return img;
}

#endif // IMAGE_DECODER_H
