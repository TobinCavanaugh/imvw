#ifndef IMAGE_DECODER_H
#define IMAGE_DECODER_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <webp/decode.h>
#include <tiffio.h>
#include "tinyexr.h"

#define IMVW_ICO_ENTRY_OVERRIDE -1

#ifndef RAYLIB_H
typedef struct Image {
    void *data;
    int width;
    int height;
    int mipmaps;
    int format;
} Image;
#define PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 7
#endif

// Define basic types if not present
#ifndef u8
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;
typedef void u0;
#endif

#ifdef _MSC_VER
#pragma pack(push, 1)
typedef struct {
#else
typedef struct __attribute__((packed)) {
#endif
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} IMVW_BMPFILEHEADER;
#ifdef _MSC_VER
#pragma pack(pop)
#endif

// Registry Structure
typedef struct {
    const char *name;
    u8 (*probe)(const u8 *data, size_t size);
    Image (*decode)(const u8 *data, size_t size);
} image_decoder_t;

static image_decoder_t *decoders_array = NULL;
static i32 decoders_count = 0;

static unsigned char *load_file_to_buffer(const char *filepath, size_t *size) {
    FILE *f = fopen(filepath, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        *size = ftell(f);
        fseek(f, 0, SEEK_SET);

        u8 *file_data = (u8 *)malloc(*size);
        if (file_data) {
            size_t read = fread(file_data, 1, *size, f);
            (void)read;
        }
        fclose(f);
        return file_data;
    }
    return NULL;
}

static u0 add_loader(image_decoder_t dec) {
    decoders_array = (image_decoder_t *)realloc(decoders_array, (decoders_count + 1) * sizeof(image_decoder_t));
    decoders_array[decoders_count] = dec;
    ++decoders_count;
}

// --- WebP Decoder ---
u8 webp_probe(const u8 *data, size_t size) {
    if (size < 12) return 0;
    return (memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0);
}

Image webp_decode(const u8 *data, size_t size) {
    Image img = {0};
    int width, height;
    uint8_t *webp_data = WebPDecodeRGBA(data, (int)size, &width, &height);
    if (webp_data) {
        img.width = width;
        img.height = height;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.mipmaps = 1;
        img.data = webp_data;
    }
    return img;
}

// --- TIFF Decoder ---
typedef struct {
    const u8 *data;
    tsize_t size;
    tsize_t pos;
} tiff_mem_t;

static tsize_t _tiff_read(thandle_t st, tdata_t buf, tsize_t size) {
    tiff_mem_t *m = (tiff_mem_t *)st;
    tsize_t n = (m->pos + size > m->size) ? (m->size - m->pos) : size;
    memcpy(buf, m->data + m->pos, n);
    m->pos += n;
    return n;
}

static toff_t _tiff_seek(thandle_t st, toff_t off, int whence) {
    tiff_mem_t *m = (tiff_mem_t *)st;
    if (whence == SEEK_SET) m->pos = (tsize_t)off;
    else if (whence == SEEK_CUR) m->pos += (tsize_t)off;
    else if (whence == SEEK_END) m->pos = m->size + (tsize_t)off;
    return (toff_t)m->pos;
}

static int _tiff_close(thandle_t st) { (void)st; return 0; }
static toff_t _tiff_size(thandle_t st) { return (toff_t)((tiff_mem_t *)st)->size; }

u8 tiff_probe(const u8 *data, size_t size) {
    if (size < 4) return 0;
    return (memcmp(data, "II\x2a\x00", 4) == 0 || memcmp(data, "MM\x00\x2a", 4) == 0);
}

Image tiff_decode(const u8 *data, size_t size) {
    Image img = {0};
    tiff_mem_t m = {data, (tsize_t)size, 0};
    TIFF *tif = TIFFClientOpen("mem", "r", (thandle_t)&m, _tiff_read, _tiff_read, _tiff_seek, _tiff_close, _tiff_size, NULL, NULL);
    if (tif) {
        uint32_t w, h;
        TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
        TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
        uint32_t *raster = (uint32_t *)_TIFFmalloc(w * h * sizeof(uint32_t));
        if (raster) {
            if (TIFFReadRGBAImageOriented(tif, w, h, raster, ORIENTATION_TOPLEFT, 0)) {
                img.width = (int)w;
                img.height = (int)h;
                img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                img.mipmaps = 1;
                img.data = malloc(w * h * 4);
                if (img.data) memcpy(img.data, raster, w * h * 4);
            }
            _TIFFfree(raster);
        }
        TIFFClose(tif);
    }
    return img;
}

// --- ICO Decoder ---
u8 ico_probe(const u8 *data, size_t size) {
    if (size < 6) return 0;
    // Magic: 00 00 01 00
    return (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x00);
}

Image ico_decode(const u8 *data, size_t size) {
    Image img = {0};
    uint16_t count;
    memcpy(&count, data + 4, 2);

    int selected = -1;
    int best_area = -1;

    for (int i = 0; i < count; i++) {
        size_t entry_offset = 6 + i * 16;
        if (entry_offset + 16 > size) break;

        int w = data[entry_offset] == 0 ? 256 : data[entry_offset];
        int h = data[entry_offset + 1] == 0 ? 256 : data[entry_offset + 1];
        int area = w * h;

        if (IMVW_ICO_ENTRY_OVERRIDE >= 0) {
            if (i == IMVW_ICO_ENTRY_OVERRIDE) { selected = i; break; }
        } else if (area > best_area) {
            best_area = area;
            selected = i;
        }
    }

    if (selected >= 0) {
        size_t entry_offset = 6 + selected * 16;
        uint32_t d_size, d_offset;
        memcpy(&d_size, data + entry_offset + 8, 4);
        memcpy(&d_offset, data + entry_offset + 12, 4);

        if (d_offset + d_size <= size) {
            const u8 *entry_ptr = data + d_offset;
            int channels;
            u8 *decoded = NULL;

            // PNG compressed icon (Vista and newer)
            if (d_size >= 8 && memcmp(entry_ptr, "\x89PNG\r\n\x1a\n", 8) == 0) {
                decoded = stbi_load_from_memory(entry_ptr, (int)d_size, &img.width, &img.height, &channels, 4);
            } else if (d_size >= 40) {
                // Raw DIB: BITMAPINFOHEADER starts here.
                // We construct a full BMP header to trick stbi.
                uint32_t biSize, biWidth, biHeight;
                uint16_t biBitCount;
                memcpy(&biSize, entry_ptr, 4);
                memcpy(&biWidth, entry_ptr + 4, 4);
                memcpy(&biHeight, entry_ptr + 8, 4);
                memcpy(&biBitCount, entry_ptr + 14, 2);

                // ICO DIB height is double (XOR + AND mask). stb_image needs the real height.
                // We'll patch the height in a temporary buffer before sending to stbi.
                u8 *bmp_buffer = (u8 *)malloc(14 + d_size);
                if (bmp_buffer) {
                    IMVW_BMPFILEHEADER bfh = {0};
                    bfh.bfType = 0x4D42; // 'BM'
                    bfh.bfSize = 14 + d_size;

                    // In ICO, the DIB header is biSize. bfOffBits is 14 + biSize + palette.
                    // However, stb_image is very good at handling the DIB as a BMP if we just
                    // provide the header and the data as-is.
                    bfh.bfOffBits = 14 + biSize;

                    // Handle Palette: If biBitCount <= 8, there's a palette after the header.
                    if (biBitCount <= 8) {
                        uint32_t clrUsed;
                        memcpy(&clrUsed, entry_ptr + 32, 4);
                        if (clrUsed == 0) clrUsed = (1 << biBitCount);
                        bfh.bfOffBits += clrUsed * 4;
                    }

                    memcpy(bmp_buffer, &bfh, 14);
                    memcpy(bmp_buffer + 14, entry_ptr, d_size);

                    // Patch the height in the BITMAPINFOHEADER inside the BMP buffer
                    uint32_t real_height = biHeight / 2;
                    memcpy(bmp_buffer + 14 + 8, &real_height, 4);

                    decoded = stbi_load_from_memory(bmp_buffer, 14 + (int)d_size, &img.width, &img.height, &channels, 4);
                    free(bmp_buffer);
                }
            }

            if (decoded) {
                img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                img.mipmaps = 1;
                img.data = decoded;
            }
        }
    }
    return img;
}

// --- Public API ---
u0 imvw_init_loaders() {
    if (decoders_count > 0) return;
    add_loader((image_decoder_t){"WebP", webp_probe, webp_decode});
    add_loader((image_decoder_t){"TIFF", tiff_probe, tiff_decode});
    add_loader((image_decoder_t){"ICO", ico_probe, ico_decode});
}

static Image imvw_load_image_extended(const char *filepath) {
    size_t size = 0;
    u8 *file_data = load_file_to_buffer(filepath, &size);
    Image img = {0};
    if (!file_data) return img;

    for (int i = 0; i < decoders_count; i++) {
        if (decoders_array[i].probe(file_data, size)) {
            img = decoders_array[i].decode(file_data, size);
            if (img.data) {
                free(file_data);
                return img;
            }
        }
    }

    int channels;
    u8 *stbi_data = stbi_load_from_memory(file_data, (int)size, &img.width, &img.height, &channels, 4);
    if (stbi_data) {
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.mipmaps = 1;
        img.data = stbi_data;
    }

    free(file_data);
    return img;
}

#endif // IMAGE_DECODER_H
