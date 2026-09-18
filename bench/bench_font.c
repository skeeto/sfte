/*
    bench/bench_font.c - headless font lookup and render benchmark for sfte

    Measures the per-frame cost of the core render pass, most notably the
    rune -> glyph id resolution that runs for every visible cell on every
    sfte_render() call, plus the number of backend GET_ID and BAKE calls.

    Two backends can be benchmarked:
      - DirectWrite (default)  : custom backend, ClearType atlas
      - stb_truetype (-DBENCH_STB): custom backend wrapper around stb_truetype

    Usage:
        bench_font.exe [frames] [mode]
            frames : measured frames (default 3000)
            mode   : 0 = idle grid (default), 1 = one new line per frame,
                     2 = grid filled with distinct runes (memo capacity stress)

    Manual build from the repository root (w64devkit/MinGW):

        gcc -std=gnu11 -O2 -Wall -Wextra -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006 \
            -DWIN32_LEAN_AND_MEAN -DNOMINMAX -I. bench/bench_font.c -o bench_font.exe \
            -ldwrite -lole32 -luser32

        gcc -std=gnu11 -O2 -Wall -Wextra -D_WIN32_WINNT=0x0A00 -DNTDDI_VERSION=0x0A000006 \
            -DWIN32_LEAN_AND_MEAN -DNOMINMAX -DBENCH_STB -I. bench/bench_font.c \
            -o bench_font_stb.exe -luser32

    Or with CMake:
        cmake -S . -B build -DSFTE_BUILD_BENCH=ON
        cmake --build build
*/

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static uint64_t g_get_id_calls;
static uint64_t g_bake_calls;

#ifndef BENCH_STB

#define SFTE_CUSTOM_BACKEND
#define SFTE_NO_POSIX
#define SFTE_FONT_CUSTOM_BACKEND
#define SFTE_FONT_SUBPIXEL 1
#include "sfte_dwrite.h"

static int _bench_get_id(sfte_font_backend_info *info, uint32_t rune) {
    g_get_id_calls++;
    return _sfte_dwrite_get_id(info, rune);
}

static void _bench_bake(void *ctx, sfte_font_backend_info *info, int glyph_id, float scale,
                        uint8_t *atlas_ptr, int gw, int gh, int atlas_stride) {
    g_bake_calls++;
    _sfte_dwrite_bake(ctx, info, glyph_id, scale, atlas_ptr, gw, gh, atlas_stride);
}

#undef SFTE_FONT_GET_ID
#undef SFTE_FONT_BAKE
#define SFTE_FONT_GET_ID _bench_get_id
#define SFTE_FONT_BAKE _bench_bake

#else  // BENCH_STB

#define SFTE_CUSTOM_BACKEND
#define SFTE_NO_POSIX
#define SFTE_FONT_CUSTOM_BACKEND
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "vendor/stb_truetype.h"

typedef struct sfte_font_backend_info {
    stbtt_fontinfo stb;
} sfte_font_backend_info;

static void _bench_stb_init(sfte_font_backend_info *info, const uint8_t *data) {
    stbtt_InitFont(&info->stb, data, 0);
}

static float _bench_stb_get_scale(sfte_font_backend_info *info, float px_hei) {
    return stbtt_ScaleForPixelHeight(&info->stb, px_hei);
}

static void _bench_stb_vmetrics(sfte_font_backend_info *info, int *ascent, int *descent,
                                int *linegap) {
    stbtt_GetFontVMetrics(&info->stb, ascent, descent, linegap);
}

static void _bench_stb_bounds(sfte_font_backend_info *info, int glyph_id, float scale, int *adv,
                              int *x0, int *y0, int *x1, int *y1) {
    int lsb;
    stbtt_GetGlyphHMetrics(&info->stb, glyph_id, adv, &lsb);
    stbtt_GetGlyphBitmapBox(&info->stb, glyph_id, scale, scale, x0, y0, x1, y1);
}

static void _bench_stb_bake(void *ctx, sfte_font_backend_info *info, int glyph_idx, float scale,
                            uint8_t *atlas_ptr, int gw, int gh, int atlas_stride) {
    (void)ctx;
    g_bake_calls++;
    stbtt_MakeGlyphBitmap(&info->stb, atlas_ptr, gw, gh, atlas_stride, scale, scale, glyph_idx);
}

static int _bench_stb_get_id(sfte_font_backend_info *info, uint32_t rune) {
    g_get_id_calls++;
    return stbtt_FindGlyphIndex(&info->stb, rune);
}

#define SFTE_FONT_INIT _bench_stb_init
#define SFTE_FONT_GET_SCALE _bench_stb_get_scale
#define SFTE_FONT_VMETRICS _bench_stb_vmetrics
#define SFTE_FONT_BOUNDS _bench_stb_bounds
#define SFTE_FONT_BAKE _bench_stb_bake
#define SFTE_FONT_GET_ID _bench_stb_get_id

#endif  // BENCH_STB

// Keep the measurement focused on the render/lookup path.
#define SFTE_FONT_BOLD
#define SFTE_FONT_ITALIC
#define SFTE_FONT_BOLD_ITALIC
#define SFTE_TERM_SCROLL_SMOOTH 0
#define SFTE_TERM_ANIMATE_SCREEN 0
#define SFTE_CURSOR_BLINK 0
#define SFTE_CURSOR_TRAIL 0

#define SFTE_IMPL
#include "sfte_dev.h"

static double _bench_now_ms(void) {
    static LARGE_INTEGER freq;
    LARGE_INTEGER counter;

    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);

    return (double)counter.QuadPart * 1000.0 / (double)freq.QuadPart;
}

/*
    Fills a buffer with representative terminal content: ASCII, punctuation,
    box drawing, symbols, latin-1 and runes missing from Consolas (negative
    lookups), one line per row.
*/
static size_t _bench_build_screen(char *buf, size_t cap, int rows) {
    static const char *lines[] = {
        "The quick brown fox jumps over the lazy dog 0123456789",
        "!@#$%^&*()_+-=[]{}|;':\",./<>?`~",
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz",
        "sig: () => {} != == === <= >= -> <- || && ...",
        "\xe2\x94\x8c\xe2\x94\x80\xe2\x94\xac\xe2\x94\x80\xe2\x94\x90 "
        "\xe2\x94\x82 \xe2\x94\x9c\xe2\x94\x80\xe2\x94\xbc\xe2\x94\x80\xe2\x94\xa4 "
        "\xe2\x94\x94\xe2\x94\xb4\xe2\x94\x80\xe2\x94\x98",
        "\xe2\x96\x91\xe2\x96\x92\xe2\x96\x93\xe2\x96\x88 \xe2\x9c\x93 \xe2\x9c\x97 "
        "\xe2\x86\x92 \xe2\x86\x90 \xe2\x86\x91 \xe2\x86\x93",
        "latin: \xc3\xa9\xc3\xa8\xc3\xaa\xc3\xab \xc3\xb1 \xc3\xbc \xc3\x9f "
        "\xc2\xa9 \xc2\xae \xc2\xb0 \xc2\xb1 \xc3\x97 \xc3\xb7",
        "missing: \xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e \xe4\xb8\xad\xe6\x96\x87 "
        "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4",
    };
    size_t line_cnt = sizeof(lines) / sizeof(lines[0]);
    size_t off = 0;

    for (int r = 0; r < rows + 4; ++r) {
        const char *line = lines[(size_t)r % line_cnt];
        int n = snprintf(buf + off, cap - off, "%s\r\n", line);
        if (n <= 0 || (size_t)n >= cap - off) break;
        off += (size_t)n;
    }

    return off;
}

/*
    Fills the grid with sequential, narrow, distinct runes (Private Use Areas),
    none of which exist in Consolas. Stresses the rune memo capacity.
*/
static size_t _bench_build_dense(char *buf, size_t cap, int cols, int rows) {
    size_t off = 0;
    uint32_t rune = 0xE000;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols - 1; ++c) {
            uint8_t tmp[4];
            int n = 0;
            uint32_t cp = rune++;

            if (cp < 0x800) {
                tmp[n++] = (uint8_t)(0xC0 | (cp >> 6));
                tmp[n++] = (uint8_t)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                tmp[n++] = (uint8_t)(0xE0 | (cp >> 12));
                tmp[n++] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
                tmp[n++] = (uint8_t)(0x80 | (cp & 0x3F));
            } else {
                tmp[n++] = (uint8_t)(0xF0 | (cp >> 18));
                tmp[n++] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
                tmp[n++] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
                tmp[n++] = (uint8_t)(0x80 | (cp & 0x3F));
            }

            if (off + (size_t)n + 2 >= cap) return off;
            memcpy(buf + off, tmp, (size_t)n);
            off += (size_t)n;

            if (rune > 0xF8FF && rune < 0xF0000) rune = 0xF0000;  // skip to plane 15 PUA
            if (rune > 0xFFFFD) rune = 0xE000;
        }
        buf[off++] = '\r';
        buf[off++] = '\n';
    }

    return off;
}

int main(int argc, char **argv) {
    int frames = argc > 1 ? atoi(argv[1]) : 3000;
    int mode = argc > 2 ? atoi(argv[2]) : 0;
    if (frames <= 0) frames = 3000;

    sfte_ctx *ctx = sfte_init(NULL, NULL);
    if (!ctx) {
        printf("FAIL: sfte_init\n");
        return 1;
    }

    sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "C:/Windows/Fonts/consola.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD, "C:/Windows/Fonts/consolab.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_ITALIC, "C:/Windows/Fonts/consolai.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD_ITALIC, "C:/Windows/Fonts/consolaz.ttf");

    int32_t w = 1280, h = 720;
    sfte_resize(ctx, w, h);

    uint32_t *px = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    if (!px) {
        printf("FAIL: pixel buffer\n");
        return 1;
    }

    static char screen[1 << 18];
    size_t len;
    if (mode == 2)
        len = _bench_build_dense(screen, sizeof(screen), ctx->term.cols, ctx->term.rows);
    else
        len = _bench_build_screen(screen, sizeof(screen), ctx->term.rows);
    sfte_parse(ctx, (const uint8_t *)screen, len);

    sfte_damage_rect dmg;
    for (int i = 0; i < 5; ++i) sfte_render(ctx, px, w, h, &dmg);  // warmup / bake glyphs

    g_get_id_calls = 0;
    g_bake_calls = 0;

    double t0 = _bench_now_ms();
    for (int i = 0; i < frames; ++i) {
        if (mode == 1) {
            static const char line[] = "benchmark output line 0123456789 abcXYZ\r\n";
            sfte_parse(ctx, (const uint8_t *)line, sizeof(line) - 1);
        }
        sfte_render(ctx, px, w, h, &dmg);
    }
    double elapsed = _bench_now_ms() - t0;

#ifdef BENCH_STB
    const char *backend = "stb";
#else
    const char *backend = "dwrite";
#endif
    const char *mode_name = mode == 1 ? "output" : (mode == 2 ? "dense" : "idle");

    printf("backend=%s mode=%s grid=%dx%d (%d cells) frames=%d\n", backend, mode_name,
           ctx->term.cols, ctx->term.rows, ctx->term.cols * ctx->term.rows, frames);
    printf("  total=%.2f ms  avg=%.4f ms/frame\n", elapsed, elapsed / (double)frames);
    printf("  get_id=%llu (%.1f/frame)  bakes=%llu (%.3f/frame)\n",
           (unsigned long long)g_get_id_calls, (double)g_get_id_calls / (double)frames,
           (unsigned long long)g_bake_calls, (double)g_bake_calls / (double)frames);

    free(px);
    sfte_free(ctx);
    return 0;
}