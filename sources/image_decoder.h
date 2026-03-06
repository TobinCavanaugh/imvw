#ifndef IMAGE_DECODER_H
#define IMAGE_DECODER_H





#include "raylib.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Extended format libraries
#include <webp/decode.h>
#include <tiffio.h>

// Helper to get file extensions
static const char* get_file_ext(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

Image imvw_load_image_extended(const char* filepath) {
    Image img = { 0 };
    const char* ext = get_file_ext(filepath);

    // ==========================================
    // 1. WebP Decoding
    // ==========================================
    if (strcasecmp(ext, "webp") == 0) {
        int data_size;
        // Load the raw binary file into memory using Raylib
        unsigned char* file_data = LoadFileData(filepath, &data_size);

        if (file_data) {
            int width, height;
            // Decode the WebP into an RGBA byte array
            uint8_t* webp_data = WebPDecodeRGBA(file_data, data_size, &width, &height);
            UnloadFileData(file_data); // Free the raw file binary

            if (webp_data) {
                img.width = width;
                img.height = height;
                img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                img.mipmaps = 1;

                // Allocate Raylib-compatible memory and copy the pixels
                int byte_size = width * height * 4;
                img.data = RL_MALLOC(byte_size);
                memcpy(img.data, webp_data, byte_size);

                WebPFree(webp_data); // Free WebP's internal allocation
                return img;
            }
        }
    }

        // ==========================================
        // 2. TIFF Decoding
        // ==========================================
    else if (strcasecmp(ext, "tif") == 0 || strcasecmp(ext, "tiff") == 0) {
        TIFF* tif = TIFFOpen(filepath, "r");
        if (tif) {
            uint32_t width, height;
            TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
            TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);

            // Allocate memory for TIFF to unpack into
            uint32_t* raster = (uint32_t*) _TIFFmalloc(width * height * sizeof(uint32_t));
            if (raster) {
                // Read the image (ORIENTATION_TOPLEFT prevents it from drawing upside down)
                if (TIFFReadRGBAImageOriented(tif, width, height, raster, ORIENTATION_TOPLEFT, 0)) {
                    img.width = width;
                    img.height = height;
                    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                    img.mipmaps = 1;

                    int byte_size = width * height * 4;
                    img.data = RL_MALLOC(byte_size);

                    // TIFF outputs in ABGR format on little-endian systems.
                    // We need to manually swap it to RGBA so the colors aren't inverted in Raylib.
                    uint8_t* dst = (uint8_t*)img.data;
                    uint8_t* src = (uint8_t*)raster;
                    for (int i = 0; i < byte_size; i += 4) {
                        dst[i + 0] = src[i + 0]; // R
                        dst[i + 1] = src[i + 1]; // G
                        dst[i + 2] = src[i + 2]; // B
                        dst[i + 3] = src[i + 3]; // A
                    }
                }
                _TIFFfree(raster);
            }
            TIFFClose(tif);
            if (img.data) return img; // Return if successful
        }
    }

    // ==========================================
    // 3. Native Raylib Fallback (PNG, JPG, BMP)
    // ==========================================
    img = LoadImage(filepath);
    if (img.data == NULL) {
        fprintf(stderr, "RAYLIB_ERR: Failed to load %s\n", filepath);
    }
    return img;
}


#endif // IMAGE_DECODER_H
