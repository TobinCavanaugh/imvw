#include "decoders.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <webp/decode.h>
#include <tiffio.h>
#include "tinyexr.h"
#include "external/stb_image.h"

#define IMVW_ICO_ENTRY_OVERRIDE -1

// Define basic types if not present
#ifndef u32
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
static u8 webp_probe(const u8 *data, size_t size) {
    if (size < 12) return 0;
    return (memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0);
}

static Image webp_decode(const u8 *data, size_t size) {
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

static u8 tiff_probe(const u8 *data, size_t size) {
    if (size < 4) return 0;
    return (memcmp(data, "II\x2a\x00", 4) == 0 || memcmp(data, "MM\x00\x2a", 4) == 0);
}

static Image tiff_decode(const u8 *data, size_t size) {
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
static u8 ico_probe(const u8 *data, size_t size) {
    if (size < 6) return 0;
    return (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01 && data[3] == 0x00);
}

static Image ico_decode(const u8 *data, size_t size) {
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

            if (d_size >= 8 && memcmp(entry_ptr, "\x89PNG\r\n\x1a\n", 8) == 0) {
                decoded = stbi_load_from_memory(entry_ptr, (int)d_size, &img.width, &img.height, &channels, 4);
            } else if (d_size >= 40) {
                uint32_t biSize, biWidth, biHeight;
                uint16_t biBitCount;
                memcpy(&biSize, entry_ptr, 4);
                memcpy(&biWidth, entry_ptr + 4, 4);
                memcpy(&biHeight, entry_ptr + 8, 4);
                memcpy(&biBitCount, entry_ptr + 14, 2);

                u8 *bmp_buffer = (u8 *)malloc(14 + d_size);
                if (bmp_buffer) {
                    IMVW_BMPFILEHEADER bfh = {0};
                    bfh.bfType = 0x4D42;
                    bfh.bfSize = 14 + d_size;
                    bfh.bfOffBits = 14 + biSize;

                    if (biBitCount <= 8) {
                        uint32_t clrUsed;
                        memcpy(&clrUsed, entry_ptr + 32, 4);
                        if (clrUsed == 0) clrUsed = (1 << biBitCount);
                        bfh.bfOffBits += clrUsed * 4;
                    }

                    memcpy(bmp_buffer, &bfh, 14);
                    memcpy(bmp_buffer + 14, entry_ptr, d_size);

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
void imvw_init_loaders() {
    if (decoders_count > 0) return;
    add_loader((image_decoder_t){"WebP", webp_probe, webp_decode});
    add_loader((image_decoder_t){"TIFF", tiff_probe, tiff_decode});
    add_loader((image_decoder_t){"ICO", ico_probe, ico_decode});
}

Image imvw_load_image_extended(const char *filepath) {
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

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

static const IID IID_IShellItemImageFactory_thumb = {
    0xbcc18b79, 0xba16, 0x442f, { 0x80, 0xc4, 0x8a, 0x59, 0xc3, 0x0c, 0x46, 0x3b }
};

Image imvw_get_thumbnail(const char *filepath, int max_w, int max_h) {
    Image img = {0};
    if (!filepath || !filepath[0]) return img;

    wchar_t raw_wpath[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, filepath, -1, raw_wpath, MAX_PATH) == 0) {
        MultiByteToWideChar(CP_ACP, 0, filepath, -1, raw_wpath, MAX_PATH);
    }
    for (wchar_t *p = raw_wpath; *p; p++) {
        if (*p == L'/') *p = L'\\';
    }
    wchar_t wpath[MAX_PATH];
    if (GetFullPathNameW(raw_wpath, MAX_PATH, wpath, NULL) == 0) {
        wcsncpy(wpath, raw_wpath, MAX_PATH);
    }

    IShellItemImageFactory *factory = NULL;
    HRESULT hr = SHCreateItemFromParsingName(wpath, NULL, &IID_IShellItemImageFactory_thumb, (void**)&factory);
    if (FAILED(hr) || !factory) {
        // Fallback: try raw_wpath directly
        hr = SHCreateItemFromParsingName(raw_wpath, NULL, &IID_IShellItemImageFactory_thumb, (void**)&factory);
        if (FAILED(hr) || !factory) return img;
    }

    if (max_w <= 0) max_w = 1024;
    if (max_h <= 0) max_h = 1024;
    SIZE size = { (LONG)max_w, (LONG)max_h };

    HBITMAP hbmp = NULL;
    // SIIGBF_RESIZETOFIT (0x00) shrinks/fits thumbnail preserving aspect ratio
    hr = factory->lpVtbl->GetImage(factory, size, 0x00 /*SIIGBF_RESIZETOFIT*/, &hbmp);
    if (FAILED(hr) || !hbmp) {
        hr = factory->lpVtbl->GetImage(factory, size, 0x01 /*SIIGBF_BIGGERSIZEOK*/, &hbmp);
    }
    if (FAILED(hr) || !hbmp) {
        hr = factory->lpVtbl->GetImage(factory, size, 0x08 /*SIIGBF_THUMBNAILONLY*/, &hbmp);
    }
    factory->lpVtbl->Release(factory);

    if (FAILED(hr) || !hbmp) return img;

    BITMAP bm;
    if (GetObject(hbmp, sizeof(BITMAP), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
        BITMAPINFO bi = {0};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = bm.bmWidth;
        bi.bmiHeader.biHeight = bm.bmHeight; // Must be positive for GetDIBits!
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        HDC hdc = GetDC(NULL);
        u8 *raw_dib = (u8*)malloc((size_t)bm.bmWidth * bm.bmHeight * 4);
        u8 *pixels  = (u8*)malloc((size_t)bm.bmWidth * bm.bmHeight * 4);
        if (raw_dib && pixels) {
            if (GetDIBits(hdc, hbmp, 0, bm.bmHeight, raw_dib, &bi, DIB_RGB_COLORS)) {
                // Check if any pixel has non-zero alpha
                int has_alpha = 0;
                for (int i = 0; i < bm.bmWidth * bm.bmHeight; i++) {
                    if (raw_dib[i * 4 + 3] > 0) {
                        has_alpha = 1;
                        break;
                    }
                }

                // DIB is bottom-up; invert Y and convert BGRA -> RGBA
                for (int y = 0; y < bm.bmHeight; y++) {
                    int src_row = bm.bmHeight - 1 - y;
                    for (int x = 0; x < bm.bmWidth; x++) {
                        u8 *src_px = raw_dib + (src_row * bm.bmWidth + x) * 4;
                        u8 *dst_px = pixels  + (y * bm.bmWidth + x) * 4;
                        dst_px[0] = src_px[2]; // R
                        dst_px[1] = src_px[1]; // G
                        dst_px[2] = src_px[0]; // B
                        dst_px[3] = has_alpha ? src_px[3] : 255; // A
                    }
                }

                img.data = pixels;
                img.width = bm.bmWidth;
                img.height = bm.bmHeight;
                img.mipmaps = 1;
                img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            } else {
                free(pixels);
            }
        } else {
            if (pixels) free(pixels);
        }
        if (raw_dib) free(raw_dib);
        ReleaseDC(NULL, hdc);
    }
    DeleteObject(hbmp);
    return img;
}

Image imvw_load_resource_icon(int resource_id, int width, int height) {
    Image img = {0};
    HINSTANCE hInst = GetModuleHandle(NULL);
    HICON hIcon = (HICON)LoadImageA(hInst, MAKEINTRESOURCEA(resource_id), IMAGE_ICON, width, height, LR_DEFAULTCOLOR);
    if (!hIcon) hIcon = LoadIconA(hInst, MAKEINTRESOURCEA(resource_id));
    if (!hIcon) return img;

    ICONINFO icon_info = {0};
    if (GetIconInfo(hIcon, &icon_info)) {
        BITMAP bm;
        if (GetObject(icon_info.hbmColor, sizeof(BITMAP), &bm) && bm.bmWidth > 0 && bm.bmHeight > 0) {
            BITMAPINFO bi = {0};
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = bm.bmWidth;
            bi.bmiHeader.biHeight = bm.bmHeight;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;

            HDC hdc = GetDC(NULL);
            u8 *raw_dib = (u8*)malloc((size_t)bm.bmWidth * bm.bmHeight * 4);
            u8 *pixels  = (u8*)malloc((size_t)bm.bmWidth * bm.bmHeight * 4);
            if (raw_dib && pixels) {
                if (GetDIBits(hdc, icon_info.hbmColor, 0, bm.bmHeight, raw_dib, &bi, DIB_RGB_COLORS)) {
                    int has_alpha = 0;
                    for (int i = 0; i < bm.bmWidth * bm.bmHeight; i++) {
                        if (raw_dib[i * 4 + 3] > 0) {
                            has_alpha = 1;
                            break;
                        }
                    }

                    for (int y = 0; y < bm.bmHeight; y++) {
                        int src_row = bm.bmHeight - 1 - y;
                        for (int x = 0; x < bm.bmWidth; x++) {
                            u8 *src_px = raw_dib + (src_row * bm.bmWidth + x) * 4;
                            u8 *dst_px = pixels  + (y * bm.bmWidth + x) * 4;
                            dst_px[0] = src_px[2]; // R
                            dst_px[1] = src_px[1]; // G
                            dst_px[2] = src_px[0]; // B
                            dst_px[3] = has_alpha ? src_px[3] : 255; // A
                        }
                    }

                    img.data = pixels;
                    img.width = bm.bmWidth;
                    img.height = bm.bmHeight;
                    img.mipmaps = 1;
                    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                } else {
                    free(pixels);
                }
            } else {
                if (pixels) free(pixels);
            }
            if (raw_dib) free(raw_dib);
            ReleaseDC(NULL, hdc);
        }
        if (icon_info.hbmColor) DeleteObject(icon_info.hbmColor);
        if (icon_info.hbmMask)  DeleteObject(icon_info.hbmMask);
    }
    DestroyIcon(hIcon);
    return img;
}
#else
Image imvw_get_thumbnail(const char *filepath, int max_w, int max_h) {
    (void)filepath; (void)max_w; (void)max_h;
    Image img = {0};
    return img;
}
Image imvw_load_resource_icon(int resource_id, int width, int height) {
    (void)resource_id; (void)width; (void)height;
    Image img = {0};
    return img;
}
#endif
