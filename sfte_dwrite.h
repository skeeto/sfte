/*
    sfte_dwrite.h - DirectWrite font backend for sfte

    Rasterizes glyphs with the system text stack, honoring the ClearType
    configuration (antialiasing mode, gamma and enhanced contrast).

    Usage, inside config.c, before including sfte.h/sfte_dev.h:

        #define SFTE_FONT_CUSTOM_BACKEND
        #define SFTE_FONT_SUBPIXEL 1
        #include "sfte_dwrite.h"

    Configuration:
        SFTE_FONT_SUBPIXEL           1 enables 3-channel (ClearType) coverage in the atlas,
                                     0 produces grayscale coverage. Default: 0.
        SFTE_DWRITE_ANTIALIAS        0 = follow the system ClearType setting (default),
                                     1 = force ClearType, 2 = force grayscale.
                                     Values 1 and 2 only take effect with SFTE_FONT_SUBPIXEL 1.
        SFTE_DWRITE_RENDERING_MODE   0 = GDI classic (default, hinted),
                                     1 = GDI natural, 2 = natural, 3 = natural symmetric.
        SFTE_DWRITE_SWAP_RB          1 swaps the red/blue coverage channels.

    Requires Windows 10 1703+ (IDWriteFactory5) and linking against dwrite.
*/

#ifndef SFTE_DWRITE_H
#define SFTE_DWRITE_H

#include <windows.h>
#include <objbase.h>

#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <dwrite.h>
#include <dwrite_3.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef SFTE_DWRITE_ANTIALIAS
#define SFTE_DWRITE_ANTIALIAS 0
#endif  // SFTE_DWRITE_ANTIALIAS
#if SFTE_DWRITE_ANTIALIAS < 0 || SFTE_DWRITE_ANTIALIAS > 2
#error "SFTE_DWRITE_ANTIALIAS must be 0 (system), 1 (ClearType) or 2 (grayscale)."
#endif

#ifndef SFTE_DWRITE_RENDERING_MODE
#define SFTE_DWRITE_RENDERING_MODE 0
#endif  // SFTE_DWRITE_RENDERING_MODE
#if SFTE_DWRITE_RENDERING_MODE < 0 || SFTE_DWRITE_RENDERING_MODE > 3
#error "SFTE_DWRITE_RENDERING_MODE must be 0..3."
#endif

#ifndef SFTE_DWRITE_SWAP_RB
#define SFTE_DWRITE_SWAP_RB 0
#endif  // SFTE_DWRITE_SWAP_RB

#if SFTE_FONT_SUBPIXEL
#define _SFTE_DWRITE_ATLAS_BPP 3
#else
#define _SFTE_DWRITE_ATLAS_BPP 1
#endif  // SFTE_FONT_SUBPIXEL

struct sfte_ctx;

/*
    DirectWrite state for a single loaded font.
*/
typedef struct sfte_font_backend_info {
    IDWriteFontFace *face;
    IDWriteFontFile *file;

    uint16_t units_per_em;
    int32_t ascent;    // Design units, from the baseline up
    int32_t descent;   // Design units, from the baseline down (positive)
    int32_t line_gap;  // Design units
    uint8_t valid;
} sfte_font_backend_info;

// =================================================================================================
// >>dwrite globals
// =================================================================================================

static IDWriteFactory5 *_sfte_dwrite_factory = NULL;
static IDWriteInMemoryFontFileLoader *_sfte_dwrite_loader = NULL;
static uint8_t _sfte_dwrite_state = 0;  // 0=uninitialized, 1=ready, 2=failed

static inline int _sfte_dwrite_init_globals(void) {
    if (_sfte_dwrite_state == 1) return 1;
    if (_sfte_dwrite_state == 2) return 0;
    _sfte_dwrite_state = 2;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    static const GUID iid_factory5 = {
        0x958db99a, 0xbe2a, 0x4f09, {0xaf, 0x7d, 0x65, 0x18, 0x98, 0x03, 0xd1, 0xd3}};

    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &iid_factory5,
                                   (IUnknown **)&_sfte_dwrite_factory)) ||
        !_sfte_dwrite_factory)
        return 0;

    if (FAILED(IDWriteFactory5_CreateInMemoryFontFileLoader(_sfte_dwrite_factory,
                                                            &_sfte_dwrite_loader)) ||
        !_sfte_dwrite_loader)
        return 0;

    if (FAILED(IDWriteFactory_RegisterFontFileLoader(
            (IDWriteFactory *)_sfte_dwrite_factory, (IDWriteFontFileLoader *)_sfte_dwrite_loader)))
        return 0;

    _sfte_dwrite_state = 1;
    return 1;
}

// =================================================================================================
// >>dwrite helpers
// =================================================================================================

/*
    Computes the byte size of a TTF buffer from its table directory.
    Only needed for sfte_font_load_mem, where the size is not provided.
*/
static inline uint32_t _sfte_dwrite_ttf_size(const uint8_t *data) {
    if (!data) return 0;

    uint32_t version = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
                       ((uint32_t)data[2] << 8) | data[3];
    if (version != 0x00010000u && version != 0x74727565u /*true*/ &&
        version != 0x4F54544Fu /*OTTO*/)
        return 0;

    uint16_t num_tables = (uint16_t)(((uint16_t)data[4] << 8) | data[5]);
    if (num_tables == 0 || num_tables > 4096) return 0;

    uint32_t end = 12u + (uint32_t)num_tables * 16u;

    for (uint32_t i = 0; i < num_tables; ++i) {
        const uint8_t *rec = data + 12 + i * 16;
        uint32_t off = ((uint32_t)rec[8] << 24) | ((uint32_t)rec[9] << 16) |
                       ((uint32_t)rec[10] << 8) | rec[11];
        uint32_t len = ((uint32_t)rec[12] << 24) | ((uint32_t)rec[13] << 16) |
                       ((uint32_t)rec[14] << 8) | rec[15];

        if (off > 0x7FFFFFFFu || len > 0x7FFFFFFFu) return 0;

        uint32_t table_end = off + len;
        if (table_end > end) end = table_end;
    }

    return end;
}

/*
    Returns 1 when glyphs should be rasterized with ClearType subpixel coverage.
*/
static inline int _sfte_dwrite_cleartype_enabled(void) {
#if !SFTE_FONT_SUBPIXEL
    return 0;
#else
#if SFTE_DWRITE_ANTIALIAS == 1
    return 1;
#elif SFTE_DWRITE_ANTIALIAS == 2
    return 0;
#else
    BOOL smoothing = FALSE;
    if (!SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &smoothing, 0) || !smoothing) return 0;

    UINT smoothing_type = 0;
    SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothing_type, 0);
    return smoothing_type == FE_FONTSMOOTHINGCLEARTYPE;
#endif
#endif  // !SFTE_FONT_SUBPIXEL
}

static inline DWRITE_TEXTURE_TYPE _sfte_dwrite_texture_type(void) {
    return _sfte_dwrite_cleartype_enabled() ? DWRITE_TEXTURE_CLEARTYPE_3x1
                                            : DWRITE_TEXTURE_ALIASED_1x1;
}

static inline DWRITE_RENDERING_MODE1 _sfte_dwrite_rendering_mode(void) {
#if SFTE_DWRITE_RENDERING_MODE == 1
    return DWRITE_RENDERING_MODE1_GDI_NATURAL;
#elif SFTE_DWRITE_RENDERING_MODE == 2
    return DWRITE_RENDERING_MODE1_NATURAL;
#elif SFTE_DWRITE_RENDERING_MODE == 3
    return DWRITE_RENDERING_MODE1_NATURAL_SYMMETRIC;
#else
    return DWRITE_RENDERING_MODE1_GDI_CLASSIC;
#endif
}

/*
    Builds a single-glyph run analysis. `em_px` is the em size in pixels and
    `adv_px` the glyph advance in pixels; both are treated 1:1 by DirectWrite.
*/
static inline IDWriteGlyphRunAnalysis *_sfte_dwrite_make_analysis(
    sfte_font_backend_info *info, UINT16 glyph_id, float em_px, float adv_px) {
    if (!_sfte_dwrite_factory || !info->face) return NULL;

    DWRITE_GLYPH_RUN run = {0};
    run.fontFace = info->face;
    run.fontEmSize = em_px;
    run.glyphCount = 1;
    run.glyphIndices = &glyph_id;
    run.glyphAdvances = &adv_px;
    run.glyphOffsets = NULL;
    run.isSideways = FALSE;
    run.bidiLevel = 0;

    IDWriteGlyphRunAnalysis *analysis = NULL;
    HRESULT hr = IDWriteFactory5_CreateGlyphRunAnalysis(
        _sfte_dwrite_factory, &run, NULL, _sfte_dwrite_rendering_mode(),
        DWRITE_MEASURING_MODE_NATURAL, DWRITE_GRID_FIT_MODE_DEFAULT,
        _sfte_dwrite_cleartype_enabled() ? DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE
                                         : DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE,
        0.0f, 0.0f, &analysis);

    if (FAILED(hr)) return NULL;
    return analysis;
}

// =================================================================================================
// >>dwrite hooks
// =================================================================================================

static inline void _sfte_dwrite_init_sized(sfte_font_backend_info *info, const uint8_t *data,
                                           size_t size) {
    memset(info, 0, sizeof(*info));
    if (!data) return;
    if (!_sfte_dwrite_init_globals()) return;

    uint32_t file_size = (uint32_t)size;
    if (file_size == 0) file_size = _sfte_dwrite_ttf_size(data);
    if (file_size == 0) return;

    IDWriteFontFile *file = NULL;
    if (FAILED(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(
            _sfte_dwrite_loader, (IDWriteFactory *)_sfte_dwrite_factory, data, file_size, NULL,
            &file)) ||
        !file)
        return;

    IDWriteFontFace *face = NULL;
    if (FAILED(IDWriteFactory_CreateFontFace(
            (IDWriteFactory *)_sfte_dwrite_factory, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0,
            DWRITE_FONT_SIMULATIONS_NONE, &face)) ||
        !face) {
        IDWriteFontFile_Release(file);
        return;
    }

    DWRITE_FONT_METRICS metrics;
    IDWriteFontFace_GetMetrics(face, &metrics);

    info->face = face;
    info->file = file;
    info->units_per_em = metrics.designUnitsPerEm;
    info->ascent = (int32_t)metrics.ascent;
    info->descent = (int32_t)metrics.descent;
    info->line_gap = (int32_t)metrics.lineGap;
    info->valid = 1;
}

static inline void _sfte_dwrite_init(sfte_font_backend_info *info, const uint8_t *data) {
    _sfte_dwrite_init_sized(info, data, 0);
}

static inline void _sfte_dwrite_deinit(sfte_font_backend_info *info) {
    if (info->face) {
        IDWriteFontFace_Release(info->face);
        info->face = NULL;
    }
    if (info->file) {
        IDWriteFontFile_Release(info->file);
        info->file = NULL;
    }
    info->valid = 0;
}

static inline float _sfte_dwrite_get_scale(sfte_font_backend_info *info, float px_hei) {
    if (!info->valid) return 0.0f;

    int32_t span = info->ascent + info->descent;
    if (span <= 0) return 0.0f;

    return px_hei / (float)span;
}

static inline void _sfte_dwrite_vmetrics(sfte_font_backend_info *info, int *ascent, int *descent,
                                         int *linegap) {
    *ascent = info->valid ? (int)info->ascent : 0;
    *descent = info->valid ? -(int)info->descent : 0;
    *linegap = info->valid ? (int)info->line_gap : 0;
}

static inline int _sfte_dwrite_get_id(sfte_font_backend_info *info, uint32_t rune) {
    if (!info->valid) return 0;

    UINT32 codepoint = rune;
    UINT16 glyph_id = 0;
    if (FAILED(IDWriteFontFace_GetGlyphIndices(info->face, &codepoint, 1, &glyph_id))) return 0;

    return (int)glyph_id;
}

static inline void _sfte_dwrite_bounds(sfte_font_backend_info *info, int glyph_id, float scale,
                                       int *adv, int *x0, int *y0, int *x1, int *y1) {
    *adv = 0;
    *x0 = 0;
    *y0 = 0;
    *x1 = 0;
    *y1 = 0;
    if (!info->valid || glyph_id <= 0) return;

    UINT16 gid = (UINT16)glyph_id;
    DWRITE_GLYPH_METRICS metrics;
    if (SUCCEEDED(IDWriteFontFace_GetDesignGlyphMetrics(info->face, &gid, 1, &metrics, FALSE)))
        *adv = (int)metrics.advanceWidth;

    float em_px = scale * (float)info->units_per_em;
    float adv_px = (float)*adv * scale;

    IDWriteGlyphRunAnalysis *analysis = _sfte_dwrite_make_analysis(info, gid, em_px, adv_px);
    if (!analysis) return;

    RECT bounds;
    if (SUCCEEDED(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(
            analysis, _sfte_dwrite_texture_type(), &bounds))) {
        *x0 = (int)bounds.left;
        *y0 = (int)bounds.top;
        *x1 = (int)bounds.right;
        *y1 = (int)bounds.bottom;
    }

    IDWriteGlyphRunAnalysis_Release(analysis);
}

static inline void _sfte_dwrite_bake(struct sfte_ctx *ctx, sfte_font_backend_info *info,
                                     int glyph_id, float scale, uint8_t *atlas_ptr, int gw, int gh,
                                     int atlas_stride) {
    (void)ctx;
    if (!info->valid || glyph_id <= 0 || gw <= 0 || gh <= 0) return;

    UINT16 gid = (UINT16)glyph_id;
    DWRITE_GLYPH_METRICS metrics;
    float adv_design = 0.0f;
    if (SUCCEEDED(IDWriteFontFace_GetDesignGlyphMetrics(info->face, &gid, 1, &metrics, FALSE)))
        adv_design = (float)metrics.advanceWidth;

    float em_px = scale * (float)info->units_per_em;
    float adv_px = adv_design * scale;

    IDWriteGlyphRunAnalysis *analysis = _sfte_dwrite_make_analysis(info, gid, em_px, adv_px);
    if (!analysis) return;

    DWRITE_TEXTURE_TYPE texture_type = _sfte_dwrite_texture_type();
    int bpp = texture_type == DWRITE_TEXTURE_CLEARTYPE_3x1 ? 3 : 1;

    RECT bounds;
    if (FAILED(IDWriteGlyphRunAnalysis_GetAlphaTextureBounds(analysis, texture_type, &bounds))) {
        IDWriteGlyphRunAnalysis_Release(analysis);
        return;
    }

    int tex_w = (int)(bounds.right - bounds.left);
    int tex_h = (int)(bounds.bottom - bounds.top);
    if (tex_w <= 0 || tex_h <= 0) {
        IDWriteGlyphRunAnalysis_Release(analysis);
        return;
    }

    uint32_t buf_size = (uint32_t)tex_w * (uint32_t)tex_h * (uint32_t)bpp;
    uint8_t *tmp = (uint8_t *)malloc(buf_size);
    if (!tmp) {
        IDWriteGlyphRunAnalysis_Release(analysis);
        return;
    }

    if (SUCCEEDED(IDWriteGlyphRunAnalysis_CreateAlphaTexture(analysis, texture_type, &bounds, tmp,
                                                             buf_size))) {
        int copy_w = tex_w < gw ? tex_w : gw;
        int copy_h = tex_h < gh ? tex_h : gh;

        for (int y = 0; y < copy_h; ++y) {
            const uint8_t *src = tmp + (size_t)y * (size_t)tex_w * (size_t)bpp;
            uint8_t *dst = atlas_ptr + (size_t)y * (size_t)atlas_stride * _SFTE_DWRITE_ATLAS_BPP;

            for (int x = 0; x < copy_w; ++x) {
#if SFTE_FONT_SUBPIXEL
                if (bpp == 3) {
#if SFTE_DWRITE_SWAP_RB
                    dst[x * 3 + 0] = src[x * 3 + 2];
                    dst[x * 3 + 1] = src[x * 3 + 1];
                    dst[x * 3 + 2] = src[x * 3 + 0];
#else
                    dst[x * 3 + 0] = src[x * 3 + 0];
                    dst[x * 3 + 1] = src[x * 3 + 1];
                    dst[x * 3 + 2] = src[x * 3 + 2];
#endif  // SFTE_DWRITE_SWAP_RB
                } else {
                    uint8_t v = src[x];
                    dst[x * 3 + 0] = v;
                    dst[x * 3 + 1] = v;
                    dst[x * 3 + 2] = v;
                }
#else   // !SFTE_FONT_SUBPIXEL
                if (bpp == 3)
                    dst[x] = (uint8_t)(((uint32_t)src[x * 3 + 0] + (uint32_t)src[x * 3 + 1] +
                                        (uint32_t)src[x * 3 + 2]) /
                                       3u);
                else
                    dst[x] = src[x];
#endif  // SFTE_FONT_SUBPIXEL
            }
        }
    }

    free(tmp);
    IDWriteGlyphRunAnalysis_Release(analysis);
}

// =================================================================================================
// >>dwrite macro hooks
// =================================================================================================

#define SFTE_FONT_INIT _sfte_dwrite_init
#define SFTE_FONT_INIT_SIZED _sfte_dwrite_init_sized
#define SFTE_FONT_DEINIT _sfte_dwrite_deinit
#define SFTE_FONT_GET_SCALE _sfte_dwrite_get_scale
#define SFTE_FONT_VMETRICS _sfte_dwrite_vmetrics
#define SFTE_FONT_BOUNDS _sfte_dwrite_bounds
#define SFTE_FONT_BAKE _sfte_dwrite_bake
#define SFTE_FONT_GET_ID _sfte_dwrite_get_id

#endif  // SFTE_DWRITE_H