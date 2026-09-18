/*
    sfte -- single-file terminal emulator
    v1.00

    Project URL: https://github.com/nihiL7331/sfte

    Do this:
        #define SFTE_IMPL
    before you include this file in the `config.c` file.

    You can #define SFTE_ASSERT(x) before the #include to avoid using assert.h.
    And #define SFTE_MALLOC, SFTE_REALLOC, SFTE_CALLOC and SFTE_FREE
    to avoid using malloc, realloc, free.

    PURPOSE
    =======
    Primarily of interest to: Wayland users, since it's the only backend currently supported
                              Embedded tinkerers, due to it's possible low memory usage and
                                                  performance-altering customizability
                              Game engine developers, who need an in-game terminal that works.

    Both the font backend (by default, stb_truetype) and rendering/I/O backend (by default, Wayland)
    can be changed by #define SFTE_CUSTOM_BACKEND, #define SFTE_NO_POSIX
                                                               and #define SFTE_FONT_CUSTOM_BACKEND.


    Full customization macros documentation below, in the `>>>CONFIGURATION` section.
    *sfte* requires explicit dependency enabling.

    EYE CANDY
    =========

    If we're not working on a low-power machine, we can easily purpose 0.1% of our CPU for some
    eye candy. Over time, terminal emulators started implementing different eye-catching 'candies'.
    *sfte* has support for a cursor trail (similarly to kitty and neovide), it can be enabled using:

    #define SFTE_CURSOR_TRAIL 10


    Another, similar feature implemented are screen buffer transitions.
    It's a simple scroll animation that happens when terminal transitions from main to alt grid.
    That occurs when a TUI like nvim, yazi, btop gets opened.
    It can be seen in the README.md gif. This can be enabled by using:

    #define SFTE_TERM_ANIMATE_SCREEN 1

    NOTE:
    Due to how tmux works, even if enabled, this function doesn't work when using it.
    This *technically* could be 'fixed' by intercepting the SM/RM calls, but it's hacky and probably
    will never be apart of the *sfte* core.

    FONTS
    =====

    There's a lot of customization options regarding fonts.
    50% of API calls with a default setup are also font-related.

    To set them up properly, first consider what font types you want to use.
    If you want to use a bold font,        add #define SFTE_FONT_BOLD.
    If you want to use an italic font,     add #define SFTE_FONT_ITALIC.
    If you want to use a bold-italic font, add #define SFTE_FONT_BOLD_ITALIC.

    If a certain font type is enabled, at least one font of that type must be loaded, using either:

    void sfte_font_load_mem(sfte_ctx *ctx, sfte_font_style style, const uint8_t *ttf_data)

    or

    void sfte_font_load_file(sfte_ctx *ctx, sfte_font_style style, const char *path)

    They both achieve very different things (but both can be used at once).
    `sfte_font_load_mem` expects the font data as a pointer to a const array.
    This means, that the data lives inside the binary, bloating the binary size significantly
    in exchange for having the font 'baked' into the binary.
    That means, that the terminal becomes a single unit, independent from system fonts.
    To get a TTF font in form of a C buffer, you can use xxd:

    xxd -i /path/to/my/font.ttf > font_data.h

    Then, you just include the font data header inside `config.c`, and pass the array from inside
    that file into the `sfte_font_load_mem` function:

    sfte_font_load_mem(ctx, SFTE_FONT_STYLE_REGULAR, _path_to_my_font_ttf);

    `sfte_font_load_file`, on the other hand loads the font from a provided file path on startup.
    This means that the font data is saved to RAM, not changing the binary size.
    However, it requires the font to live in provided directory DURING THE STARTUP.
    It's a less portable, but more lightweight solution.
    It can be used like so:

    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD, "/path/to/my/font_bold.ttf");


    *sfte* supports fallback fonts. That's especially useful for cases where we have a primary font
    and a nerd symbols font. They can be loaded using two sfte_font_load_file/mem calls:

    sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "/path/to/my/main_font.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "/path/to/my/nerd_symbols_font.ttf");

    Because the nerd symbols font is loaded second, it's a 'second priority' font.


    Often times, the nerd symbols are too big, causing weird visual overlaps.
    To scale down the nerd symbols font, you can use:

    #define SFTE_FONT_SCALES {1.0f, 0.8f}

    SHORTCUTS
    =========

    It is possible to create own shortcuts.
    To create a shortcut, first create its callback following this definition:

    void my_shortcut(sfte_ctx *ctx, const sfte_arg *arg) {
        // my shortcut logic... e.g.,
        (void)arg;
        printf("Current cursor position: %d x %d\n", ctx->term.cursor_col, ctx->term.cursor_row);
    }

    Inside this function write the wanted logic that should happen when a shortcut is toggled.
    `sfte_arg` is a type that's a union:

    typedef union {
        int i;
        float f;
        const void *v;
    } sfte_arg;

    Then, add it to your SFTE_SHORTCUTS macro override:

    #define SFTE_SHORTCUTS {SFTE_BASE_SHORTCUTS,                                                   \
        {SFTE_MOD_CTRL | SFTE_MOD_ALT, XKB_KEY_C, my_shortcut, {.v = NULL}}}

    NOTE:
    Remember to use SFTE_BASE_SHORTCUTS if you want to keep default shortcuts.



    TODO:
    Sections explaining: custom backend support, procedural boxes, cursor macros, images, input,
    clipboard, minimal setup, SFTE_COLOR_DRAW/BLEND_PIXEL overrides, ligatures.

    LICENSE
    =======
    zlib/libpng license

    Copyright (c) 2026 Patryk Pujanek

    This software is provided 'as-is', without any express or implied warranty.
    In no event will the authors be held liable for any damages arising from the
    use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

        1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software in a
        product, an acknowledgment in the product documentation would be
        appreciated but is not required.

        2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.

        3. This notice may not be removed or altered from any source
        distribution.
*/

#ifndef SFTE_FONT_CUSTOM_BACKEND
#include "vendor/stb_truetype.h"
typedef stbtt_fontinfo sfte_font_backend_info;
#else
typedef struct sfte_font_backend_info sfte_font_backend_info;
#endif  // SFTE_FONT_CUSTOM_BACKEND

// default value for SFTE_IMG_KITTY is 1, so if it's undefined its 1
#if !defined(SFTE_IMG_KITTY) || SFTE_IMG_KITTY
#include "vendor/stb_image.h"
#endif  // !defined(SFTE_IMG_KITTY) || SFTE_IMG_KITTY

#include <float.h>   // FLT_MAX
#include <stddef.h>  // size_t
#include <stdint.h>

#ifndef SFTE_CUSTOM_BACKEND
#ifndef SFTE_WAYLAND
#ifdef _WIN32
#define SFTE_WAYLAND 0
#else
#define SFTE_WAYLAND 1
#endif  // _WIN32
#endif  // SFTE_WAYLAND
#ifndef SFTE_WIN32
#if defined(_WIN32) && !SFTE_WAYLAND
#define SFTE_WIN32 1
#else
#define SFTE_WIN32 0
#endif  // defined(_WIN32) && !SFTE_WAYLAND
#endif  // SFTE_WIN32
#else   // SFTE_CUSTOM_BACKEND
#define SFTE_WAYLAND 0
#define SFTE_WIN32 0
#endif  // SFTE_CUSTOM_BACKEND

#if SFTE_WAYLAND && SFTE_WIN32
#error "SFTE_WAYLAND and SFTE_WIN32 cannot be enabled at the same time."
#endif  // SFTE_WAYLAND && SFTE_WIN32

#if SFTE_WAYLAND
#define SFTE_XKB_COMMON
#endif  // SFTE_WAYLAND

#if SFTE_WIN32 && !defined(SFTE_NO_POSIX)
#define SFTE_NO_POSIX
#endif  // SFTE_WIN32 && !defined(SFTE_NO_POSIX)

#ifndef SFTE_NO_POSIX
#include <errno.h>
#include <unistd.h>
#endif  // !SFTE_NO_POSIX

#ifdef SFTE_XKB_COMMON
#include <xkbcommon/xkbcommon-keysyms.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon.h>
#endif  // SFTE_XKB_COMMON

// #################################################################################################
// >>>CONFIGURATION
// #################################################################################################

// =================================================================================================
// >>system/memory macros
// =================================================================================================

typedef struct sfte_ctx sfte_ctx;

/*
    Memory allocation macro hooks.
*/
#ifndef SFTE_MALLOC
#include <stdlib.h>
#define SFTE_MALLOC(sz) malloc(sz)
#define SFTE_REALLOC(p, sz) realloc(p, sz)
#define SFTE_CALLOC(n, sz) calloc(n, sz)
#define SFTE_FREE(p) free(p)
#endif  // !SFTE_MALLOC

#ifndef SFTE_ASSERT
#include <assert.h>
#define SFTE_ASSERT(c, m) assert((c) && (m))
#define SFTE_STATIC_ASSERT(c, m) _Static_assert(c, m)
#endif  // !SFTE_ASSERT

/*
    Internal helper macro used to ensure the value set
    for a macro is within predefined bounds [min;max].
*/
#define _SFTE_ENSURE_RANGE(val, min, max)                                                          \
    SFTE_STATIC_ASSERT((val) >= (min) && (val) <= (max),                                           \
                       #val " must be strictly between " #min " and " #max)

/*
    Internal helper macro used to ensure the dependencies
    for a macro value were met.
    Usage:

    _SFTE_ENSURE_DEPS(dependent_macro, dependency1 && dependency2);
*/
#define _SFTE_ENSURE_DEPS(macro, dep)                                                              \
    SFTE_STATIC_ASSERT(!(macro) || (dep), #macro " unmet dependencies: " #dep)

typedef enum {
    SFTE_LOG_LVL_PANIC,
    SFTE_LOG_LVL_ERROR,
    SFTE_LOG_LVL_WARN,
    SFTE_LOG_LVL_INFO,
} sfte_log_level;

/*
    Available levels of severity:
    SFTE_LOG_LVL_PANIC
    SFTE_LOG_LVL_ERROR (default)
    SFTE_LOG_LVL_WARN
    SFTE_LOG_LVL_INFO
*/
#ifndef SFTE_LOG_LEVEL
#define SFTE_LOG_LEVEL SFTE_LOG_LVL_ERROR
#endif  // SFTE_LOG_LEVEL
_SFTE_ENSURE_RANGE(SFTE_LOG_LEVEL, SFTE_LOG_LVL_PANIC, SFTE_LOG_LVL_INFO);

/*
    String prefixed to every log output.
*/
#ifndef SFTE_LOG_TAG
#define SFTE_LOG_TAG "sfte"
#endif  // SFTE_LOG_TAG

/*
    Macro hook for logging callback.
    Must match the following definition:

    void log_func(const char *tag, sfte_log_level log_level, const char *msg, uint32_t line_nr);

*/
#ifndef SFTE_LOG_FUNC
#define SFTE_LOG_FUNC _sfte_log_default_func /* fuzz skip */
#endif                                       // SFTE_LOG_FUNC

/*
    Maximum size of the global scratch stack in bytes.
    Used for temporary allocations during OSC parsing, Sixel and Kitty image processing.

    WARN:
    Setting this value too low might cause unexpected issues.
*/
#ifndef SFTE_MEM_STACK_SIZE
#define SFTE_MEM_STACK_SIZE (1024 * 1024 * 4)
#endif  // SFTE_MEM_STACK_SIZE
_SFTE_ENSURE_RANGE(SFTE_MEM_STACK_SIZE, 1024, INT32_MAX);

// =================================================================================================
// >>term macros
// =================================================================================================

/*
    The $TERM environment variable exposed to the shell.
*/
#ifndef SFTE_TERM_ENV
#define SFTE_TERM_ENV "xterm-256color"
#endif  // SFTE_TERM_ENV

/*
    Enables focus tracking (DECSET 1004) and hollow cursor rendering.
*/
#ifndef SFTE_TERM_FOCUS
#define SFTE_TERM_FOCUS 1
#endif  // SFTE_TERM_FOCUS
_SFTE_ENSURE_RANGE(SFTE_TERM_FOCUS, 0, 1);

/*
    Initial terminal grid column size.
*/
#ifndef SFTE_TERM_INIT_COLS
#define SFTE_TERM_INIT_COLS 80
#endif  // SFTE_TERM_INIT_COLS
_SFTE_ENSURE_RANGE(SFTE_TERM_INIT_COLS, 1, INT16_MAX);

/*
    Initial terminal grid row size.
*/
#ifndef SFTE_TERM_INIT_ROWS
#define SFTE_TERM_INIT_ROWS 24
#endif  // SFTE_TERM_INIT_ROWS
_SFTE_ENSURE_RANGE(SFTE_TERM_INIT_ROWS, 1, INT16_MAX);

/*
    Number of bytes read from the PTY per poll event.
*/
#ifndef SFTE_TERM_PTY_BUF_SIZE
#define SFTE_TERM_PTY_BUF_SIZE 4096
#endif  // SFTE_TERM_PTY_BUF_SIZE
_SFTE_ENSURE_RANGE(SFTE_TERM_PTY_BUF_SIZE, 1, UINT16_MAX);

/*
    Tab stop interval in grid cells.
*/
#ifndef SFTE_TERM_TAB_WIDTH
#define SFTE_TERM_TAB_WIDTH 8
#endif  // SFTE_TERM_TAB_WIDTH
_SFTE_ENSURE_RANGE(SFTE_TERM_TAB_WIDTH, 1, INT16_MAX);

/*
    Enables procedural rendering for box drawing characters (U+2500 - U+257F).
    Guarantees seamless, gapless lines for TUIs.
*/
#ifndef SFTE_TERM_CUSTOM_BOXES
#define SFTE_TERM_CUSTOM_BOXES 1
#endif  // SFTE_TERM_CUSTOM_BOXES
_SFTE_ENSURE_RANGE(SFTE_TERM_CUSTOM_BOXES, 0, 1);

/*
    Enables standard terminal alternate screen buffer (used by TUIs extensively).
    Can be disabled to halve the memory usage for logical grid,
    but CAN and WILL break TUIs rendering.
*/
#ifndef SFTE_TERM_ALT_SCREEN
#define SFTE_TERM_ALT_SCREEN 1
#endif  // SFTE_TERM_ALT_SCREEN
_SFTE_ENSURE_RANGE(SFTE_TERM_ALT_SCREEN, 0, 1);

/*
    Defines frame rate during rendering (in frames per second).
    Used for screen animations, smooth scrolling, cursor trail.
*/
#ifndef SFTE_TERM_REFRESH_RATE
#define SFTE_TERM_REFRESH_RATE 60
#endif  // SFTE_TERM_REFRESH_RATE
_SFTE_ENSURE_RANGE(SFTE_TERM_REFRESH_RATE, 1, 720);

/*
    Maintains a secondary pixel buffer to prevent tearing.
    Can be disabled without any noticeable changes on minimal setups,
    lowering the memory usage (and requiring one less memcpy in hot path).
    NOTE:
    Needs to be enabled for cursor trails and screen buffer transitions.
*/
#ifndef SFTE_TERM_DOUBLE_BUFFER
#define SFTE_TERM_DOUBLE_BUFFER 1
#endif  // SFTE_TERM_DOUBLE_BUFFER
_SFTE_ENSURE_RANGE(SFTE_TERM_DOUBLE_BUFFER, 0, 1);

/*
    Enables scrolling animation on TUI open/close (alt screen toggle).
    Obviously requires `SFTE_TERM_ALT_SCREEN` to be enabled to work.

    NOTE:
    This is an experimental feature.
    It makes the animation toggle on EVERY alt screen switch.
    It's purely sugar candy, might slightly affect the CPU usage.
    It also doesn't work if inside tmux.
*/
#ifndef SFTE_TERM_ANIMATE_SCREEN
#define SFTE_TERM_ANIMATE_SCREEN 0
#endif  // SFTE_TERM_ANIMATE_SCREEN
_SFTE_ENSURE_RANGE(SFTE_TERM_ANIMATE_SCREEN, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_TERM_ANIMATE_SCREEN, SFTE_TERM_ALT_SCREEN &&SFTE_TERM_DOUBLE_BUFFER);

/*
    Duration of the scrolling animation on TUI open/close (alt screen toggle).
*/
#ifndef SFTE_TERM_ANIM_DUR_MS
#define SFTE_TERM_ANIM_DUR_MS 250.0f
#endif  // SFTE_TERM_ANIM_DUR_MS
_SFTE_ENSURE_RANGE(SFTE_TERM_ANIM_DUR_MS, 0.0f, FLT_MAX);

/*
    Enables text reflow when resizing the terminal window.
    With SFTE_TERM_REFLOW disabled, any text going off the right edge is deleted immediately.
*/
#ifndef SFTE_TERM_REFLOW
#define SFTE_TERM_REFLOW 1
#endif  // SFTE_TERM_REFLOW
_SFTE_ENSURE_RANGE(SFTE_TERM_REFLOW, 0, 1);

/*
    Maximum lines of scrollback history kept in memory.
    Increasing it WILL affect the memory footprint size.
    Setting it to 0 disables scrollback logic entirely.
*/
#ifndef SFTE_TERM_SCROLLBACK_CAP
#define SFTE_TERM_SCROLLBACK_CAP 2000
#endif  // SFTE_TERM_SCROLLBACK_CAP
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLLBACK_CAP, 0, INT32_MAX);

/*
    Determines if `clear` commands (CSI 3 J) actually wipe the scrollback buffer.
    By default, the scrollback DOES get wiped on `clear` call.
*/
#ifndef SFTE_TERM_SCROLLBACK_CLEAR
#define SFTE_TERM_SCROLLBACK_CLEAR 1
#endif  // SFTE_TERM_SCROLLBACK_CLEAR
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLLBACK_CLEAR, 0, 1);

/*
    Number of lines to shift per mouse wheel / trackpad scroll tick.
    NOTE:
    This doesn't affect the Shift+PgUp/Dn scrolling speed.
    It can be changed in the default shortcuts, lower in the file.
*/
#ifndef SFTE_TERM_SCROLL_STEP
#define SFTE_TERM_SCROLL_STEP 3
#endif  // SFTE_TERM_SCROLL_STEP
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLL_STEP, 0, INT32_MAX);

/*
    Enables smooth text scrolling.
    Essentially enables linear interpolation to closest row, replacing instant snapping.
    Because it requires polling while scrolling, it will affect CPU usage while scrolling.
*/
#ifndef SFTE_TERM_SCROLL_SMOOTH
#define SFTE_TERM_SCROLL_SMOOTH 1
#endif  // SFTE_TERM_SCROLL_SMOOTH
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLL_SMOOTH, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_TERM_SCROLL_SMOOTH, SFTE_TERM_SCROLLBACK_CAP);

/*
    Affects how fast the scroll smooths to its target position.
*/
#ifndef SFTE_TERM_SCROLL_DECAY
#define SFTE_TERM_SCROLL_DECAY 0.05f
#endif  // SFTE_TERM_SCROLL_DECAY
_SFTE_ENSURE_RANGE(SFTE_TERM_SCROLL_DECAY, 0.0f, FLT_MAX);

/*
    Narrows the allowed characters range to ASCII, lowering the memory usage.
    Enabling this allows the terminal to store each rune as a `uint8_t` rather than a `uint32_t`.
*/
#ifndef SFTE_TERM_ASCII_CHARSET
#define SFTE_TERM_ASCII_CHARSET 0
#endif  // SFTE_TERM_ASCII_CHARSET
_SFTE_ENSURE_RANGE(SFTE_TERM_ASCII_CHARSET, 0, 1);

// =================================================================================================
// >>window macros
// =================================================================================================

/*
    Horizontal padding around the terminal grid in pixels.
*/
#ifndef SFTE_WINDOW_PAD_X
#define SFTE_WINDOW_PAD_X 8
#endif  // SFTE_WINDOW_PAD_X
_SFTE_ENSURE_RANGE(SFTE_WINDOW_PAD_X, 0, INT32_MAX);

/*
    Vertical padding around the terminal grid in pixels.
*/
#ifndef SFTE_WINDOW_PAD_Y
#define SFTE_WINDOW_PAD_Y 8
#endif  // SFTE_WINDOW_PAD_Y
_SFTE_ENSURE_RANGE(SFTE_WINDOW_PAD_Y, 0, INT32_MAX);

// =================================================================================================
// >>color macros
// =================================================================================================

/*
    Enables parsing of 24-bit TrueColor sequences (CSI 38;2;R;G;B m).
*/
#ifndef SFTE_COLOR_TRUECOLOR
#define SFTE_COLOR_TRUECOLOR 1
#endif  // SFTE_COLOR_TRUECOLOR
_SFTE_ENSURE_RANGE(SFTE_COLOR_TRUECOLOR, 0, 1);

/*
    Default background color (RGB888).
*/
#ifndef SFTE_COLOR_BG
#define SFTE_COLOR_BG 0x000000
#endif  // SFTE_COLOR_BG
_SFTE_ENSURE_RANGE(SFTE_COLOR_BG, 0x000000, 0xFFFFFF);

/*
    Default text foreground color (text and underlines, if applicable).
    Format RGB888.
*/
#ifndef SFTE_COLOR_FG
#define SFTE_COLOR_FG 0xFFFFFF
#endif  // SFTE_COLOR_FG
_SFTE_ENSURE_RANGE(SFTE_COLOR_FG, 0x000000, 0xFFFFFF);

/*
    Background opacity (0x00 transparent to 0xFF opaque).
*/
#ifndef SFTE_COLOR_BG_OPACITY
#define SFTE_COLOR_BG_OPACITY 0xFF
#endif  // SFTE_COLOR_BG_OPACITY
_SFTE_ENSURE_RANGE(SFTE_COLOR_BG_OPACITY, 0x00, 0xFF);

/*
    16-color ANSI fallback palette.
    Colors are, in order: black, red, green, yellow, blue, magenta, cyan, white.
    Indices 0 to 7 are regular colors.
    Indices 8 to 15 are bright colors.

    WARN:
    Do NOT wrap the colors in curly braces.
    These values act as the 0-15 index colors in a 256 color palette.
*/
#ifndef SFTE_COLOR_ANSI_PALETTE
#define SFTE_COLOR_ANSI_PALETTE                                                                    \
    0x181818, 0xCC241D, 0x98971A, 0xD79921, 0x458588, 0xB16286, 0x689D6A, 0xA89984, /* regular */  \
        0x928374, 0xFB4934, 0xB8BB26, 0xFABD2F, 0x83A598, 0xD3869B, 0x8EC07C,                      \
        0xEBDBB2 /* bright  */
#endif           // SFTE_COLOR_ANSI_PALETTE

/*
    A macro helper used to draw pixels.
    This can be overriden to support different color formats,
    e.g.: monochrome, RGB233, RGB565.
*/
#ifndef SFTE_COLOR_DRAW_PIXEL
#define SFTE_COLOR_DRAW_PIXEL(buf, x, y, stride, height, color)                                    \
    if ((x) >= 0 && (x) < (stride) && (y) >= 0 && (y) < (height))                                  \
        ((uint32_t *)(buf))[(y) * (stride) + (x)] = (color);
#endif  // SFTE_COLOR_DRAW_PIXEL

/*
    A macro helper used to blend pixel colors based on their alpha value.
    This can be overriden to support different color formats,
    e.g.: monochrome, RGB233, RGB565.
    Needed for cursor trail and images support.
*/
#ifndef SFTE_COLOR_BLEND_PIXEL
#define SFTE_COLOR_BLEND_PIXEL(buf, x, y, stride, argb_color, alpha)                               \
    (((uint32_t *)(buf))[(y) * (stride) + (x)] = _sfte_render_blend_argb(                          \
         ((uint32_t *)(buf))[(y) * (stride) + (x)], (argb_color), (alpha)))
#endif  // SFTE_COLOR_BLEND_PIXEL

#define SFTE_COLOR_ALPHA_MASK 0xFF000000

// =================================================================================================
// >>font macros
// =================================================================================================

#ifndef SFTE_FONT_CUSTOM_BACKEND
static inline void _sfte_stb_init(sfte_font_backend_info *info, const uint8_t *data);
static inline float _sfte_stb_get_scale(sfte_font_backend_info *info, float px_hei);
static inline void _sfte_stb_vmetrics(sfte_font_backend_info *info, int *ascent, int *descent,
                                      int *linegap);
static inline void _sfte_stb_bounds(sfte_font_backend_info *info, int glyph_id, float scale,
                                    int *adv, int *x0, int *y0, int *x1, int *y1);
static inline void _sfte_stb_bake(sfte_ctx *ctx, sfte_font_backend_info *info, int glyph_idx,
                                  float scale, uint8_t *atlas_ptr, int gw, int gh,
                                  int atlas_stride);
static inline int _sfte_stb_get_id(sfte_font_backend_info *info, uint32_t rune);
#else  // SFTE_FONT_CUSTOM_BACKEND
#if !defined(SFTE_FONT_INIT) || !defined(SFTE_FONT_GET_SCALE) || !defined(SFTE_FONT_VMETRICS) ||   \
    !defined(SFTE_FONT_BOUNDS) || !defined(SFTE_FONT_BAKE) || !defined(SFTE_FONT_GET_ID)
#error                                                                                             \
    "SFTE_FONT_CUSTOM_BACKEND requires defining all 6 macro hooks: INIT, GET_SCALE, VMETRICS, BOUNDS, BAKE and GET_ID."
#endif  // !defined(SFTE_FONT_INIT) || !defined(SFTE_FONT_GET_SCALE) || !defined(SFTE_FONT_VMETRICS)
        // || !defined(SFTE_FONT_BOUNDS) || !defined(SFTE_FONT_BAKE) || !defined(SFTE_FONT_GET_ID)
#endif  // SFTE_FONT_CUSTOM_BACKEND

/*
    Minimum size of the font in pixels.
*/
#ifndef SFTE_FONT_MIN_SIZE
#define SFTE_FONT_MIN_SIZE 1.0f
#endif  // SFTE_FONT_MIN_SIZE
_SFTE_ENSURE_RANGE(SFTE_FONT_MIN_SIZE, 0.1f, FLT_MAX);

/*
    Maximum size of the font in pixels.
*/
#ifndef SFTE_FONT_MAX_SIZE
#define SFTE_FONT_MAX_SIZE 96.0f
#endif  // SFTE_FONT_MAX_SIZE
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_SIZE, 0.1f, FLT_MAX);

/*
    Starting font size in pixels.
*/
#ifndef SFTE_FONT_DEFAULT_SIZE
#define SFTE_FONT_DEFAULT_SIZE 12.0f
#endif  // SFTE_FONT_DEFAULT_SIZE
_SFTE_ENSURE_RANGE(SFTE_FONT_DEFAULT_SIZE, 0.1f, FLT_MAX);

/*
    Enables font ligatures support.
*/
#ifndef SFTE_FONT_LIGATURES
#define SFTE_FONT_LIGATURES 1
#endif  // SFTE_FONT_LIGATURES
_SFTE_ENSURE_RANGE(SFTE_FONT_LIGATURES, 0, 1);

/*
    Maximum amount of lookup indices for a ligature shaper feature.
*/
#ifndef SFTE_FONT_MAX_LIGATURE_LOOKUPS
#define SFTE_FONT_MAX_LIGATURE_LOOKUPS 256
#endif  // SFTE_FONT_MAX_LIGATURE_LOOKUPS
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_LIGATURE_LOOKUPS, 1, UINT16_MAX);

/*
    Maximum amount of subtables per lookup for ligatures.
*/
#ifndef SFTE_FONT_MAX_LIGATURE_SUBTABLES
#define SFTE_FONT_MAX_LIGATURE_SUBTABLES 16
#endif  // SFTE_FONT_MAX_LIGATURE_SUBTABLES
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_LIGATURE_SUBTABLES, 1, UINT16_MAX);

/*
    Maximum amount of records for ligatures.
*/
#ifndef SFTE_FONT_MAX_LIGATURE_RECORDS
#define SFTE_FONT_MAX_LIGATURE_RECORDS 8
#endif  // SFTE_FONT_MAX_LIGATURE_RECORDS
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_LIGATURE_RECORDS, 1, UINT16_MAX);

/*
    Max number of fallback fonts (primary + fallbacks).
*/
#ifndef SFTE_FONT_MAX_COUNT
#define SFTE_FONT_MAX_COUNT 4
#endif  // SFTE_FONT_MAX_COUNT
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_COUNT, 1, INT8_MAX);

/*
    Font oversample scale.
    If set to 1, no oversampling occurs.

    NOTE:
    To avoid jagged baseline issues, blurring of horizontal stems
    oversampling only applies horizontally, not vertically.
*/
#ifndef SFTE_FONT_OVERSAMPLE
#define SFTE_FONT_OVERSAMPLE 1
#endif  // SFTE_FONT_OVERSAMPLE
_SFTE_ENSURE_RANGE(SFTE_FONT_OVERSAMPLE, 1, 16);

/*
    Tweaks scaling per-font to match baseline heights.
    Useful for nerd symbol fonts, where often symbols are too big.
    With primary font in slot 0 and nerd font in slot 1, something like this can be used:
    #define SFTE_FONT_SCALES {1.0f, 0.8f}
*/
#ifndef SFTE_FONT_SCALES
#define SFTE_FONT_SCALES {1.0f, 1.0f, 1.0f, 1.0f}
#endif  // SFTE_FONT_SCALES

/*
    Enables Ctrl +/- font zooming at runtime.
*/
#ifndef SFTE_FONT_ZOOM
#define SFTE_FONT_ZOOM 1
#endif  // SFTE_FONT_ZOOM
_SFTE_ENSURE_RANGE(SFTE_FONT_ZOOM, 0, 1);

/*
    Enables double-width characters rendering (useful e.g. for Chinese symbols).
*/
#ifndef SFTE_FONT_WIDE_CHARS
#define SFTE_FONT_WIDE_CHARS 1
#endif  // SFTE_FONT_WIDE_CHARS
_SFTE_ENSURE_RANGE(SFTE_FONT_WIDE_CHARS, 0, 1);

/*
    Fixes clipping issues on symbols bigger than their cell by expanding the dirty render box.
    This is necessary for nerd symbols to render correctly,
    since very often they go out of their cell bounds.
    Disabling it removes a O(W^2 x H) loop from the hot path.
*/
#ifndef SFTE_FONT_BLEED
#define SFTE_FONT_BLEED 1
#endif  // SFTE_FONT_BLEED
_SFTE_ENSURE_RANGE(SFTE_FONT_BLEED, 0, 1);

/*
    Dimensions for the 2D texture atlas caching rendered glyphs.
    One atlas is shared across all font types and font fallbacks.
*/
#ifndef SFTE_FONT_ATLAS_SIZE
#define SFTE_FONT_ATLAS_SIZE 1024
#endif  // SFTE_FONT_ATLAS_SIZE
_SFTE_ENSURE_RANGE(SFTE_FONT_ATLAS_SIZE, 1, INT32_MAX);

/*
    Maximum number of distinct characters cached in memory at once.
    One glyph buffer is shared across all font types and font fallbacks.
*/
#ifndef SFTE_FONT_GLYPH_CAP
#define SFTE_FONT_GLYPH_CAP 4096
#endif  // SFTE_FONT_GLYPH_CAP
_SFTE_ENSURE_RANGE(SFTE_FONT_GLYPH_CAP, 1, UINT16_MAX);

/*
    Maximum amount of combining (width = 0) glyphs on one cell.
*/
#ifndef SFTE_FONT_MAX_COMBINING
#define SFTE_FONT_MAX_COMBINING 2
#endif  // SFTE_FONT_MAX_COMBINING
_SFTE_ENSURE_RANGE(SFTE_FONT_MAX_COMBINING, 0, 16);

// =================================================================================================
// >>cursor macros
// =================================================================================================

typedef enum sfte_cursor_style {
    SFTE_CURSOR_STYLE_BLOCK,
    SFTE_CURSOR_STYLE_UNDERLINE,
    SFTE_CURSOR_STYLE_BAR,
} sfte_cursor_style;

/*
    Sets the default terminal cursor style.
    Available options:
    SFTE_CURSOR_STYLE_BLOCK (default)
    SFTE_CURSOR_STYLE_UNDERLINE
    SFTE_CURSOR_STYLE_BAR

    NOTE:
    This value still can be overwritten by certain applications in runtime,
    #define SFTE_CURSOR_DYNAMIC 0
    can be used to keep the cursor at one, default style.
*/
#ifndef SFTE_CURSOR_STYLE
#define SFTE_CURSOR_STYLE SFTE_CURSOR_STYLE_BLOCK
#endif  // SFTE_CURSOR_STYLE
_SFTE_ENSURE_RANGE(SFTE_CURSOR_STYLE, SFTE_CURSOR_STYLE_BLOCK, SFTE_CURSOR_STYLE_BAR);

/*
    Allows programs to dynamically change the cursor shape via escape sequences.
    If set to 0, cursor style stays as SFTE_CURSOR_STYLE.
*/
#ifndef SFTE_CURSOR_DYNAMIC
#define SFTE_CURSOR_DYNAMIC 1
#endif  // SFTE_CURSOR_DYNAMIC
_SFTE_ENSURE_RANGE(SFTE_CURSOR_DYNAMIC, 0, 1);

/*
    Default cursor color (RGB888).
*/
#ifndef SFTE_CURSOR_COLOR
#define SFTE_CURSOR_COLOR 0xFFFFFF
#endif  // SFTE_CURSOR_COLOR
_SFTE_ENSURE_RANGE(SFTE_CURSOR_COLOR, 0x000000, 0xFFFFFF);

/*
    Determines the width of bar cursors and height of underline cursors relative to font size.
*/
#ifndef SFTE_CURSOR_THICK_RATIO
#define SFTE_CURSOR_THICK_RATIO 0.1f
#endif  // SFTE_CURSOR_THICK_RATIO
_SFTE_ENSURE_RANGE(SFTE_CURSOR_THICK_RATIO, 0.0f, 1.0f);

/*
    Enables cursor blinking.
    This technically affects CPU usage, since it requires polling on every state change.
*/
#ifndef SFTE_CURSOR_BLINK
#define SFTE_CURSOR_BLINK 1
#endif  // SFTE_CURSOR_BLINK
_SFTE_ENSURE_RANGE(SFTE_CURSOR_BLINK, 0, 1);

#ifndef SFTE_CURSOR_BLINK_RATE_MS
#define SFTE_CURSOR_BLINK_RATE_MS 500
#endif  // SFTE_CURSOR_BLINK_RATE_MS
_SFTE_ENSURE_RANGE(SFTE_CURSOR_BLINK_RATE_MS, 0, UINT32_MAX);

/*
    Renders an animated smooth-scrolling ghost trail behind the cursor,
    similar to cursor_trail from kitty or default (I believe?) neovide cursor trail.
    INFO:
    This is NOT a toggle, it defines time (in ms) of movement required for trail to start rendering.
    If it's set to 0, the value is disabled.
    Extremely low values may cause flickers when programs render themselves.
    10 seems like a sensible default for trail enabled.

    This affects CPU usage, since it requires polling every 16ms when the trail is visible.
    It requires double buffering, otherwise resulting in terrible rendering artifacts.
*/
#ifndef SFTE_CURSOR_TRAIL
#define SFTE_CURSOR_TRAIL 0
#endif  // SFTE_CURSOR_TRAIL
_SFTE_ENSURE_RANGE(SFTE_CURSOR_TRAIL, 0, 1024);
_SFTE_ENSURE_DEPS(SFTE_CURSOR_TRAIL, SFTE_TERM_DOUBLE_BUFFER);

/*
    Affects how fast the trail disappears.
*/
#ifndef SFTE_CURSOR_TRAIL_DECAY
#define SFTE_CURSOR_TRAIL_DECAY 0.01f
#endif  // SFTE_CURSOR_TRAIL_DECAY
_SFTE_ENSURE_RANGE(SFTE_CURSOR_TRAIL_DECAY, 0.01f, FLT_MAX);

// =================================================================================================
// >>underline macros
// =================================================================================================

/*
    Enables rendering of double, curly, dotted and dashed underlines (CSI 4:x m).
    Undercurls are rendered using a simple triangle wave to avoid CPU-heavy math calls.
*/
#ifndef SFTE_UNDERLINE_EXTENDED
#define SFTE_UNDERLINE_EXTENDED 1
#endif  // SFTE_UNDERLINE_EXTENDED
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_EXTENDED, 0, 1);

/*
    Enables underlines with custom true-colors distint from the text (CSI 58:2::R:G:B m).
    Default underline color is SFTE_COLOR_FG.
*/
#ifndef SFTE_UNDERLINE_COLORED
#define SFTE_UNDERLINE_COLORED 1
#endif  // SFTE_UNDERLINE_COLORED
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_COLORED, 0, 1);

/*
    Line thickness relative to font cell height.
*/
#ifndef SFTE_UNDERLINE_THICK_RATIO
#define SFTE_UNDERLINE_THICK_RATIO 0.1f
#endif  // SFTE_UNDERLINE_THICK_RATIO
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_THICK_RATIO, 0.0f, 1.0f);

/*
    Gap between text baseline and the underline relative to cell height.
*/
#ifndef SFTE_UNDERLINE_OFFSET_RATIO
#define SFTE_UNDERLINE_OFFSET_RATIO 0.12f
#endif  // SFTE_UNDERLINE_OFFSET_RATIO
_SFTE_ENSURE_RANGE(SFTE_UNDERLINE_OFFSET_RATIO, -1.0f, 1.0f);

// =================================================================================================
// >>img macros
// =================================================================================================

/*
    Enables DEC VT340 sixel bitmap graphics support.
*/
#ifndef SFTE_IMG_SIXEL
#define SFTE_IMG_SIXEL 1
#endif  // SFTE_IMG_SIXEL
_SFTE_ENSURE_RANGE(SFTE_IMG_SIXEL, 0, 1);

/*
    Enables kitty image protocol support.
*/
#ifndef SFTE_IMG_KITTY
#define SFTE_IMG_KITTY 1
#endif  // SFTE_IMG_KITTY
_SFTE_ENSURE_RANGE(SFTE_IMG_KITTY, 0, 1);

/*
    Maximum allocated dimension size for a temporary pixel buffer
    used while creating a sixel image from escape sequences.
    Biggest renderable Sixel image is SFTE_IMG_SIXEL_MAX_SIZE x SFTE_IMG_SIXEL_MAX_SIZE,
    assuming image pool size is sufficient.
*/
#ifndef SFTE_IMG_SIXEL_MAX_SIZE
#define SFTE_IMG_SIXEL_MAX_SIZE 4096
#endif  // SFTE_IMG_SIXEL_MAX_SIZE
_SFTE_ENSURE_RANGE(SFTE_IMG_SIXEL_MAX_SIZE, 64, INT32_MAX);

/*
    Smallest allocated dimension size for a temporary pixel buffer
    used while creating a sixel image from escape sequences.
    Initial allocation is SFTE_IMG_SIXEL_INIT_SIZE x SFTE_IMG_SIXEL_INIT_SIZE.
*/
#ifndef SFTE_IMG_SIXEL_INIT_SIZE
#define SFTE_IMG_SIXEL_INIT_SIZE 64
#endif  // SFTE_IMG_SIXEL_INIT_SIZE
_SFTE_ENSURE_RANGE(SFTE_IMG_SIXEL_INIT_SIZE, 1, SFTE_IMG_SIXEL_MAX_SIZE);

/*
    Maximum capacity of the base64 kitty encoding temporary buffer.
    This is required so that the terminal doesn't
    get bombed with a 100GB bugged/malware escape sequence.
    Default is 16MB, might not be enough for big 4K images.

    NOTE:
    Currently sfte doesn't run encoding on a worker thread,
    so opening big images might cause visible stutters.
*/
#ifndef SFTE_KITTY_B64_MAX_CAP
#define SFTE_KITTY_B64_MAX_CAP (16 * 1024 * 1024)
#endif  // SFTE_KITTY_B64_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_KITTY_B64_MAX_CAP, 128, UINT32_MAX);

/*
    Initial capacity of the base64 kitty encoding temporary buffer.
    Due to how kitty sequences are structured, it can grow (gets doubled on OOM)
    while reading the image data, since size is unknown during the read.
*/
#ifndef SFTE_KITTY_B64_INIT_CAP
#define SFTE_KITTY_B64_INIT_CAP 128
#endif  // SFTE_KITTY_B64_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_KITTY_B64_INIT_CAP, 1, SFTE_KITTY_B64_MAX_CAP);

/*
    Maximum capacity of the shared image reference pool.
    This defines how many DIFFERENT images can be rendered at once.
*/
#ifndef SFTE_IMG_POOL_MAX_CAP
#define SFTE_IMG_POOL_MAX_CAP 1024
#endif  // SFTE_IMG_POOL_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_IMG_POOL_MAX_CAP, 8, UINT32_MAX);

/*
    Initial capacity of the shared image reference pool.
*/
#ifndef SFTE_IMG_POOL_INIT_CAP
#define SFTE_IMG_POOL_INIT_CAP 8
#endif  // SFTE_IMG_POOL_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_IMG_POOL_INIT_CAP, 1, SFTE_IMG_POOL_MAX_CAP);

/*
    Maximum capacity of active viewport placements.
    This defines how many IDENTICAL images can be rendered at once.
*/
#ifndef SFTE_IMG_PLACEMENT_MAX_CAP
#define SFTE_IMG_PLACEMENT_MAX_CAP 4096
#endif  // SFTE_IMG_PLACEMENT_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_IMG_PLACEMENT_MAX_CAP, 16, UINT32_MAX);

/*
    Initial capacity of active viewport placements.
*/
#ifndef SFTE_IMG_PLACEMENT_INIT_CAP
#define SFTE_IMG_PLACEMENT_INIT_CAP 16
#endif  // SFTE_IMG_PLACEMENT_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_IMG_PLACEMENT_INIT_CAP, 1, SFTE_IMG_PLACEMENT_MAX_CAP);

// =================================================================================================
// >>input macros
// =================================================================================================

/*
    Enables mouse support.
    Required for SFTE_INPUT_SELECTION to work.
*/
#ifndef SFTE_INPUT_MOUSE
#define SFTE_INPUT_MOUSE 1
#endif  // SFTE_INPUT_MOUSE
_SFTE_ENSURE_RANGE(SFTE_INPUT_MOUSE, 0, 1);

/*
    Enables text selection.

    Requires
    #define SFTE_INPUT_MOUSE 1
    to work.
*/
#ifndef SFTE_INPUT_SELECTION
#define SFTE_INPUT_SELECTION 1
#endif  // SFTE_INPUT_SELECTION
_SFTE_ENSURE_RANGE(SFTE_INPUT_SELECTION, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_INPUT_SELECTION, SFTE_INPUT_MOUSE);

/*
    Enables kitty extended keyboard protocol.
    If enabled, the terminal sends key release events and complex modifiers.
*/
#ifndef SFTE_INPUT_KITTY
#define SFTE_INPUT_KITTY 1
#endif  // SFTE_INPUT_KITTY
_SFTE_ENSURE_RANGE(SFTE_INPUT_KITTY, 0, 1);

/*
    Enables OSC 8 clickable terminal hyperlinks.
*/
#ifndef SFTE_INPUT_HYPERLINKS
#define SFTE_INPUT_HYPERLINKS 1
#endif  // SFTE_INPUT_HYPERLINKS
_SFTE_ENSURE_RANGE(SFTE_INPUT_HYPERLINKS, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_INPUT_HYPERLINKS, SFTE_INPUT_MOUSE);

/*
    Maximum dynamic buffer count for hyperlinks.
    Each cell stores a link index, so this defines the maximum amount of UNIQUE hyperlinks.
*/
#ifndef SFTE_INPUT_HYPERLINKS_MAX_CAP
#define SFTE_INPUT_HYPERLINKS_MAX_CAP 65535
#endif  // SFTE_INPUT_HYPERLINKS_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_INPUT_HYPERLINKS_MAX_CAP, 16, UINT32_MAX);

/*
    Initial dynamic buffer count for hyperlinks.
*/
#ifndef SFTE_INPUT_HYPERLINKS_INIT_CAP
#define SFTE_INPUT_HYPERLINKS_INIT_CAP 16
#endif  // SFTE_INPUT_HYPERLINKS_POOL_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_INPUT_HYPERLINKS_INIT_CAP, 1, SFTE_INPUT_HYPERLINKS_MAX_CAP);

// =================================================================================================
// >>clipboard macros
// =================================================================================================

/*
    Enables system clipboard sync.
*/
#ifndef SFTE_CLIPBOARD
#define SFTE_CLIPBOARD 1
#endif  // SFTE_CLIPBOARD
_SFTE_ENSURE_RANGE(SFTE_CLIPBOARD, 0, 1);

/*
    Maximum buffer size to copy at once.
*/
#ifndef SFTE_CLIPBOARD_BUF_SIZE
#define SFTE_CLIPBOARD_BUF_SIZE 4096
#endif  // SFTE_CLIPBOARD_BUF_SIZE
_SFTE_ENSURE_RANGE(SFTE_CLIPBOARD_BUF_SIZE, 1, UINT32_MAX);

/*
    Allows host applications to read/write the clipboard via OSC 52.
    This is especially useful to synchronize the clipboard with a SSH'd machine.
*/
#ifndef SFTE_CLIPBOARD_OSC52
#define SFTE_CLIPBOARD_OSC52 1
#endif  // SFTE_CLIPBOARD_OSC52
_SFTE_ENSURE_RANGE(SFTE_CLIPBOARD_OSC52, 0, 1);
_SFTE_ENSURE_DEPS(SFTE_CLIPBOARD_OSC52, SFTE_CLIPBOARD);

// =================================================================================================
// >>osc macros
// =================================================================================================

/*
    Maximum size of OSC payload buffer.
    Used for base64 clipboard data or long links.
*/
#ifndef SFTE_OSC_MAX_CAP
#define SFTE_OSC_MAX_CAP (16 * 1024 * 1024)
#endif  // SFTE_OSC_MAX_CAP
_SFTE_ENSURE_RANGE(SFTE_OSC_MAX_CAP, 128, UINT32_MAX);

/*
    Initial size of OSC payload buffer.
*/
#ifndef SFTE_OSC_INIT_CAP
#define SFTE_OSC_INIT_CAP 128
#endif  // SFTE_OSC_INIT_CAP
_SFTE_ENSURE_RANGE(SFTE_OSC_INIT_CAP, 1, SFTE_OSC_MAX_CAP);

// =================================================================================================
// >>modifiers and shortcuts macros
// =================================================================================================

typedef union {
    int i;
    float f;
    const void *v;
} sfte_arg;

typedef struct {
    uint32_t mod_mask;
    uint32_t /* xkb_keysym_t */ keysym;
    void (*func)(sfte_ctx *ctx, const sfte_arg *);
    const sfte_arg arg;
} sfte_shortcut;

#if SFTE_WAYLAND
#if SFTE_FONT_ZOOM
static inline void _sfte_wayland_font_resize(sfte_ctx *ctx, const sfte_arg *arg);
static inline void _sfte_wayland_font_reset(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_FONT_ZOOM
#if SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_wayland_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg);
static inline void _sfte_wayland_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_WAYLAND

#if SFTE_WIN32
#if SFTE_FONT_ZOOM
static inline void _sfte_win32_font_resize(sfte_ctx *ctx, const sfte_arg *arg);
static inline void _sfte_win32_font_reset(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_FONT_ZOOM
#if SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_win32_view_scroll(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_TERM_SCROLLBACK_CAP
#if SFTE_CLIPBOARD
#if SFTE_INPUT_SELECTION
static inline void _sfte_win32_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_INPUT_SELECTION
static inline void _sfte_win32_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_CLIPBOARD
#endif  // SFTE_WIN32

#if SFTE_FONT_ZOOM && SFTE_WAYLAND
#define _SFTE_WAYLAND_ZOOM_BINDS                                                                   \
    {SFTE_MOD_CTRL, XKB_KEY_equal, _sfte_wayland_font_resize, {.f = 2.0f}},                        \
        {SFTE_MOD_CTRL, XKB_KEY_plus, _sfte_wayland_font_resize, {.f = 2.0f}},                     \
        {SFTE_MOD_CTRL, XKB_KEY_minus, _sfte_wayland_font_resize, {.f = -2.0f}},                   \
        {SFTE_MOD_CTRL, XKB_KEY_0, _sfte_wayland_font_reset, {.v = NULL}},
#else  // !SFTE_FONT_ZOOM || !SFTE_WAYLAND
#define _SFTE_WAYLAND_ZOOM_BINDS
#endif  // !SFTE_FONT_ZOOM || !SFTE_WAYLAND

#if SFTE_TERM_SCROLLBACK_CAP && SFTE_WAYLAND
#define _SFTE_WAYLAND_SCROLL_BINDS                                                                 \
    {SFTE_MOD_SHIFT, XKB_KEY_Page_Up, _sfte_wayland_view_scroll, {.i = 10}},                       \
        {SFTE_MOD_SHIFT, XKB_KEY_Page_Down, _sfte_wayland_view_scroll, {.i = -10}},
#else  // !SFTE_TERM_SCROLLBACK_CAP || !SFTE_WAYLAND
#define _SFTE_WAYLAND_SCROLL_BINDS
#endif  // !SFTE_TERM_SCROLLBACK_CAP || !SFTE_WAYLAND

#if SFTE_CLIPBOARD && SFTE_WAYLAND
#if SFTE_INPUT_SELECTION
#define _SFTE_WAYLAND_COPY_BIND                                                                    \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, XKB_KEY_C, _sfte_wayland_clipboard_copy, {.v = NULL}},
#endif  // SFTE_INPUT_SELECTION
#define _SFTE_WAYLAND_PASTE_BIND                                                                   \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, XKB_KEY_V, _sfte_wayland_clipboard_paste, {.v = NULL}},
#else  // !SFTE_CLIPBOARD || !SFTE_WAYLAND
#define _SFTE_WAYLAND_PASTE_BIND
#endif  // !SFTE_CLIPBOARD || !SFTE_WAYLAND

#if !SFTE_CLIPBOARD || !SFTE_WAYLAND || !SFTE_INPUT_SELECTION
#define _SFTE_WAYLAND_COPY_BIND
#endif  // !SFTE_CLIPBOARD || !SFTE_WAYLAND || !SFTE_INPUT_SELECTION

#if SFTE_FONT_ZOOM && SFTE_WIN32
#define _SFTE_WIN32_ZOOM_BINDS                                                                     \
    {SFTE_MOD_CTRL, 0xBB /* VK_OEM_PLUS '='/'+' */, _sfte_win32_font_resize, {.f = 2.0f}},         \
        {SFTE_MOD_CTRL, 0x6B /* VK_ADD */, _sfte_win32_font_resize, {.f = 2.0f}},                  \
        {SFTE_MOD_CTRL, 0xBD /* VK_OEM_MINUS '-' */, _sfte_win32_font_resize, {.f = -2.0f}},       \
        {SFTE_MOD_CTRL, 0x6D /* VK_SUBTRACT */, _sfte_win32_font_resize, {.f = -2.0f}},            \
        {SFTE_MOD_CTRL, 0x30 /* '0' */, _sfte_win32_font_reset, {.v = NULL}},
#else  // !SFTE_FONT_ZOOM || !SFTE_WIN32
#define _SFTE_WIN32_ZOOM_BINDS
#endif  // !SFTE_FONT_ZOOM || !SFTE_WIN32

#if SFTE_TERM_SCROLLBACK_CAP && SFTE_WIN32
#define _SFTE_WIN32_SCROLL_BINDS                                                                   \
    {SFTE_MOD_SHIFT, 0x21 /* VK_PRIOR */, _sfte_win32_view_scroll, {.i = 10}},                     \
        {SFTE_MOD_SHIFT, 0x22 /* VK_NEXT */, _sfte_win32_view_scroll, {.i = -10}},
#else  // !SFTE_TERM_SCROLLBACK_CAP || !SFTE_WIN32
#define _SFTE_WIN32_SCROLL_BINDS
#endif  // !SFTE_TERM_SCROLLBACK_CAP || !SFTE_WIN32

#if SFTE_CLIPBOARD && SFTE_WIN32
#if SFTE_INPUT_SELECTION
#define _SFTE_WIN32_COPY_BIND                                                                      \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, 0x43 /* 'C' */, _sfte_win32_clipboard_copy, {.v = NULL}},
#endif  // SFTE_INPUT_SELECTION
#define _SFTE_WIN32_PASTE_BIND                                                                     \
    {SFTE_MOD_CTRL | SFTE_MOD_SHIFT, 0x56 /* 'V' */, _sfte_win32_clipboard_paste, {.v = NULL}},
#else  // !SFTE_CLIPBOARD || !SFTE_WIN32
#define _SFTE_WIN32_PASTE_BIND
#endif  // !SFTE_CLIPBOARD || !SFTE_WIN32

#if !SFTE_CLIPBOARD || !SFTE_WIN32 || !SFTE_INPUT_SELECTION
#define _SFTE_WIN32_COPY_BIND
#endif  // !SFTE_CLIPBOARD || !SFTE_WIN32 || !SFTE_INPUT_SELECTION

#define SFTE_BASE_SHORTCUTS                                                                        \
    _SFTE_WAYLAND_ZOOM_BINDS _SFTE_WAYLAND_SCROLL_BINDS _SFTE_WAYLAND_COPY_BIND                    \
        _SFTE_WAYLAND_PASTE_BIND _SFTE_WIN32_ZOOM_BINDS _SFTE_WIN32_SCROLL_BINDS                   \
            _SFTE_WIN32_COPY_BIND _SFTE_WIN32_PASTE_BIND

#ifndef SFTE_SHORTCUTS
#define SFTE_SHORTCUTS {SFTE_BASE_SHORTCUTS}
#endif  // SFTE_SHORTCUTS

// #################################################################################################
// >>>PUBLIC API
// #################################################################################################

// =================================================================================================
// >>enums & structs
// =================================================================================================

typedef enum sfte_font_style {
    SFTE_FONT_STYLE_REGULAR = 0,
#ifdef SFTE_FONT_BOLD
    SFTE_FONT_STYLE_BOLD = 1,
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    SFTE_FONT_STYLE_ITALIC = 2,
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    SFTE_FONT_STYLE_BOLD_ITALIC = 3,
#endif  // SFTE_FONT_BOLD_ITALIC
} sfte_font_style;

typedef enum sfte_modifier {
    SFTE_MOD_NONE = 0b0000,
    SFTE_MOD_CTRL = 0b0001,
    SFTE_MOD_ALT = 0b0010,
    SFTE_MOD_SHIFT = 0b0100,
    SFTE_MOD_SUPER = 0b1000,
} sfte_modifier;

/*
    Used to report what regions of the pixel buffer have changed.
*/
typedef struct sfte_damage_rect {
    int32_t x, y, w, h;
} sfte_damage_rect;

typedef enum sfte_key {
    SFTE_KEY_NONE = 0,
    // Control keys (ASCII-based)
    SFTE_KEY_TAB = 9,
    SFTE_KEY_ENTER = 13,
    SFTE_KEY_ESCAPE = 27,
    SFTE_KEY_BACKSPACE = 127,
    // Navigation keys (offset into dedicated range)
    SFTE_KEY_UP = 1000,
    SFTE_KEY_DOWN,
    SFTE_KEY_LEFT,
    SFTE_KEY_RIGHT,
    SFTE_KEY_HOME,
    SFTE_KEY_END,
    SFTE_KEY_PAGE_UP,
    SFTE_KEY_PAGE_DOWN,
    SFTE_KEY_INSERT,
    SFTE_KEY_DELETE,
    // Function keys
    SFTE_KEY_F1,
    SFTE_KEY_F2,
    SFTE_KEY_F3,
    SFTE_KEY_F4,
    SFTE_KEY_F5,
    SFTE_KEY_F6,
    SFTE_KEY_F7,
    SFTE_KEY_F8,
    SFTE_KEY_F9,
    SFTE_KEY_F10,
    SFTE_KEY_F11,
    SFTE_KEY_F12,
} sfte_key;

typedef enum sfte_mouse_button {
    SFTE_MOUSE_BUTTON_LEFT = 0,
    SFTE_MOUSE_BUTTON_MIDDLE = 1,
    SFTE_MOUSE_BUTTON_RIGHT = 2,
    SFTE_MOUSE_BUTTON_NONE = 3,  // Used internally for release states in legacy protocol
} sfte_mouse_button;

// =================================================================================================
// >>callbacks
// =================================================================================================

/*
    Callback interface for the core to talk back to the host.
*/
typedef void (*sfte_write_cb)(void *user_data, const char *data, size_t len);

#if SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52
/*
    Callback interface for the core to request clipboard copy/paste operations.
    `target` is typically 'c' (clipboard) or 'p' (primary selection).
*/
typedef void (*sfte_osc52_clipboard_cb)(void *user_data, char target, const char *data);
#endif  // SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52

#if SFTE_INPUT_HYPERLINKS
/*
    Callback interface for the core to request opening a URI.
*/
typedef void (*sfte_open_link_cb)(void *user_data, const char *uri);
#endif  // SFTE_INPUT_HYPERLINKS

/*
    Fired when the terminal receives the BEL (\a) character.
*/
typedef void (*sfte_bell_cb)(void *user_data);

/*
    Fired when the terminal receives an OSC 0 or OSC 2 title change sequence.
*/
typedef void (*sfte_title_cb)(void *user_data, const char *title);

/*
    Fired when the terminal receives an OSC 9;4 progress bar sequence.
    `state` can either be:
        0 for remove,
        1 for normal,
        2 for error,
        3 for indeterminate,
        4 for warning.
    `progress` is a value from 0 to 100.
*/
typedef void (*sfte_progress_cb)(void *user_data, uint8_t state, uint8_t progress);

// =================================================================================================
// >>core initialization & lifecycle
// =================================================================================================

/*
    Context (de)initialization
*/
sfte_ctx *sfte_init(sfte_write_cb write_fn, void *user_data);
void sfte_free(sfte_ctx *ctx);

/*
    Loads a TTF font from a raw memory buffer (e.g. compiled into the binary).
*/
void sfte_font_load_mem(sfte_ctx *ctx, sfte_font_style style, const uint8_t *ttf_data);

/*
    Convenience function to load a TTF font from the disk.
    Useful for runtime font swap functionality.
*/
void sfte_font_load_file(sfte_ctx *ctx, sfte_font_style style, const char *path);

#ifndef SFTE_NO_POSIX
/*
    Spawns a shell and populates `out_fd` with the master PTY descriptor.
    Returns the shell PID.
*/
pid_t sfte_posix_pty_spawn(sfte_ctx *ctx, int32_t *out_fd, uint16_t px_w, uint16_t px_h);

/*
    Sends the TIOCSWINSZ ioctl to keep the OS shell in sync with the engine grid.
*/
void sfte_posix_pty_resize(sfte_ctx *ctx, int32_t pty_fd, uint16_t px_w, uint16_t px_h);
#endif  // SFTE_NO_POSIX

/*
    Returns the maximum time (in milliseconds) the host event loop
    should sleep before the terminal requires a redraw.
    Returns -1 if the terminal is fully static and can sleep indefinitely.
*/
int32_t sfte_get_timeout_ms(sfte_ctx *ctx);

/*
    Updates time-based internal state (animations, cursor trail, blinking).
    Returns 1 if the terminal visual state changed and requires a redraw.
*/
uint8_t sfte_tick(sfte_ctx *ctx);

// =================================================================================================
// >>rendering & parsing
// =================================================================================================

/*
    Ask engine how many pixels it needs to display a specified grid.
*/
void sfte_get_ideal_size(sfte_ctx *ctx, int16_t cols, int16_t rows, int32_t *out_w, int32_t *out_h);

/*
    Feed bytes from the shell/PTY to the terminal state machine.
*/
void sfte_parse(sfte_ctx *ctx, const uint8_t *data, size_t len);

/*
    Render the grid to the provided `px_buf` buffer
    and populate the `out_dmg` damage rectangle.

*/
void sfte_render(sfte_ctx *ctx, void *px_buf, int32_t w, int32_t h, sfte_damage_rect *out_dmg);

/*
    Explicitly tell the terminal engine its canvas size has changed.
*/
void sfte_resize(sfte_ctx *ctx, int32_t w, int32_t h);

#if SFTE_FONT_ZOOM
/*
    Zooms in/out changing the font size, rebaking and reallocating the terminal grid data.
    `delta` > 0 - zoom in
    `delta` < 0 - zoom out
    `delta` = 0 - noop
*/
void sfte_zoom(sfte_ctx *ctx, float delta);
#endif  // SFTE_FONT_ZOOM

// =================================================================================================
// >>input & interaction
// =================================================================================================

/*
    Feed raw UTF-8 text into the terminal.
    Used for both keyboard typing and streaming clipboard chunks.
*/
void sfte_input_text(sfte_ctx *ctx, const char *text, size_t len);

/*
    Signals the start of a clipboard paste operation.
    Generates \033[200~ (bracketed paste) if enabled by the shell.
*/
void sfte_input_paste_begin(sfte_ctx *ctx);

/*
    Signals the end of a clipboard paste operation.
    Generates \033[201~ (bracketed paste) if enabled by the shell.
*/
void sfte_input_paste_end(sfte_ctx *ctx);

/*
    Feed a physical control key into the terminal.
    Automatically generates VT100/ANSI escape codes (arrows, F-keys).
    Routes through `sfte_write_cb` internally.
*/
void sfte_input_key(sfte_ctx *ctx, sfte_key key, uint32_t mod_mask);

#if SFTE_INPUT_MOUSE
/*
    Feed raw OS mouse events to the terminal engine for hover events and drag selections.
*/
void sfte_mouse_move(sfte_ctx *ctx, int32_t px_x, int32_t px_y);

/*
    Feed mouse clicks to the engine.
    `pressed` is a boolean: 1 for button down, 0 for button release.
*/
void sfte_mouse_click(sfte_ctx *ctx, sfte_mouse_button btn, uint8_t pressed, int32_t px_x,
                      int32_t px_y);

/*
    Feed scroll wheel events to the terminal engine.
    `dir` is the scroll direction: positive (>0) for up, negative (<0) for down.
*/
void sfte_mouse_scroll(sfte_ctx *ctx, int8_t dir, int32_t px_x, int32_t px_y);
#endif  // SFTE_INPUT_MOUSE

#if SFTE_INPUT_HYPERLINKS
/*
    Returns the URL at the given cell coords.
    Returns NULL if no link is present.
*/
const char *sfte_get_link_at(sfte_ctx *ctx, int16_t col, int16_t row);
#endif  // SFTE_INPUT_HYPERLINKS

#if SFTE_INPUT_SELECTION
/*
    Copies the UTF-8 selection into `out_buf`, up to `max_bytes`.
    If `out_buf` is NULL, performs a dry-run and returns the required byte size.
    Returns 0 if nothing is selected.
*/
size_t sfte_get_selection(sfte_ctx *ctx, char *out_buf, size_t max_bytes);
#endif  // SFTE_INPUT_SELECTION

#if SFTE_TERM_SCROLLBACK_CAP
/*
    Shift viewport up or down in the scrollback buffer.
*/
void sfte_view_scroll(sfte_ctx *ctx, int32_t delta);
#endif  // SFTE_TERM_SCROLLBACK_CAP

#ifdef SFTE_XKB_COMMON
/*
    Input entrypoint for XKB (Linux) implementations.
    Handles modifiers, control keys, kitty keyboard support (if enabled).
*/
void sfte_xkb_process_key(sfte_ctx *ctx, struct xkb_state *state, uint32_t keycode);
#endif  // SFTE_XKB_COMMON

#if SFTE_TERM_FOCUS
/*
    Set window focus to `focused`.
    Toggles the state, dirties the cursor cell and sends I/O escape sequences to PTY.
*/
void sfte_set_focus(sfte_ctx *ctx, uint8_t focused);
#endif  // SFTE_TERM_FOCUS

// =================================================================================================
// >>wayland backend
// =================================================================================================

#if SFTE_WAYLAND
typedef struct sfte_wayland_app sfte_wayland_app;

/*
    Initializes Wayland, PTY and `sfte_ctx`.
*/
sfte_wayland_app *sfte_wayland_init(void);

/*
    Exposes the context for runtime configuration.
*/
sfte_ctx *sfte_wayland_get_ctx(sfte_wayland_app *app);

/*
    Enters the blocking event loop, run it last.
*/
int sfte_wayland_run(sfte_wayland_app *app);
#endif  // SFTE_WAYLAND

// =================================================================================================
// >>win32 backend
// =================================================================================================

#if SFTE_WIN32
typedef struct sfte_win32_app sfte_win32_app;

/*
    Initializes the Win32 backend and `sfte_ctx`.
    Call `sfte_win32_set_shell` (optional) and load fonts before `sfte_win32_run`.
*/
sfte_win32_app *sfte_win32_init(void);

/*
    Exposes the context for runtime configuration.
*/
sfte_ctx *sfte_win32_get_ctx(sfte_win32_app *app);

/*
    Creates the window and enters the blocking event loop, run it last.
*/
int sfte_win32_run(sfte_win32_app *app);

/*
    Overrides the shell command line (UTF-8). If not called, the backend resolves
    SFTE_SHELL, ash.exe, powershell.exe, COMSPEC and finally cmd.exe.
*/
void sfte_win32_set_shell(sfte_win32_app *app, const char *cmdline);
#endif  // SFTE_WIN32

#ifdef SFTE_IMPL
// #################################################################################################
// >>>INTERNAL DECLARATIONS
// #################################################################################################

#ifndef SFTE_FONT_CUSTOM_BACKEND
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "vendor/stb_truetype.h"
#endif  // !SFTE_FONT_CUSTOM_BACKEND

#if SFTE_IMG_KITTY
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF  // NOTE: remove this with gif support added
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_FAILURE_STRINGS
#define STBI_NO_THREAD_LOCALS
#include "vendor/stb_image.h"
#endif  // SFTE_IMG_KITTY

#include <locale.h>  // LC_ALL
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>  // memset

#ifndef SFTE_NO_POSIX
#include <fcntl.h>
#include <poll.h>
#include <pty.h>  // forkpty
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/wait.h>
#include <unistd.h>  // exec/fork/env
#endif  // !SFTE_NO_POSIX

#if SFTE_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif  // NOMINMAX
#include <windows.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <wchar.h>
#endif  // SFTE_WIN32

#if SFTE_CURSOR_BLINK
#include <time.h>
#endif  // SFTE_CURSOR_BLINK

#if SFTE_WAYLAND
#include "vendor/xdg-shell.c"
#include "vendor/xdg-shell.h"
#include <wayland-client.h>
#endif  // SFTE_WAYLAND

// =================================================================================================
// >>infrastructure macros
// =================================================================================================

// Retrieves the number of elements in a statically allocated array.
#define _SFTE_ARRAY_LEN(x) (sizeof(x) / sizeof((x)[0]))

// Constraints a value within a specified inclusive range [min, max].
#define _SFTE_CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

// Reads a 16-bit big-endian value from `d` array starting from `off` byte.
#define _SFTE_R16BE(d, off) (uint16_t)(((d)[off] << 8) | (d)[(off) + 1])

// Reads a 32-bit big-endian value from `d` array starting from `off` byte.
#define _SFTE_R32BE(d, off)                                                                        \
    (uint32_t)(((d)[off] << 24) | ((d)[(off) + 1] << 16) | ((d)[(off) + 2] << 8) | (d)[(off) + 3])

#ifndef SFTE_NO_LOGGING

#define _SFTE_LOG_ITEMS                                                                            \
    _SFTE_LOGITEM_XMACRO(OK, "ok")                                                                 \
    _SFTE_LOGITEM_XMACRO(FONT_LOADED, "font loaded and baked to atlas")                            \
    _SFTE_LOGITEM_XMACRO(                                                                          \
        FONT_LOAD_FAIL,                                                                            \
        "Failed to open font file: '%s'\nIf this is your first time running sfte, you need to "    \
        "edit 'config.c' and set the correct font paths, then recompile.\n")                       \
    _SFTE_LOGITEM_XMACRO(PTY_SPAWN, "master/slave pair successfully spawned")                      \
    _SFTE_LOGITEM_XMACRO(PTY_FORK_FAIL, "forkpty failed with errno: '%d'")                         \
    _SFTE_LOGITEM_XMACRO(UNHANDLED_CSI, "unhandled CSI command: '%c' with '%d' parms")             \
    _SFTE_LOGITEM_XMACRO(UNHANDLED_OSC, "unhandled OSC payload: '%s'")                             \
    _SFTE_LOGITEM_XMACRO(TERM_RESIZE, "resized grid to '%dx%d'")                                   \
    _SFTE_LOGITEM_XMACRO(WAYLAND_REGISTRY_BOUND, "wayland globals bound")                          \
    _SFTE_LOGITEM_XMACRO(KEYMAP_LOADED, "xkb keymap loaded from compositor")                       \
    _SFTE_LOGITEM_XMACRO(SHELL_FALLBACK, "SHELL env var unset, falling back to /bin/sh")           \
    _SFTE_LOGITEM_XMACRO(CLIPBOARD_EMPTY, "clipboard call requested but buffer is empty")

#define _SFTE_LOGITEM_XMACRO(item, msg) item,
typedef enum { _SFTE_LOG_ITEMS } _sfte_log_item;
#undef _SFTE_LOGITEM_XMACRO

#define _SFTE_LOGITEM_XMACRO(item, msg) #item ": " msg,
static const char *_sfte_log_messages[] = {_SFTE_LOG_ITEMS};
#undef _SFTE_LOGITEM_XMACRO

#define _SFTE_PANIC(ctx, code, ...)                                                                \
    _sfte_log(ctx, code, SFTE_LOG_LVL_PANIC, __LINE__, ##__VA_ARGS__)
#define _SFTE_ERROR(ctx, code, ...)                                                                \
    _sfte_log(ctx, code, SFTE_LOG_LVL_ERROR, __LINE__, ##__VA_ARGS__)
#define _SFTE_WARN(ctx, code, ...) _sfte_log(ctx, code, SFTE_LOG_LVL_WARN, __LINE__, ##__VA_ARGS__)
#define _SFTE_INFO(ctx, code, ...) _sfte_log(ctx, code, SFTE_LOG_LVL_INFO, __LINE__, ##__VA_ARGS__)

#else  // !SFTE_NO_LOGGING

#define _SFTE_PANIC(ctx, code, ...) abort()
#define _SFTE_ERROR(ctx, code, ...)                                                                \
    do {                                                                                           \
    } while (0)
#define _SFTE_WARN(ctx, code, ...)                                                                 \
    do {                                                                                           \
    } while (0)
#define _SFTE_INFO(ctx, code, ...)                                                                 \
    do {                                                                                           \
    } while (0)

#endif  // !SFTE_NO_LOGGING

#if SFTE_FONT_WIDE_CHARS
#ifdef _WIN32
/*
    Minimal wcwidth replacement for Windows toolchains (MinGW-w64 does not provide wcwidth).
    Handles combining/zero-width marks and the common East Asian wide/fullwidth ranges.
*/
static inline int _sfte_win32_wcwidth(uint32_t rune) {
    if ((rune >= 0x0300 && rune <= 0x036F) || (rune >= 0x1AB0 && rune <= 0x1AFF) ||
        (rune >= 0x1DC0 && rune <= 0x1DFF) || (rune >= 0x20D0 && rune <= 0x20FF) ||
        (rune >= 0xFE00 && rune <= 0xFE0F) || (rune >= 0xFE20 && rune <= 0xFE2F) ||
        rune == 0x200B || rune == 0x200C || rune == 0x200D || rune == 0xFEFF)
        return 0;

    if ((rune >= 0x1100 && rune <= 0x115F) ||
        (rune >= 0x2E80 && rune <= 0xA4CF && rune != 0x303F) ||
        (rune >= 0xAC00 && rune <= 0xD7A3) || (rune >= 0xF900 && rune <= 0xFAFF) ||
        (rune >= 0xFE10 && rune <= 0xFE19) || (rune >= 0xFE30 && rune <= 0xFE6F) ||
        (rune >= 0xFF00 && rune <= 0xFF60) || (rune >= 0xFFE0 && rune <= 0xFFE6) ||
        (rune >= 0x1F300 && rune <= 0x1FAFF) || (rune >= 0x20000 && rune <= 0x3FFFD))
        return 2;

    return 1;
}
#define _SFTE_CHAR_WIDTH(rune) _sfte_win32_wcwidth(rune)
#else
#include <wchar.h>
#define _SFTE_CHAR_WIDTH(rune) wcwidth(rune)
#endif  // _WIN32
#else
#define _SFTE_CHAR_WIDTH(rune) 1
#endif

#if SFTE_CURSOR_BLINK || SFTE_CURSOR_TRAIL || (SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP)
#ifndef SFTE_TIME_MS
#ifdef _WIN32
static inline uint64_t _sfte_time_ms(void) {
    return (uint64_t)GetTickCount64();
}
#else
#include <time.h>
static inline uint64_t _sfte_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif  // _WIN32
#define SFTE_TIME_MS() _sfte_time_ms()
#endif  // SFTE_TIME_MS
#endif  // SFTE_CURSOR_BLINK || SFTE_CURSOR_TRAIL || (SFTE_TERM_SCROLL_SMOOTH &&
        // SFTE_TERM_SCROLLBACK_CAP)
// =================================================================================================
// >>internal data structures
// =================================================================================================
typedef struct sfte_stack {
    uint8_t *buf;
    size_t cap;
    size_t off;
} sfte_stack;

#ifndef SFTE_NO_LOGGING
typedef struct sfte_logger {
    void (*func)(const char *tag,  // Always "sfte"
                 sfte_log_level log_level,
                 const char *message_or_null,  // A message string, may be nullptr in release mode
                 uint32_t line_nr              // Line number in sfte.h
    );
} sfte_logger;
#endif  // !SFTE_NO_LOGGING

#if SFTE_COLOR_TRUECOLOR
typedef uint32_t sfte_color;
#define _SFTE_COLOR_FG_DEFAULT SFTE_COLOR_FG
#define _SFTE_COLOR_BG_DEFAULT SFTE_COLOR_BG
#else  // !SFTE_COLOR_TRUECOLOR
typedef uint8_t sfte_color;
#define _SFTE_COLOR_FG_DEFAULT 254
#define _SFTE_COLOR_BG_DEFAULT 255
#endif  // !SFTE_COLOR_TRUECOLOR

#if SFTE_TERM_ASCII_CHARSET
typedef uint8_t sfte_rune;
#else   // !SFTE_TERM_ASCII_CHARSET
typedef uint32_t sfte_rune;
#endif  // SFTE_TERM_ASCII_CHARSET

typedef enum {
    _SFTE_ATTR_NONE = 0,
    _SFTE_ATTR_BOLD = 1 << 0,
    _SFTE_ATTR_ITALIC = 1 << 1,
    _SFTE_ATTR_UNDERLINE = 1 << 2,
    _SFTE_ATTR_REVERSE = 1 << 3,
#if SFTE_FONT_WIDE_CHARS
    _SFTE_ATTR_WIDE = 1 << 4,
    _SFTE_ATTR_DUMMY = 1 << 5,  // Marks skipped trailing cell after wide rune
#endif                          // SFTE_FONT_WIDE_CHARS
} sfte_attr;

/*
    Represents a single cell on the terminal grid.
*/
typedef struct {
    sfte_rune rune;
#if SFTE_FONT_WIDE_CHARS && SFTE_FONT_MAX_COMBINING
    sfte_rune combining_runes[SFTE_FONT_MAX_COMBINING];
#endif  // SFTE_FONT_WIDE_CHARS && SFTE_FONT_MAX_COMBINING

#if SFTE_INPUT_HYPERLINKS
    uint16_t link_idx;  // 0=no link, >0=index to `term.link_pool`
#endif                  // SFTE_INPUT_HYPERLINKS

    sfte_color fg;
    sfte_color bg;
#if SFTE_UNDERLINE_COLORED
    sfte_color ul_color;
#endif             // SFTE_UNDERLINE_COLORED
    uint8_t attr;  // Bitmask of sfte_attr
#if SFTE_FONT_WIDE_CHARS
    uint8_t combining_cnt;
#endif  // SFTE_FONT_WIDE_CHARS
#if SFTE_UNDERLINE_EXTENDED
    uint8_t ul_style;  // 1=straight, 2=double, 3=curl, 4=dotted, 5=dashed
#endif                 // SFTE_UNDERLINE_EXTENDED
    uint8_t dirty;     // 1 if this cell changed and needs redraw
#if SFTE_TERM_REFLOW
    uint8_t wrapped;  // 1 if this cell caused a soft line-wrap
#endif                // SFTE_TERM_REFLOW
} sfte_cell;

/*
    Represents a baked texture atlas entry for a single character.
*/
typedef struct {
    uint16_t glyph_id;
    int16_t xadvance;
    uint16_t x0, y0, x1, y1;  // Atlas texture coordinates
    int16_t xoff, yoff;       // Render offsets
    uint8_t font_idx;         // 0=primary, 1+=fallbacks
} sfte_glyph;

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
/*
    Shared image buffer data.
*/
typedef struct {
    uint32_t *pixels;  // Format ARGB8888
    uint32_t id;
    uint32_t ref_cnt;  // How many placements are using this image
    int32_t width;     // In pixels
    int32_t height;    // In pixels
    uint8_t is_sixel;
} sfte_img;

/*
    Represents where and how an image is drawn on the screen.
*/
typedef struct {
    uint32_t img_id;
    uint32_t placement_id;
    int16_t start_col;
    int16_t start_row;
    int16_t x_off;
    int16_t y_off;
    int8_t z_idx;  // <0=below text, >=0=above text
    uint8_t is_sixel;
#if SFTE_TERM_ALT_SCREEN
    uint8_t alt_screen;  // 0=main, 1=alt
#endif                   // SFTE_TERM_ALT_SCREEN
} sfte_img_placement;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_IMG_SIXEL
typedef enum {
    SIXEL_GROUND,
    SIXEL_REPEAT,       // !
    SIXEL_COLOR_INTRO,  // #
    SIXEL_COLOR_PARAM   // Color definition
} sfte_sixel_state_enum;

typedef struct {
    uint32_t *pixels;  // Temporary dynamic buffer for the in-progress image

    uint32_t palette[256];
    uint32_t repeat_cnt;

    int32_t x, y;           // In pixels
    int32_t width, height;  // Maximum bounds currently touched
    int32_t cap_w, cap_h;   // Allocated capacity of `pixels`

    int16_t start_col;   // Grid column where parsing started
    int16_t start_row;   // Grid row where parsing started
    uint16_t params[5];  // Parsed numbers for color registers/HLS/RGB

    sfte_sixel_state_enum state;
    uint8_t col_idx;    // Active palette register index
    uint8_t param_idx;  // Current parameter index
} sfte_sixel_state;
#endif  // SFTE_IMG_SIXEL

#if SFTE_IMG_KITTY
typedef struct {
    char *b64_buf;
    size_t b64_len;
    size_t b64_cap;

    uint32_t id;
    uint32_t placement_id;
    int32_t width;   // Source image width in pixels
    int32_t height;  // Source image height in pixels
    int32_t crop_x;  // Crop X start in pixels
    int32_t crop_y;  // Crop Y start in pixels
    int32_t crop_w;  // Crop width in pixels
    int32_t crop_h;  // Crop height in pixels

    int16_t cols;   // Target grid columns
    int16_t rows;   // Target height columns
    int16_t x_off;  // Placement X offset
    int16_t y_off;  // Placement Y offset

    int8_t z_idx;    // <0=below text, >=0=above text
    uint8_t format;  // 24=RGB, 32=RGBA, 100=PNG/JPEG
    uint8_t quiet;   // 0=always, 1=error, 2=never
    char action;     // Protocol action
    char t_medium;   // Transmission medium
    char d_action;   // Delete action
} sfte_kitty_state;
#endif  // SFTE_IMG_KITTY

#if SFTE_FONT_LIGATURES
typedef struct {
    uint16_t lookup_indices[SFTE_FONT_MAX_LIGATURE_LOOKUPS];
    uint32_t subtable_offs[SFTE_FONT_MAX_LIGATURE_LOOKUPS][SFTE_FONT_MAX_LIGATURE_SUBTABLES];
    uint16_t subtable_cnts[SFTE_FONT_MAX_LIGATURE_LOOKUPS];
    uint16_t lookup_types[SFTE_FONT_MAX_LIGATURE_LOOKUPS];
    uint16_t lookup_cnt;
} sfte_shaper_feature;

/*
    Font shaper data context.
*/
typedef struct {
    uint32_t table_off;
    uint32_t script_list_off;
    uint32_t feat_list_off;
    uint32_t lookup_list_off;

    sfte_shaper_feature calt;  // Contextual alternates
    sfte_shaper_feature liga;  // Standard ligatures
} sfte_shaper_ctx;

typedef struct {
    uint16_t sequence_idx;
    uint16_t lookup_idx;
} sfte_shaper_subst_record;
#endif  // SFTE_FONT_LIGATURES

/*
    Font variant texture cache.
*/
typedef struct {
#if SFTE_FONT_LIGATURES
    sfte_shaper_ctx shaper[SFTE_FONT_MAX_COUNT];
#endif  // SFTE_FONT_LIGATURES
    sfte_font_backend_info info[SFTE_FONT_MAX_COUNT];
    uint8_t *ttf_buf[SFTE_FONT_MAX_COUNT];
    uint8_t *atlas_pxs;
    sfte_glyph *glyphs;

    float scales[SFTE_FONT_MAX_COUNT];

    int16_t atlas_x;
    int16_t atlas_y;
    int16_t atlas_row_h;

    uint8_t owns_ttf_buf[SFTE_FONT_MAX_COUNT];  // 1 if sfte allocated it via fopen, 0 if user
                                                // provided it
    int8_t num_fonts;
} sfte_font_cache;

/*
    Core terminal emulation state machine and grid bounds.
*/
typedef struct {
    sfte_cell *cells;
    uint8_t *tab_stops;
#if SFTE_FONT_LIGATURES
    uint16_t *render_shaper_ids;
    uint64_t *row_hashes;
    uint16_t *row_shaper_ids;
#endif  // SFTE_FONT_LIGATURES
    uint16_t *render_ids;
    uint8_t *render_font_indices;
    sfte_font_cache **render_target_caches;
    char *osc_payload;
    size_t osc_len;
    size_t osc_cap;
#if SFTE_TERM_SCROLLBACK_CAP
    sfte_cell *scrollback;  // Ring buffer storing history
#endif                      // SFTE_TERM_SCROLLBACK_CAP
#if SFTE_TERM_ALT_SCREEN
    sfte_cell *alt_cells;
#endif  // SFTE_TERM_ALT_SCREEN
#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    sfte_img *img_pool;
    sfte_img_placement *img_placements;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
#if SFTE_INPUT_HYPERLINKS
    char **link_pool;
#endif  // SFTE_INPUT_HYPERLINKS
#if SFTE_CURSOR_BLINK
    uint64_t next_blink_ms;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    uint64_t last_move_ms;
    uint64_t last_trail_update_ms;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
    uint64_t anim_start_ms;
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
#if SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP
    uint64_t last_scroll_ms;
#endif  // SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP

    uint32_t saved_fg[2];  // 0=main, 1=alt
    uint32_t saved_bg[2];  // 0=main, 1=alt
    sfte_color cur_fg, cur_bg;
#if !SFTE_TERM_ASCII_CHARSET
    uint32_t utf8_rune_acc;  // Accumulator for incoming multi-byte UTF-8 streams
#endif                       // !SFTE_TERM_ASCII_CHARSET
#if SFTE_CURSOR_TRAIL
    float tail_rx, tail_ry;
    sfte_damage_rect trail_dmg;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_TERM_SCROLLBACK_CAP
    int32_t sb_cap;
    int32_t sb_len;
    int32_t sb_offset;  // 0 = live, >0 = history
    int32_t sb_head;
#endif  // SFTE_TERM_SCROLLBACK_CAP
#if SFTE_UNDERLINE_COLORED
    sfte_color cur_ul_color;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    uint32_t img_pool_cap;
    uint32_t img_pool_len;
    uint32_t img_placements_cap;
    uint32_t img_placements_len;
    uint32_t next_img_id;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    uint32_t cursor_color;
#if SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP
    int32_t last_sb_offset;
    float scroll_y_offset;
#endif  // SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP

    int16_t cols;
    int16_t rows;
    int16_t saved_grid_off[2];  // 0=main, 1=alt
    int16_t saved_col[2];       // 0=main, 1=alt
    int16_t saved_row[2];       // 0=main, 1=alt
    int16_t last_drawn_col;
    int16_t last_drawn_row;
    int16_t cursor_col;
    int16_t cursor_row;
    int16_t scroll_top;
    int16_t scroll_bot;
    int16_t grid_off;
    uint16_t cur_attr;
    uint16_t parser_state;
    uint16_t vt_params[16];  // Stores numbers from escape sequences
#if SFTE_CURSOR_TRAIL
    int16_t last_grid_col, last_grid_row;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_INPUT_MOUSE
    int16_t mouse_hover_col, mouse_hover_row;
    uint16_t mouse_mode;  // 0=off, 1000=normal, 1002=button-event, 1003=any-event
    uint16_t mouse_ext;   // 0=off, 1006=SGR
#if SFTE_INPUT_SELECTION
    int16_t mouse_sel_start_col, mouse_sel_start_row;  // abs grid coords
    int16_t mouse_sel_end_col, mouse_sel_end_row;
#endif  // SFTE_INPUT_SELECTION
#endif  // SFTE_INPUT_MOUSE
#if SFTE_INPUT_KITTY
    uint16_t kitty_kb_stack[2][16];  // 0=main, 1=alt
#endif                               // SFTE_INPUT_KITTY
#if SFTE_INPUT_HYPERLINKS
    uint16_t link_pool_len;  // Amount of stored links
    uint16_t link_pool_cap;  // Allocated capacity
    uint16_t cur_link_idx;   // Active OSC8 link for new text
#endif                       // SFTE_INPUT_HYPERLINKS

    char title[256];
    char saved_title[256];
    uint8_t saved_attr[2];  // 0=main, 1=alt
    uint8_t auto_wrap;
    uint8_t origin_mode;
    uint8_t hide_cursor;
    uint8_t vt_param_idx;
    uint8_t vt_dec_priv;      // Tracks if the sequence starts with a '?'
    uint8_t bracketed_paste;  // Tracks \033[?2004h
    uint8_t utf8_bytes_left;
#if SFTE_CURSOR_BLINK
    uint8_t blink_enabled;  // Toggle used by DECSCUSR
    uint8_t blink_visible;  // Defining whether cursor is CURRENTLY visible
#endif                      // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    uint8_t is_trailing;
    uint8_t warp_tail;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_CURSOR_DYNAMIC
    uint8_t cursor_style;  // Block/underline/bar
#endif                     // SFTE_CURSOR_DYNAMIC
#if SFTE_TERM_ALT_SCREEN
    uint8_t alt_active;  // Tracks if currently is in a alt buffer
#endif                   // SFTE_TERM_ALT_SCREEN
#if SFTE_INPUT_MOUSE
    uint8_t mouse_btn_state;  // 0=LMB, 1=MMB, 2=RMB, 3=none
#if SFTE_INPUT_SELECTION
    uint8_t mouse_sel_active;    // 1 if has selection
    uint8_t mouse_sel_dragging;  // 1 if lmb is held down
#endif                           // SFTE_INPUT_SELECTION
#endif                           // SFTE_INPUT_MOUSE
#if SFTE_INPUT_KITTY
    uint8_t kitty_kb_stack_idx[2];
#endif  // SFTE_INPUT_KITTY
#if SFTE_UNDERLINE_EXTENDED
    uint8_t cur_ul_style;
#endif  // SFTE_UNDERLINE_EXTENDED
#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
    uint8_t is_animating;
    uint8_t anim_dir;  // 1=entering alt screen, -1=leaving
#endif                 // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
#if SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP
    uint8_t is_scrolling;
#endif  // SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP
#if SFTE_TERM_FOCUS
    uint8_t is_focused;
    uint8_t report_focus;
#endif  // SFTE_TERM_FOCUS
} sfte_term;

/*
    Central typography metrics and caching.
*/
typedef struct {
    sfte_font_cache regular;
#ifdef SFTE_FONT_BOLD
    sfte_font_cache bold;
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    sfte_font_cache italic;
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    sfte_font_cache bold_italic;
#endif               // SFTE_FONT_BOLD_ITALIC
    float cur_size;  // Starts at SFTE_FONT_DEFAULT_SIZE

    int16_t cell_width;   // Width of a single monospace character
    int16_t cell_height;  // Height of a single monospace character
    int16_t ascent;       // Distance from cell top to the baseline
    int16_t descent;      // Distance from baseline to cell bottom
    int16_t line_gap;     // Recommended empty space between lines
} sfte_font;

/*
    Main context for the emulator core.
*/
struct sfte_ctx {
    sfte_term term;
    sfte_stack stack;
    sfte_font font;
#ifndef SFTE_NO_LOGGING
    sfte_logger logger;
#endif  // !SFTE_NO_LOGGING
#if SFTE_IMG_SIXEL
    sfte_sixel_state sixel;
#endif  // SFTE_IMG_SIXEL
#if SFTE_IMG_KITTY
    sfte_kitty_state kitty;
#endif  // SFTE_IMG_KITTY

    sfte_write_cb write_cb;
    sfte_bell_cb bell_cb;
#if SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52
    sfte_osc52_clipboard_cb osc52_clipboard_cb;
#endif  // SFTE_CLIPBOARD_OSC52
#if SFTE_INPUT_HYPERLINKS
    sfte_open_link_cb open_link_cb;
#endif  // SFTE_INPUT_HYPERLINKS
    sfte_title_cb title_cb;
    sfte_progress_cb progress_cb;
    void *user_data;

    int32_t width;
    int32_t height;
    uint8_t padding_dirty;
};

#if SFTE_WAYLAND
struct sfte_wayland_app {
    sfte_ctx *ctx;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct wl_shm *shm;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    struct xkb_context *xkb_context;
    struct xkb_keymap *xkb_keymap;
    struct xkb_state *xkb_state;
#if SFTE_INPUT_SELECTION
    struct wl_pointer *pointer;
#endif  // SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD
    struct wl_data_device_manager *data_device_manager;
    struct wl_data_device *data_device;
    struct wl_data_source *data_source;
    struct wl_data_offer *data_offer;
    char *selection_text;
#endif  // SFTE_CLIPBOARD
#if SFTE_INPUT_SELECTION || SFTE_CLIPBOARD
    uint32_t serial;
#endif  // SFTE_INPUT_SELECTION || SFTE_CLIPBOARD
    struct xdg_wm_base *xdg_wm_base;
    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct wl_buffer *buffer;
    uint32_t *shm_data;
    size_t shm_size;
#if SFTE_TERM_DOUBLE_BUFFER
    uint32_t *back_buffer;
#endif  // SFTE_TERM_DOUBLE_BUFFER

    int32_t width, height;
    int32_t pending_width, pending_height;
    int32_t pty_fd;  // Master file descriptor to read/write from
    pid_t pty_pid;   // PID of shell

    int32_t repeat_timer_fd;
    int32_t repeat_rate;
    int32_t repeat_delay;
    uint32_t repeating_key;

    uint8_t running;
    uint8_t needs_render;
    uint8_t frame_pending;
};
#endif  // SFTE_WAYLAND

#if SFTE_WIN32
struct sfte_win32_app {
    sfte_ctx *ctx;

    HWND hwnd;
    HINSTANCE hinstance;
    HDC mem_dc;
    HBITMAP dib;
    HBITMAP old_dib;
    uint32_t *pixels;
    HBRUSH bg_brush;

    int32_t width, height;
    int32_t pending_width, pending_height;
    uint8_t pending_resize;

    HPCON hpc;
    HANDLE pty_in_write;  // we write keystrokes here
    HANDLE pty_out_read;  // we read shell output here
    HANDLE pty_proc;      // shell process handle
    DWORD pty_pid;
    PROCESS_INFORMATION proc;
    char *shell_cmdline;

    uint8_t running;
    uint8_t needs_render;

    UINT dpi;
    float font_scale;

    WCHAR surrogate_high;
};
#endif  // SFTE_WIN32

typedef struct {
    sfte_cell *main_grid;
#if SFTE_TERM_SCROLLBACK_CAP
    sfte_cell *sb_grid;
    int32_t sb_lines;
#endif  // SFTE_TERM_SCROLLBACK_CAP
    int16_t new_col, new_row;
} _sfte_resize_buffers;

#if SFTE_TERM_REFLOW
typedef struct {
    sfte_cell *temp_rows;

    int32_t reflow_row;

    int16_t new_cols;
    int16_t reflow_col;
    int16_t new_col, new_row;
    int16_t target_old_col, target_old_row;

    uint8_t is_live;
} _sfte_reflow_state;
#endif  // SFTE_TERM_REFLOW

typedef struct {
    int32_t y_off;
    sfte_cell *grid;
    uint8_t hide_cursor;
} _sfte_pass_info;

// =================================================================================================
// >>internal api
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// >macros/general
// -------------------------------------------------------------------------------------------------

static const sfte_shortcut _sfte_shortcuts[] = SFTE_SHORTCUTS;

// clang-format off
static const uint32_t _sfte_palette_256[256] = {
    // ----------------------  0-15 standard ANSI palette  ------------------------
    SFTE_COLOR_ANSI_PALETTE,
    // ---------------------  16-231 6x6x6 RGB color cube  ------------------------
    0x000000, 0x00005F, 0x000087, 0x0000AF, 0x0000D7, 0x0000FF, 0x005F00, 0x005F5F,
    0x005F87, 0x005FAF, 0x005FD7, 0x005FFF, 0x008700, 0x00875F, 0x008787, 0x0087AF,
    0x0087D7, 0x0087FF, 0x00AF00, 0x00AF5F, 0x00AF87, 0x00AFAF, 0x00AFD7, 0x00AFFF,
    0x00D700, 0x00D75F, 0x00D787, 0x00D7AF, 0x00D7D7, 0x00D7FF, 0x00FF00, 0x00FF5F,
    0x00FF87, 0x00FFAF, 0x00FFD7, 0x00FFFF, 0x5F0000, 0x5F005F, 0x5F0087, 0x5F00AF,
    0x5F00D7, 0x5F00FF, 0x5F5F00, 0x5F5F5F, 0x5F5F87, 0x5F5FAF, 0x5F5FD7, 0x5F5FFF,
    0x5F8700, 0x5F875F, 0x5F8787, 0x5F87AF, 0x5F87D7, 0x5F87FF, 0x5FAF00, 0x5FAF5F,
    0x5FAF87, 0x5FAFAF, 0x5FAFD7, 0x5FAFFF, 0x5FD700, 0x5FD75F, 0x5FD787, 0x5FD7AF,
    0x5FD7D7, 0x5FD7FF, 0x5FFF00, 0x5FFF5F, 0x5FFF87, 0x5FFFAF, 0x5FFFD7, 0x5FFFFF,
    0x870000, 0x87005F, 0x870087, 0x8700AF, 0x8700D7, 0x8700FF, 0x875F00, 0x875F5F,
    0x875F87, 0x875FAF, 0x875FD7, 0x875FFF, 0x878700, 0x87875F, 0x878787, 0x8787AF,
    0x8787D7, 0x8787FF, 0x87AF00, 0x87AF5F, 0x87AF87, 0x87AFAF, 0x87AFD7, 0x87AFFF,
    0x87D700, 0x87D75F, 0x87D787, 0x87D7AF, 0x87D7D7, 0x87D7FF, 0x87FF00, 0x87FF5F,
    0x87FF87, 0x87FFAF, 0x87FFD7, 0x87FFFF, 0xAF0000, 0xAF005F, 0xAF0087, 0xAF00AF,
    0xAF00D7, 0xAF00FF, 0xAF5F00, 0xAF5F5F, 0xAF5F87, 0xAF5FAF, 0xAF5FD7, 0xAF5FFF,
    0xAF8700, 0xAF875F, 0xAF8787, 0xAF87AF, 0xAF87D7, 0xAF87FF, 0xAFAF00, 0xAFAF5F,
    0xAFAF87, 0xAFAFAF, 0xAFAFD7, 0xAFAFFF, 0xAFD700, 0xAFD75F, 0xAFD787, 0xAFD7AF,
    0xAFD7D7, 0xAFD7FF, 0xAFFF00, 0xAFFF5F, 0xAFFF87, 0xAFFFAF, 0xAFFFD7, 0xAFFFFF,
    0xD70000, 0xD7005F, 0xD70087, 0xD700AF, 0xD700D7, 0xD700FF, 0xD75F00, 0xD75F5F,
    0xD75F87, 0xD75FAF, 0xD75FD7, 0xD75FFF, 0xD78700, 0xD7875F, 0xD78787, 0xD787AF,
    0xD787D7, 0xD787FF, 0xD7AF00, 0xD7AF5F, 0xD7AF87, 0xD7AFAF, 0xD7AFD7, 0xD7AFFF,
    0xD7D700, 0xD7D75F, 0xD7D787, 0xD7D7AF, 0xD7D7D7, 0xD7D7FF, 0xD7FF00, 0xD7FF5F,
    0xD7FF87, 0xD7FFAF, 0xD7FFD7, 0xD7FFFF, 0xFF0000, 0xFF005F, 0xFF0087, 0xFF00AF,
    0xFF00D7, 0xFF00FF, 0xFF5F00, 0xFF5F5F, 0xFF5F87, 0xFF5FAF, 0xFF5FD7, 0xFF5FFF,
    0xFF8700, 0xFF875F, 0xFF8787, 0xFF87AF, 0xFF87D7, 0xFF87FF, 0xFFAF00, 0xFFAF5F,
    0xFFAF87, 0xFFAFAF, 0xFFAFD7, 0xFFAFFF, 0xFFD700, 0xFFD75F, 0xFFD787, 0xFFD7AF,
    0xFFD7D7, 0xFFD7FF, 0xFFFF00, 0xFFFF5F, 0xFFFF87, 0xFFFFAF, 0xFFFFD7, 0xFFFFFF,
    // -------------------------  232-255 grayscale ramp  -------------------------
    0x080808, 0x121212, 0x1C1C1C, 0x262626, 0x303030, 0x3A3A3A, 0x444444, 0x4E4E4E,
    0x585858, 0x626262, 0x6C6C6C, 0x767676, 0x808080, 0x8A8A8A, 0x949494, 0x9E9E9E,
    0xA8A8A8, 0xB2B2B2, 0xBCBCBC, 0xC6C6C6, 0xD0D0D0, 0xDADADA, 0xE4E4E4, 0xEEEEEE,
};
// clang-format on
static const float _sfte_font_scales[SFTE_FONT_MAX_COUNT] = SFTE_FONT_SCALES;

typedef enum sfte_underline_style {
    _SFTE_UNDERLINE_STYLE_STRAIGHT = 1,
    _SFTE_UNDERLINE_STYLE_DOUBLE = 2,
    _SFTE_UNDERLINE_STYLE_CURLY = 3,
    _SFTE_UNDERLINE_STYLE_DOTTED = 4,
    _SFTE_UNDERLINE_STYLE_DASHED = 5,
} _sfte_underline_style;

// -------------------------------------------------------------------------------------------------
// >mem
// -------------------------------------------------------------------------------------------------
static inline void _sfte_mem_stack_init(sfte_stack *stack, void *back_buf, size_t cap);
static inline void *_sfte_mem_stack_alloc(sfte_stack *stack, size_t size, size_t align);
static inline void _sfte_mem_stack_rewind(sfte_stack *stack, size_t off);
static inline size_t _sfte_mem_stack_save(sfte_stack *stack);

// -------------------------------------------------------------------------------------------------
// >log
// -------------------------------------------------------------------------------------------------
#ifndef SFTE_NO_LOGGING
static inline void _sfte_log_default_func(const char *tag, sfte_log_level log_level,
                                          const char *msg, uint32_t line_nr);
static inline void _sfte_log(sfte_ctx *ctx, _sfte_log_item log_item, sfte_log_level log_level,
                             uint32_t line_nr, ...);
#endif  // !SFTE_NO_LOGGING

// -------------------------------------------------------------------------------------------------
// >b64
// -------------------------------------------------------------------------------------------------
#if (SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52) || SFTE_IMG_KITTY
static inline uint8_t *_sfte_b64_decode(sfte_stack *stack, uint8_t *src, size_t len,
                                        size_t *out_len);
#endif  // (SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52) || SFTE_IMG_KITTY

// -------------------------------------------------------------------------------------------------
// >rune
// -------------------------------------------------------------------------------------------------
static inline void _sfte_rune_stamp_cell(sfte_ctx *ctx, uint32_t idx, sfte_rune rune,
                                         uint8_t extra_attr);
static inline void _sfte_rune_insert(sfte_ctx *ctx, sfte_rune rune);
static inline uint8_t _sfte_rune_utf8_decode(sfte_ctx *ctx, uint8_t b);

// -------------------------------------------------------------------------------------------------
// >color
// -------------------------------------------------------------------------------------------------
#if !SFTE_COLOR_TRUECOLOR
static inline uint16_t _sfte_color_rgb_to_256(uint8_t r, uint8_t g, uint8_t b);
#endif  // !SFTE_COLOR_TRUECOLOR
static inline uint32_t _sfte_color_from_idx(uint16_t idx);
static inline uint32_t _sfte_color_from_rgb(uint32_t rgb);

// -------------------------------------------------------------------------------------------------
// >grid
// -------------------------------------------------------------------------------------------------
static inline int32_t _sfte_grid_vis2log(sfte_ctx *ctx, int16_t visual_row);
static inline int32_t _sfte_grid_log2vis(sfte_ctx *ctx, int32_t logical_row);
static inline uint32_t _sfte_grid_get_idx(sfte_ctx *ctx, int16_t c, int32_t logical_row);
static inline sfte_cell *_sfte_grid_get_cell(sfte_ctx *ctx, int16_t col, int32_t logical_row);
static inline uint32_t _sfte_grid_get_bg(sfte_cell *cell);
static inline uint32_t _sfte_grid_get_fg(sfte_cell *cell);
static inline uint32_t _sfte_grid_get_ul(sfte_cell *cell);
#if SFTE_CURSOR_TRAIL || SFTE_INPUT_MOUSE
static inline void _sfte_grid_from_px(sfte_ctx *ctx, int32_t px_x, int32_t px_y, int16_t *out_col,
                                      int32_t *out_logical_row, int16_t *out_screen_row);
#endif  // SFTE_CURSOR_TRAIL || SFTE_INPUT_MOUSE
static inline void _sfte_grid_dirty_rows(sfte_ctx *ctx, int32_t logical_row1, int32_t logical_row2);
static inline void _sfte_grid_dirty_rect(sfte_ctx *ctx, int16_t start_col,
                                         int32_t start_logical_row, int16_t cols, int16_t rows);
static inline void _sfte_grid_dirty_range(sfte_ctx *ctx, uint32_t start_idx, uint32_t cnt);
#if SFTE_CURSOR_TRAIL
static inline void _sfte_grid_dirty_trail(sfte_ctx *ctx);
#endif  // SFTE_CURSOR_TRAIL
static inline int16_t _sfte_grid_span(int32_t px_len, int32_t px_off, int32_t cell_px);
#if SFTE_IMG_SIXEL
static inline void _sfte_grid_clear_sixel(sfte_ctx *ctx, int32_t start_idx, int32_t cnt);
#endif  // SFTE_IMG_SIXEL
#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
static inline void _sfte_grid_scroll_images(sfte_ctx *ctx, int16_t lines, int16_t top, int16_t bot);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
#if SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_grid_push_scrollback(sfte_ctx *ctx, int16_t lines);
#endif  // SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_grid_clear_cells(sfte_ctx *ctx, uint32_t start_idx, uint32_t cnt);
static inline void _sfte_grid_clear_rows(sfte_ctx *ctx, int16_t start_row, int16_t cnt);
static inline void _sfte_grid_scroll(sfte_ctx *ctx, int16_t lines);
static inline void _sfte_grid_check_wrap(sfte_ctx *ctx);
#if SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW
static sfte_cell *_sfte_grid_resize_dumb_copy(sfte_cell *old_grid, int16_t old_cols,
                                              int16_t old_rows, int16_t new_cols, int16_t new_rows);
#endif  // SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW
static inline void _sfte_grid_resize_tabs(sfte_ctx *ctx, int16_t old_cols, int16_t new_cols);
static inline void _sfte_grid_resize(sfte_ctx *ctx, int16_t new_cols, int16_t new_rows);

// -------------------------------------------------------------------------------------------------
// >img
// -------------------------------------------------------------------------------------------------
#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
static inline sfte_img_placement *_sfte_img_placement_insert(sfte_ctx *ctx, sfte_img_placement p);
static inline sfte_img *_sfte_img_find(sfte_ctx *ctx, uint32_t id);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

// -------------------------------------------------------------------------------------------------
// >view
// -------------------------------------------------------------------------------------------------
#if SFTE_WINDOW_PAD_X || SFTE_WINDOW_PAD_Y
static inline void _sfte_view_clear_padding_rects(sfte_ctx *ctx, void *px_buf);
#endif  // SFTE_WINDOW_PAD_X || SFTE_WINDOW_PAD_Y

// -------------------------------------------------------------------------------------------------
// >input
// -------------------------------------------------------------------------------------------------
#if SFTE_INPUT_SELECTION
static inline uint8_t _sfte_input_is_selected(sfte_ctx *ctx, int16_t col, int16_t row);
#endif  // SFTE_INPUT_SELECTION
#if SFTE_INPUT_MOUSE
static inline void _sfte_input_send_mouse_event(sfte_ctx *ctx, uint8_t btn, uint8_t is_release,
                                                int16_t col, int16_t row, uint8_t is_motion);
#endif  // SFTE_INPUT_MOUSE

// -------------------------------------------------------------------------------------------------
// >reflow
// -------------------------------------------------------------------------------------------------
#if SFTE_TERM_REFLOW
static inline void _sfte_reflow_push(_sfte_reflow_state *st, sfte_cell cell, uint8_t is_cursor);
static inline int16_t _sfte_reflow_get_len(sfte_cell *row, int16_t cols, int16_t cursor_cx);
static inline void _sfte_reflow_process_row(sfte_cell *row, int16_t cols, int16_t cursor_cx,
                                            _sfte_reflow_state *st);
static inline void _sfte_reflow_grid_into_linear(sfte_ctx *ctx, sfte_cell *main_old,
                                                 int16_t grid_off_old, _sfte_reflow_state *st);
static inline sfte_cell *_sfte_reflow_linearize(sfte_ctx *ctx, sfte_cell *main_old,
                                                int16_t grid_off_old, int16_t new_cols,
                                                int16_t new_rows, _sfte_reflow_state *st);
static inline void _sfte_reflow_extract_view(sfte_ctx *ctx, int16_t new_cols, int16_t new_rows,
                                             int16_t target_cy, _sfte_reflow_state *st,
                                             _sfte_resize_buffers *out);
static inline _sfte_resize_buffers _sfte_reflow_generate_buffers(sfte_ctx *ctx, sfte_cell *main_old,
                                                                 int16_t grid_off_old,
                                                                 int16_t new_cols, int16_t new_rows,
                                                                 int16_t target_cx,
                                                                 int16_t target_cy);
#endif  // SFTE_TERM_REFLOW

// -------------------------------------------------------------------------------------------------
// >sixel
// -------------------------------------------------------------------------------------------------
#if SFTE_IMG_SIXEL
static inline void _sfte_sixel_commit(sfte_ctx *ctx);
static inline void _sfte_sixel_ensure_cap(sfte_ctx *ctx, int32_t req_w, int32_t req_h);
static inline void _sfte_sixel_draw_pattern(sfte_ctx *ctx, uint8_t pattern, int32_t repeats);
static inline float _sfte_sixel_hue_to_rgb(float p, float q, float t);
static inline uint32_t _sfte_sixel_hls_to_rgb(uint16_t h_deg, uint16_t l_pct, uint16_t s_pct);
static inline void _sfte_sixel_apply_color(sfte_ctx *ctx);
static inline void _sfte_sixel_parse_byte(sfte_ctx *ctx, uint8_t b);
static inline void _sfte_sixel_deinit(sfte_ctx *ctx);
#endif  // SFTE_IMG_SIXEL

// -------------------------------------------------------------------------------------------------
// >kitty
// -------------------------------------------------------------------------------------------------
#if SFTE_INPUT_KITTY
static inline size_t _sfte_kitty_kb_encode(sfte_ctx *ctx, sfte_key key, uint32_t codepoint,
                                           uint32_t mod_mask, char *out_buf, size_t max_bytes);
#endif  // SFTE_INPUT_KITTY
#if SFTE_IMG_KITTY
static inline uint32_t *_sfte_kitty_scale_image_bilinear(sfte_stack *stack, uint32_t *src,
                                                         int32_t src_wid, int32_t src_hei,
                                                         int32_t dst_wid, int32_t dst_hei);
static inline uint32_t *_sfte_kitty_decode_payload(sfte_ctx *ctx, uint8_t *raw_data, size_t raw_len,
                                                   uint8_t is_file, const char *file_path,
                                                   int32_t *w, int32_t *h);
static inline uint32_t *_sfte_kitty_apply_crop(sfte_ctx *ctx, size_t pre_pxs_off, uint32_t *pxs,
                                               int32_t *w, int32_t *h);
static inline uint32_t *_sfte_kitty_apply_scale(sfte_ctx *ctx, uint32_t *pxs, int32_t *w,
                                                int32_t *h);
static inline uint8_t _sfte_kitty_should_delete(sfte_ctx *ctx, sfte_img_placement *p,
                                                sfte_img *img);
static inline void _sfte_kitty_gc_pool(sfte_ctx *ctx);
static inline const char *_sfte_kitty_apply_placement(sfte_ctx *ctx, sfte_img *img);
static inline const char *_sfte_kitty_exec_query(sfte_ctx *ctx);
static inline const char *_sfte_kitty_exec_delete(sfte_ctx *ctx);
static inline const char *_sfte_kitty_exec_transmit(sfte_ctx *ctx, sfte_img **out_img);
static inline const char *_sfte_kitty_exec_place(sfte_ctx *ctx);
static inline void _sfte_kitty_send_ack(sfte_ctx *ctx, const char *err_msg);
static inline void _sfte_kitty_parse_graphics(sfte_ctx *ctx, const char *payload);
static inline void _sfte_kitty_deinit(sfte_ctx *ctx);
#endif  // SFTE_IMG_KITTY

// -------------------------------------------------------------------------------------------------
// >csi
// -------------------------------------------------------------------------------------------------
static inline uint32_t _sfte_csi_parse_truecolor(uint16_t *p, uint16_t i);
static inline void _sfte_csi_exec_ich(sfte_ctx *ctx, uint16_t *p, int16_t col);
static inline void _sfte_csi_exec_cnl(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_cpl(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_ed(sfte_ctx *ctx, int16_t mode, int16_t col);
static inline void _sfte_csi_exec_el(sfte_ctx *ctx, int16_t mode, int16_t col);
static inline void _sfte_csi_exec_il(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_dl(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_dch(sfte_ctx *ctx, uint16_t *p, int16_t col);
static inline void _sfte_csi_exec_ech(sfte_ctx *ctx, uint16_t *p, int16_t col);
static inline void _sfte_csi_exec_da(sfte_ctx *ctx);
static inline void _sfte_csi_exec_vpa(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_hvp(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_tbc(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_set_mode(sfte_ctx *ctx, uint16_t *p, uint16_t cnt, int16_t col);
static inline void _sfte_csi_reset_mode(sfte_ctx *ctx, uint16_t *p, uint16_t cnt, int16_t col);
static inline void _sfte_csi_exec_sgr(sfte_ctx *ctx, uint16_t *p, uint16_t cnt);
static inline void _sfte_csi_exec_dsr(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_decstr(sfte_ctx *ctx, int16_t col);
static inline void _sfte_csi_exec_decscusr(sfte_ctx *ctx, uint16_t *p, int16_t col);
static inline void _sfte_csi_exec_decstbm(sfte_ctx *ctx, uint16_t *p, uint16_t cnt);
static inline void _sfte_csi_exec_scosc(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_xtwinops(sfte_ctx *ctx, uint16_t *p);
static inline void _sfte_csi_exec_scorc(sfte_ctx *ctx, uint16_t *p);
#if SFTE_INPUT_KITTY
static inline void _sfte_csi_exec_kitty(sfte_ctx *ctx, uint16_t *p);
#endif  // SFTE_INPUT_KITTY
static inline void _sfte_csi_dispatch(sfte_ctx *ctx, uint8_t cmd);

// -------------------------------------------------------------------------------------------------
// >parser
// -------------------------------------------------------------------------------------------------
#if SFTE_CURSOR_DYNAMIC
static inline uint32_t _sfte_parser_osc_color(const char *str, uint32_t fallback);
#endif  // SFTE_CURSOR_DYNAMIC
static inline void _sfte_parser_append_payload(sfte_ctx *ctx, uint8_t b);
static inline void _sfte_parser_c0_lf(sfte_ctx *ctx);
static inline void _sfte_parser_c0_ht(sfte_ctx *ctx);
static inline void _sfte_parser_esc_ris(sfte_ctx *ctx);
static inline void _sfte_parser_esc_sc(sfte_ctx *ctx);
static inline void _sfte_parser_esc_rc(sfte_ctx *ctx);
static inline void _sfte_parser_esc_ind(sfte_ctx *ctx);
static inline void _sfte_parser_esc_ri(sfte_ctx *ctx);
static inline void _sfte_parser_esc_nel(sfte_ctx *ctx);
static inline void _sfte_parser_hash_decaln(sfte_ctx *ctx);
static inline void _sfte_parser_osc_dispatch(sfte_ctx *ctx, uint8_t terminator);
static inline void _sfte_parser_dcs_dispatch(sfte_ctx *ctx, uint8_t terminator);
static inline void _sfte_parser_feed_byte(sfte_ctx *ctx, uint8_t b);

// -------------------------------------------------------------------------------------------------
// >shaper
// -------------------------------------------------------------------------------------------------
#if SFTE_FONT_LIGATURES
static inline uint16_t _sfte_shaper_consume16(const uint8_t *ttf_data, uint32_t *off);
static inline uint32_t _sfte_shaper_consume32(const uint8_t *ttf_data, uint32_t *off);
static inline uint32_t _sfte_shaper_get_font_table_off(const uint8_t *ttf_data, const char tag[4]);
static inline void _sfte_shaper_load_feature(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                             const char tag[4], sfte_shaper_feature *out_feat);
static inline void _sfte_shaper_parse_subtables(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint16_t lookup_idx, uint32_t lookup_tab_off,
                                                uint16_t lookup_type, uint16_t subtable_cnt);
static inline void _sfte_shaper_parse_lookups(sfte_shaper_ctx *ctx, const uint8_t *ttf_data);
static inline void _sfte_shaper_init(sfte_shaper_ctx *ctx, const uint8_t *ttf_data);
static inline int32_t _sfte_shaper_get_coverage_index(const uint8_t *ttf_data, uint32_t cov_off,
                                                      uint16_t glyph_id);
static inline uint16_t _sfte_shaper_get_class(const uint8_t *ttf_data, uint32_t class_off,
                                              uint16_t glyph_id);
static inline uint16_t _sfte_shaper_get_type1_subst(const uint8_t *ttf_data, uint32_t t1_off,
                                                    uint16_t glyph_id);
static inline uint8_t _sfte_shaper_match_rule_format1(const uint8_t *ttf_data, uint32_t rule_off,
                                                      uint16_t *grid, size_t grid_len, size_t pos,
                                                      sfte_shaper_subst_record *out_records,
                                                      uint16_t *out_cnt);
static inline void _sfte_shaper_apply_subst(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                            uint16_t *grid, size_t pos,
                                            const sfte_shaper_subst_record *records,
                                            uint16_t records_cnt);
static inline uint8_t _sfte_shaper_eval_format1(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint32_t sub_off, uint16_t cur_glyph,
                                                uint16_t *grid, size_t grid_len, size_t pos);
static inline uint8_t _sfte_shaper_eval_format3(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint32_t sub_off, uint16_t *grid, size_t grid_len,
                                                size_t pos);
static inline void _sfte_shaper_shape_row(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                          uint16_t *grid, size_t grid_len);
#endif  // SFTE_FONT_LIGATURES
// -------------------------------------------------------------------------------------------------
// >font
// -------------------------------------------------------------------------------------------------
#ifndef SFTE_FONT_CUSTOM_BACKEND
#define SFTE_FONT_INIT _sfte_stb_init
#define SFTE_FONT_GET_SCALE _sfte_stb_get_scale
#define SFTE_FONT_VMETRICS _sfte_stb_vmetrics
#define SFTE_FONT_BOUNDS _sfte_stb_bounds
#define SFTE_FONT_BAKE _sfte_stb_bake
#define SFTE_FONT_GET_ID _sfte_stb_get_id
#endif  // !SFTE_FONT_CUSTOM_BACKEND
static inline sfte_font_cache *_sfte_font_get_cache(sfte_ctx *ctx, sfte_font_style style);
static inline void _sfte_font_clear_cache(sfte_font_cache *cache);
static inline void _sfte_font_update_scales(sfte_ctx *ctx, sfte_font_cache *cache);
static inline void _sfte_font_pack_and_bake(sfte_ctx *ctx, sfte_font_cache *cache, sfte_glyph *g,
                                            int32_t font_idx, int32_t glyph_id, int32_t gw,
                                            int32_t gh);
static inline sfte_glyph *_sfte_font_get_glyph(sfte_ctx *ctx, sfte_font_cache *cache,
                                               uint16_t glyph_id, uint8_t font_idx);
static inline void _sfte_font_resolve_rune(sfte_font_cache *cache, sfte_rune rune,
                                           uint8_t *out_font_idx, uint16_t *out_glyph_id);
static inline void _sfte_font_reset_cache(sfte_ctx *ctx);

// -------------------------------------------------------------------------------------------------
// >render
// -------------------------------------------------------------------------------------------------
static inline void _sfte_render_damage_add(sfte_damage_rect *dmg, int32_t x, int32_t y, int32_t w,
                                           int32_t h);
static inline void _sfte_render_propagate_damage(sfte_ctx *ctx, int16_t vis_col, int16_t vis_row);
#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
static inline void _sfte_render_sort_images(sfte_ctx *ctx);
static inline void _sfte_render_images(sfte_ctx *ctx, void *px_buf, uint8_t is_bg_pass,
                                       int32_t base_y_off, uint8_t pad_was_dirty);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
#if SFTE_CURSOR_TRAIL
static inline void _sfte_render_trail(sfte_ctx *ctx, void *px_buf, sfte_damage_rect *out_dmg);
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
static inline uint8_t _sfte_render_get_anim_offsets(sfte_ctx *ctx, int32_t *out_y, int32_t *in_y);
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
static inline uint8_t _sfte_render_prepare_passes(sfte_ctx *ctx, void *px_buf,
                                                  _sfte_pass_info *passes,
                                                  sfte_damage_rect *out_dmg);
static inline uint32_t _sfte_render_blend_argb(uint32_t dst, uint32_t src_col, uint8_t src_a);
static inline void _sfte_render_bg_cell(sfte_ctx *ctx, void *px_buf, int16_t col, int16_t row,
                                        int32_t y_off, uint32_t bg);
static inline void _sfte_render_fg_cell(sfte_ctx *ctx, void *px_buf, int16_t col, int16_t row,
                                        int32_t y_off, sfte_rune rune, uint16_t glyph_id,
                                        uint8_t font_idx, uint32_t fg,
                                        sfte_font_cache *target_cache);
static inline void _sfte_render_underline_cell(sfte_ctx *ctx, void *px_buf, int32_t cx, int32_t cy,
                                               int32_t render_w, sfte_cell *vcell);
static inline void _sfte_render_cursor_shape(sfte_ctx *ctx, void *px_buf, int32_t cx, int32_t cy,
                                             int32_t render_w);
#if SFTE_TERM_CUSTOM_BOXES && !SFTE_TERM_ASCII_CHARSET
static inline void _sfte_render_line(sfte_ctx *ctx, void *px_buf, int32_t x0, int32_t y0,
                                     int32_t x1, int32_t y1, int32_t thickness, uint32_t col);
static inline uint8_t _sfte_render_box_char(sfte_ctx *ctx, void *px_buf, int32_t cx, int32_t cy,
                                            uint32_t col, uint32_t rune, int32_t y_off);
#endif  // SFTE_TERM_CUSTOM_BOXES && !SFTE_TERM_ASCII_CHARSET
static inline void _sfte_render_decorations_cell(sfte_ctx *ctx, void *px_buf, int16_t col,
                                                 int16_t row, int32_t y_off, sfte_cell *vcell,
                                                 uint8_t is_cursor);
static inline void _sfte_render_bg_grid(sfte_ctx *ctx, void *px_buf, int16_t vis_col,
                                        int16_t vis_row, int32_t y_off);
#if SFTE_FONT_LIGATURES
static inline uint64_t _sfte_render_get_row_hash(sfte_ctx *ctx, int32_t logical_row);
static inline void _sfte_render_shape_fg_row(sfte_ctx *ctx, int32_t logical_row);
#endif  // SFTE_FONT_LIGATURES
static inline void _sfte_render_extract_fg_row(sfte_ctx *ctx, int32_t logical_row, int16_t row,
                                               int16_t vis_col, int16_t vis_row);
static inline void _sfte_render_fg_row(sfte_ctx *ctx, void *px_buf, int16_t row,
                                       int32_t logical_row, int16_t vis_col, int16_t vis_row,
                                       int32_t y_off, sfte_damage_rect *out_dmg);
static inline void _sfte_render_fg_grid(sfte_ctx *ctx, void *px_buf, int16_t vis_col,
                                        int16_t vis_row, int32_t y_off, sfte_damage_rect *out_dmg);

// -------------------------------------------------------------------------------------------------
// >wayland
// -------------------------------------------------------------------------------------------------
#if SFTE_WAYLAND
static inline void _sfte_wayland_write_cb(void *user_data, const char *data, size_t len);
static inline void _sfte_wayland_pty_spawn(sfte_wayland_app *app);
static inline void _sfte_wayland_pty_update(sfte_wayland_app *app);
#if SFTE_FONT_ZOOM
static inline void _sfte_wayland_font_resize(sfte_ctx *ctx, const sfte_arg *arg);
static inline void _sfte_wayland_font_reset(sfte_ctx *ctx, const sfte_arg *dummy);
#endif  // SFTE_FONT_ZOOM
#if SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_wayland_create_buffer(sfte_wayland_app *app);
#if SFTE_INPUT_HYPERLINKS
static inline void _sfte_wayland_open_link_cb(void *user_data, const char *uri);
#endif  // SFTE_INPUT_HYPERLINKS
static inline void _sfte_wayland_load(sfte_wayland_app *app);
static inline void _sfte_wayland_unload(sfte_wayland_app *app);
#if SFTE_CLIPBOARD
#if SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD_OSC52
static inline void _sfte_wayland_osc52_clipboard_cb(void *user_data, char target, const char *data);
#endif  // SFTE_CLIPBOARD_OSC52
static inline void _sfte_wayland_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_INPUT_SELECTION
static inline void _sfte_wayland_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_CLIPBOARD
static inline void _sfte_wayland_loop(sfte_wayland_app *app);
#endif  // SFTE_WAYLAND

// -------------------------------------------------------------------------------------------------
// >win32
// -------------------------------------------------------------------------------------------------
#if SFTE_WIN32
static inline void _sfte_win32_write_cb(void *user_data, const char *data, size_t len);
static inline void _sfte_win32_title_cb(void *user_data, const char *title);
static inline void _sfte_win32_pty_spawn(sfte_win32_app *app);
static inline void _sfte_win32_pty_update(sfte_win32_app *app);
static inline void _sfte_win32_create_backbuffer(sfte_win32_app *app);
static inline void _sfte_win32_destroy_backbuffer(sfte_win32_app *app);
static inline void _sfte_win32_render(sfte_win32_app *app);
static inline void _sfte_win32_setup_dpi(sfte_win32_app *app);
static inline void _sfte_win32_apply_dpi(sfte_win32_app *app, UINT new_dpi);
static inline void _sfte_win32_register_class(sfte_win32_app *app);
static inline void _sfte_win32_handle_keydown(sfte_win32_app *app, WPARAM vk, LPARAM lparam);
static inline void _sfte_win32_handle_char(sfte_win32_app *app, WPARAM ch);
static inline uint32_t _sfte_win32_mods(void);
static inline sfte_key _sfte_win32_key_from_vk(WPARAM vk);
static inline char *_sfte_win32_resolve_shell(void);
static inline char *_sfte_win32_wide_to_utf8(const WCHAR *wide, int wide_len);
static inline WCHAR *_sfte_win32_utf8_to_wide(const char *utf8);
#if SFTE_INPUT_HYPERLINKS
static inline void _sfte_win32_open_link_cb(void *user_data, const char *uri);
#endif  // SFTE_INPUT_HYPERLINKS
#if SFTE_CLIPBOARD
static inline void _sfte_win32_clipboard_set(const char *utf8);
static inline char *_sfte_win32_clipboard_get(void);
#if SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD_OSC52
static inline void _sfte_win32_osc52_clipboard_cb(void *user_data, char target, const char *data);
#endif  // SFTE_CLIPBOARD_OSC52
static inline void _sfte_win32_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_INPUT_SELECTION
static inline void _sfte_win32_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg);
#endif  // SFTE_CLIPBOARD
static inline void _sfte_win32_loop(sfte_win32_app *app);
static inline void _sfte_win32_unload(sfte_win32_app *app);
#endif  // SFTE_WIN32

// #################################################################################################
// >>>INTERNAL IMPLEMENTATION
// #################################################################################################

// =================================================================================================
// >>mem
// =================================================================================================

/*
    Dynamically resizes an 'arr' buffer of type 'type *',
    doubling its 'cap' until the required length 'len' + 'add'
    is met, up to a specified 'max_cap' limit.
    Sets 'oom_flag' to 1 if 'max_cap' is exceeded.
    If 'cap' is 0, it initializes capacity to 'init_cap'.
*/
#define _SFTE_MEM_ENSURE_CAP(type, arr, len, cap, add, init_cap, max_cap, oom_flag)                \
    do {                                                                                           \
        size_t _req = (len) + (add);                                                               \
        if (_req > (cap)) {                                                                        \
            size_t _new = (cap) == 0 ? (init_cap) : (cap) * 2;                                     \
            while (_new < _req) _new *= 2;                                                         \
            if (_new > (max_cap))                                                                  \
                (oom_flag) = 1;                                                                    \
            else {                                                                                 \
                (cap) = _new;                                                                      \
                (arr) = (type *)SFTE_REALLOC((arr), (cap) * sizeof(type));                         \
            }                                                                                      \
        }                                                                                          \
    } while (0)

/*
    Initializes the stack allocator with a pre-allocated `back_buf`.
*/
static inline void _sfte_mem_stack_init(sfte_stack *stack, void *back_buf, size_t cap) {
    stack->buf = (uint8_t *)back_buf;
    stack->cap = cap;
    stack->off = 0;
}

/*
    Allocates `size` bytes of memory in `stack`, aligned to `align` bytes.
    `align` must be a power of 2.
*/
static inline void *_sfte_mem_stack_alloc(sfte_stack *stack, size_t size, size_t align) {
    uintptr_t cur_ptr = (uintptr_t)stack->buf + stack->off;  // Current (unaligned) memory address
    // `align` is a power of 2, in binary represented as 10...0.
    // `align - 1` hence is represented in binary as     01...1.
    // `(ptr + align - 1) & ~(align - 1)` ceils `ptr` to the closest multiple of `align`.
    // ` ... + align - 1) ...` ensures that it will ceil instead of flooring
    // `              ... & ~(align - 1)` floors `ptr` to the closest multiple of `align`.
    uintptr_t off_pad = (align - 1);
    uintptr_t align_ptr = (cur_ptr + off_pad) & ~off_pad;
    size_t new_off = (align_ptr - (uintptr_t)stack->buf) + size;
    if (new_off > stack->cap) return NULL;  // OOM
    stack->off = new_off;
    return (void *)align_ptr;
}

/*
    Rewinds the stack offset to `off`.
    If called with `off` = 0, resets the entire arena.
*/
static inline void _sfte_mem_stack_rewind(sfte_stack *stack, size_t off) {
    stack->off = off;
}

/*
    Returns the current offset of `stack`.
*/
static inline size_t _sfte_mem_stack_save(sfte_stack *stack) {
    return stack->off;
}

// =================================================================================================
// >>log
// =================================================================================================
#ifndef SFTE_NO_LOGGING

#define _SFTE_LOG_MAX_MSG_LEN 512

/*
    Default standard error logging sink.
    Outputs logs in format: ['tag':'line_nr']('log_level') 'msg'
*/
static inline void _sfte_log_default_func(const char *tag, sfte_log_level log_level,
                                          const char *msg, uint32_t line_nr) {
    const char *level_str = "???";
    switch (log_level) {
    case SFTE_LOG_LVL_PANIC: level_str = "PANIC"; break;
    case SFTE_LOG_LVL_ERROR: level_str = "ERROR"; break;
    case SFTE_LOG_LVL_WARN: level_str = "WARN"; break;
    case SFTE_LOG_LVL_INFO: level_str = "INFO"; break;
    }
    fprintf(stderr, "[%s:%d](%s) %s\n", tag, line_nr, level_str, msg);
}

/*
    Formats a log message from the X-Macro catalog passed by 'log_item' and dispatches it to the
    active sink.

    Routes to a user-provided logger if one is configured in 'ctx', otherwise falls
    back to stderr. Aborts the process if 'log_level' is PANIC.

    This function is not called directly, instead its used by macros
   (`_SFTE_PANIC/ERROR/WARN/INFO`)
*/
static inline void _sfte_log(sfte_ctx *ctx, _sfte_log_item log_item, sfte_log_level log_level,
                             uint32_t line_nr, ...) {
    if (log_level > SFTE_LOG_LEVEL) return;

    char buf[_SFTE_LOG_MAX_MSG_LEN];
    va_list args;
    va_start(args, line_nr);
    vsnprintf(buf, sizeof(buf), _sfte_log_messages[log_item], args);
    va_end(args);

    void (*log_func)(const char *, sfte_log_level, const char *,
                     uint32_t) = ctx->logger.func ? ctx->logger.func : _sfte_log_default_func;

    log_func(SFTE_LOG_TAG, log_level, buf, line_nr);

    if (log_level == SFTE_LOG_LVL_ERROR) exit(1);
    // for log level PANIC it would be 'undefined behaviour' to continue
    if (log_level == SFTE_LOG_LVL_PANIC) abort();
}

#endif  // !SFTE_NO_LOGGING
// =================================================================================================
// >>b64
// =================================================================================================
#if (SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52) || SFTE_IMG_KITTY
static const int8_t _sfte_b64_table[256] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,
    7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/*
    Decodes a Base64 payload into a stack-allocated binary buffer.
    Ignores invalid characters (spaces, newlines).
    The caller assumes ownership of the returned pointer and MUST free it.
    Returns NULL if input is empty or if memory allocation fails.
*/
static inline uint8_t *_sfte_b64_decode(sfte_stack *stack, uint8_t *src, size_t len,
                                        size_t *out_len) {
    SFTE_ASSERT(src && out_len, "b64_decode requires valid pointers");

    // Strip trailing padding
    while (len > 0 && src[len - 1] == '=') len--;

    *out_len = (len * 3) / 4;
    size_t pre_off = _sfte_mem_stack_save(stack);
    uint8_t *dst = (uint8_t *)_sfte_mem_stack_alloc(stack, *out_len, _Alignof(uint8_t));
    if (!dst) {
        _sfte_mem_stack_rewind(stack, pre_off);
        return NULL;
    }

    size_t i = 0, j = 0;
    uint32_t acc = 0;
    uint8_t bits = 0;

    while (i < len) {
        int8_t v = _sfte_b64_table[src[i++]];
        if (v == -1) continue;

        acc = (acc << 6) | (v & 0x3F);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (j < *out_len) dst[j++] = (acc >> bits) & 0xFF;
        }
    }

    // Update out_len since ignored characters might've made the overall size smaller.
    *out_len = j;
    return dst;
}
#endif  // (SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52) || SFTE_IMG_KITTY
// =================================================================================================
// >>rune
// =================================================================================================

/*
    Stamps the active terminal cursor styling onto a specific grid cell.
    Isolates all feature-toggle macros to keep call sites clean.
    Does NOT advance the cursor.
*/
static inline void _sfte_rune_stamp_cell(sfte_ctx *ctx, uint32_t idx, sfte_rune rune,
                                         uint8_t extra_attr) {
    sfte_cell *c = &ctx->term.cells[idx];
    c->rune = rune;
    c->fg = ctx->term.cur_fg;
    c->bg = ctx->term.cur_bg;
    c->attr = ctx->term.cur_attr | extra_attr;
    c->dirty = 1;

#if SFTE_INPUT_HYPERLINKS
    c->link_idx = ctx->term.cur_link_idx;
#endif  // SFTE_INPUT_HYPERLINKS
#if SFTE_UNDERLINE_EXTENDED
    c->ul_style = ctx->term.cur_ul_style;
#endif  // SFTE_UNDERLINE_EXTENDED
#if SFTE_UNDERLINE_COLORED
    c->ul_color = ctx->term.cur_ul_color;
#endif  // SFTE_UNDERLINE_COLORED
}

/*
    Inserts a decoded unicode rune into the terminal grid at the current cursor position.
    Combining characters (these of width 0) are appended to the previous logical cell.
    Double-width characters (these of width 2) if placed on the last column, the column
    is left blank, the line wraps early, and the character is placed on the next line.
*/
static inline void _sfte_rune_insert(sfte_ctx *ctx, sfte_rune rune) {
    int8_t w = _SFTE_CHAR_WIDTH(rune);
    if (w < 0) w = 1;

    if (w == 0) {
#if SFTE_FONT_WIDE_CHARS
        if (ctx->term.cursor_col > 0) {
            int32_t prev_idx = _sfte_grid_get_idx(ctx, ctx->term.cursor_col - 1,
                                                  ctx->term.cursor_row);
            sfte_cell *prev = &ctx->term.cells[prev_idx];

            if (prev->attr & _SFTE_ATTR_DUMMY && ctx->term.cursor_col > 1) {
                prev_idx = _sfte_grid_get_idx(ctx, ctx->term.cursor_col - 2, ctx->term.cursor_row);
                prev = &ctx->term.cells[prev_idx];
            }

#if SFTE_FONT_MAX_COMBINING
            if (prev->combining_cnt < SFTE_FONT_MAX_COMBINING) {
                prev->combining_runes[prev->combining_cnt++] = rune;
                prev->dirty = 1;
            }
#endif  // SFTE_FONT_MAX_COMBINING
        }
#endif  // SFTE_FONT_WIDE_CHARS
        return;
    }

    // Evaluate line wrapping before drawing,
    // ensures characters placed in the final column enter a pending wrap state.
    _sfte_grid_check_wrap(ctx);

#if SFTE_FONT_WIDE_CHARS
    if (w == 2) {
        // A wide character cannot be split across lines,
        // if in last column, leave it blank and wrap early.
        if (ctx->term.cursor_col == ctx->term.cols - 1) {
            int32_t idx = _sfte_grid_get_idx(ctx, ctx->term.cursor_col, ctx->term.cursor_row);
            _sfte_rune_stamp_cell(ctx, idx, ' ', 0);
            ctx->term.cells[idx].fg = _SFTE_COLOR_FG_DEFAULT;
            ctx->term.cells[idx].bg = _SFTE_COLOR_BG_DEFAULT;
            ctx->term.cells[idx].attr = 0;

            ctx->term.cursor_col++;
            _sfte_grid_check_wrap(ctx);
        }

        // If terminal grid has only one column, we can't draw the double-width character.
        if (ctx->term.cursor_col == ctx->term.cols - 1) return;

        // Draw the actual character.
        int32_t idx = _sfte_grid_get_idx(ctx, ctx->term.cursor_col, ctx->term.cursor_row);
        _sfte_rune_stamp_cell(ctx, idx, rune, _SFTE_ATTR_WIDE);

        // Place a dummy right after it so that it has enough space to render.
        int32_t dummy_idx = _sfte_grid_get_idx(ctx, ctx->term.cursor_col + 1, ctx->term.cursor_row);
        _sfte_rune_stamp_cell(ctx, dummy_idx, rune, _SFTE_ATTR_DUMMY);

        ctx->term.cursor_col += 2;
        return;
    }
#endif  // SFTE_FONT_WIDE_CHARS

    // Write a normal, one-width character.
    int32_t idx = _sfte_grid_get_idx(ctx, ctx->term.cursor_col, ctx->term.cursor_row);
    _sfte_rune_stamp_cell(ctx, idx, rune, 0);
    ctx->term.cursor_col++;
}

/*
    Feeds a single byte into the UTF-8 state machine.
    Returns 1 if a complete rune has been successfully decoded.
    Returns 0 if more bytes are needed, or if an invalid sequence was encountered (which aborts
   the current sequence and resets the state machine).

    If `SFTE_TERM_ASCII_CHARSET` is 1, skips UTF8 bytes to ensure they don't break the layout.
*/
static inline uint8_t _sfte_rune_utf8_decode(sfte_ctx *ctx, uint8_t b) {
    if (ctx->term.utf8_bytes_left > 0) {
        if ((b & 0xC0) == 0x80) {  // Continuation byte
#if !SFTE_TERM_ASCII_CHARSET
            ctx->term.utf8_rune_acc = (ctx->term.utf8_rune_acc << 6) | (b & 0x3F);
#endif  // !SFTE_TERM_ASCII_CHARSET
            ctx->term.utf8_bytes_left--;
            if (!ctx->term.utf8_bytes_left) return 1;
            return 0;
        } else
            ctx->term.utf8_bytes_left = 0;  // Invalid sequence, abort
    }

#if !SFTE_TERM_ASCII_CHARSET
    // Update UTF-8 rune accumulator
    if ((b & 0x80) == 0x00)
        ctx->term.utf8_rune_acc = b;
    else if ((b & 0xE0) == 0xC0)
        ctx->term.utf8_rune_acc = b & 0x1F;
    else if ((b & 0xF0) == 0xE0)
        ctx->term.utf8_rune_acc = b & 0x0F;
    else if ((b & 0xF8) == 0xF0)
        ctx->term.utf8_rune_acc = b & 0x07;
#endif  // !SFTE_TERM_ASCII_CHARSET

    // Get UTF-8 byte count
    if ((b & 0x80) == 0x00) {
        ctx->term.utf8_bytes_left = 0;
        return 1;
    } else if ((b & 0xE0) == 0xC0)
        ctx->term.utf8_bytes_left = 1;
    else if ((b & 0xF0) == 0xE0)
        ctx->term.utf8_bytes_left = 2;
    else if ((b & 0xF8) == 0xF0)
        ctx->term.utf8_bytes_left = 3;

    return 0;
}
// =================================================================================================
// >>color
// =================================================================================================

#if !SFTE_COLOR_TRUECOLOR
/*
    Given a RGB888 color, returns an index to the closest color in the 256-color palette.
    The closest color is chosen picking a color with the shortest Euclidean distance.
*/
static inline uint16_t _sfte_color_rgb_to_256(uint8_t r, uint8_t g, uint8_t b) {
    // If it's grayscale, map to the 24-step grayscale ramp (232-255)
    if (r == g && g == b) {
        if (r < 8) return 16;     // Black color index
        if (r > 248) return 231;  // White color index
        uint8_t idx = 232 + ((r - 8) * 24) / 247;
        if (idx >= _SFTE_COLOR_FG_DEFAULT) return _SFTE_COLOR_FG_DEFAULT - 1;
        return idx;
    }
    // Map to the color region (6x6x6)
    uint16_t cr = (r * 5) / 255;
    uint16_t cg = (g * 5) / 255;
    uint16_t cb = (b * 5) / 255;
    return 16 + (36 * cr) + (6 * cg) + cb;
}
#endif  // !SFTE_COLOR_TRUECOLOR

/*
    Returns a color in a form compatible with how colors are stored in `sfte_cell`/`sfte_term`.
    If SFTE_COLOR_TRUECOLOR is 1, returns a RGB color from the 256-color palette.
    Otherwise returns the index itself.
*/
static inline uint32_t _sfte_color_from_idx(uint16_t idx) {
#if SFTE_COLOR_TRUECOLOR
    return _sfte_palette_256[idx];
#else   // !SFTE_COLOR_TRUECOLOR
    // Last two indices are reserved for default colors
    if (idx >= _SFTE_COLOR_FG_DEFAULT) return _SFTE_COLOR_FG_DEFAULT - 1;
    return idx;
#endif  // !SFTE_COLOR_TRUECOLOR
}

/*
    Returns a color in a form compatible with how colors are stored in `sfte_cell`/`sfte_term`.
    If SFTE_COLOR_TRUECOLOR is 1, returns the RGB color directly.
    Otherwise returns the index to the closest color to `rgb` in the 256-color palette.
 */
static inline uint32_t _sfte_color_from_rgb(uint32_t rgb) {
#if SFTE_COLOR_TRUECOLOR
    return rgb;
#else   // !SFTE_COLOR_TRUECOLOR
    return _sfte_color_rgb_to_256((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
#endif  // !SFTE_COLOR_TRUECOLOR
}

// =================================================================================================
// >>grid
// =================================================================================================

// -------------------------------------------------------------------------------------------------
// >grid helpers
// -------------------------------------------------------------------------------------------------

/*
    Converts a visual screen row (0 to ctx->term.rows-1) into a logical row.
    Projects the coordinate into the scrollback history if `SFTE_TERM_SCROLLBACK_CAP` isn't 0.
*/
static inline int32_t _sfte_grid_vis2log(sfte_ctx *ctx, int16_t visual_row) {
#if SFTE_TERM_SCROLLBACK_CAP
    return (int32_t)visual_row - ctx->term.sb_offset;
#else   // !SFTE_TERM_SCROLLBACK_CAP
    (void)ctx;
    return (int32_t)visual_row;
#endif  // !SFTE_TERM_SCROLLBACK_CAP
}

/*
    Converts a logical row (chronological) into a visual screen row.
    Returns negative values if the row is currently hidden in the scrollback.
 */
static inline int32_t _sfte_grid_log2vis(sfte_ctx *ctx, int32_t logical_row) {
#if SFTE_TERM_SCROLLBACK_CAP
    return logical_row + ctx->term.sb_offset;
#else   // !SFTE_TERM_SCROLLBACK_CAP
    (void)ctx;
    return logical_row;
#endif  // !SFTE_TERM_SCROLLBACK_CAP
}

/*
    Converts a visual 2D coordinate into a 1D physical memory index.
*/
static inline uint32_t _sfte_grid_get_idx(sfte_ctx *ctx, int16_t c, int32_t logical_row) {
    int32_t r = (logical_row + ctx->term.grid_off) % ctx->term.rows;
    if (r < 0) r += ctx->term.rows;
    return r * ctx->term.cols + c;
}

/*
    Retrieves a pointer to a specific cell in memory using logical grid coordinates.

    A negative `logical_row` seamlessly reaches back into the scrollback ring buffer.
    Assumes the caller has already validated that `logical_row` doesn't exceed scrollback
   length.
*/
static inline sfte_cell *_sfte_grid_get_cell(sfte_ctx *ctx, int16_t col, int32_t logical_row) {
#if SFTE_TERM_SCROLLBACK_CAP
    if (logical_row >= 0)
        return &ctx->term.cells[_sfte_grid_get_idx(ctx, col, logical_row)];
    else {
        int32_t cap = ctx->term.sb_cap;
        int32_t ring_row = ((ctx->term.sb_head + logical_row) % cap + cap) % cap;
        return &ctx->term.scrollback[ring_row * ctx->term.cols + col];
    }
#else   // !SFTE_TERM_SCROLLBACK_CAP
    return &ctx->term.cells[_sfte_grid_get_idx(ctx, col, logical_row)];
#endif  // !SFTE_TERM_SCROLLBACK_CAP
}

/*
    Returns the background color of a cell.
    The behavior of this function depends on whether `SFTE_COLOR_TRUECOLOR` is 1.
    If it is, it just returns the value.
    If it isn't, it treats the color value as an index to the 256-color palette.
*/
static inline uint32_t _sfte_grid_get_bg(sfte_cell *cell) {
#if SFTE_COLOR_TRUECOLOR
    return cell->bg ? cell->bg : SFTE_COLOR_BG;
#endif  // SFTE_COLOR_TRUECOLOR
    if (cell->bg == _SFTE_COLOR_BG_DEFAULT || (!cell->bg && !cell->rune)) return SFTE_COLOR_BG;
    return _sfte_palette_256[cell->bg & 0xFF];
}

/*
    Returns the foreground color of a cell.
    The behavior of this function depends on whether `SFTE_COLOR_TRUECOLOR` is 1.
    If it is, it just returns the value.
    If it isn't, it treats the color value as an index to the 256-color palette.
*/
static inline uint32_t _sfte_grid_get_fg(sfte_cell *cell) {
#if SFTE_COLOR_TRUECOLOR
    return cell->fg ? cell->fg : SFTE_COLOR_FG;
#endif  // SFTE_COLOR_TRUECOLOR
    if (cell->fg == _SFTE_COLOR_FG_DEFAULT || (!cell->fg && !cell->rune)) return SFTE_COLOR_FG;
    return _sfte_palette_256[cell->fg & 0xFF];
}

/*
    Returns the underline color of a cell.
    The behavior of this function depends on whether `SFTE_COLOR_TRUECOLOR` is 1.
    If it is, it just returns the value.
    If it isn't, it treats the color value as an index to the 256-color palette.

    If underline color is not set, defaults to foreground default color.
    If underline colors are not supported, returns foreground default color.
*/
static inline uint32_t _sfte_grid_get_ul(sfte_cell *cell) {
#if SFTE_UNDERLINE_COLORED
#if SFTE_COLOR_TRUECOLOR
    return cell->ul_color ? cell->ul_color : SFTE_COLOR_FG;
#endif  // SFTE_COLOR_TRUECOLOR

    if (cell->ul_color == _SFTE_COLOR_FG_DEFAULT || !cell->ul_color) return SFTE_COLOR_FG;
    return _sfte_palette_256[cell->ul_color & 0xFF];
#endif  // SFTE_UNDERLINE_COLORED
    (void)cell;
    return SFTE_COLOR_FG;
}

/*
    Converts physical pixel coordinates into discrete grid coordinates.
    `out_logical_row` includes scrollback offset (can be negative).
    `out_screen_row` is strictly clamped to the physical screen (0 to rows-1).
*/
static inline void _sfte_grid_from_px(sfte_ctx *ctx, int32_t px_x, int32_t px_y, int16_t *out_col,
                                      int32_t *out_logical_row, int16_t *out_screen_row) {
    int16_t c = _SFTE_CLAMP((px_x - SFTE_WINDOW_PAD_X) / ctx->font.cell_width, 0,
                            ctx->term.cols - 1);
    int16_t r = _SFTE_CLAMP((px_y - SFTE_WINDOW_PAD_Y) / ctx->font.cell_height, 0,
                            ctx->term.rows - 1);
    if (out_col) *out_col = c;
    if (out_screen_row) *out_screen_row = r;
    if (out_logical_row) *out_logical_row = _sfte_grid_vis2log(ctx, r);
}

/*
    Flags a range of logical rows as dirty to force a redraw.
    Automatically handles min/max sorting, scrollback offset mapping,
    and clamping to the visible physical screen.
*/
static inline void _sfte_grid_dirty_rows(sfte_ctx *ctx, int32_t logical_row1,
                                         int32_t logical_row2) {
    int32_t min_logical_r = logical_row1 < logical_row2 ? logical_row1 : logical_row2;
    int32_t max_logical_r = logical_row1 > logical_row2 ? logical_row1 : logical_row2;

    for (int32_t logical_r = min_logical_r; logical_r <= max_logical_r; ++logical_r)
        if (logical_r >= 0) {
            int32_t row_start_idx = _sfte_grid_get_idx(ctx, 0, logical_r);
            for (int16_t c = 0; c < ctx->term.cols; ++c)
                ctx->term.cells[row_start_idx + c].dirty = 1;
        }
#if SFTE_TERM_SCROLLBACK_CAP
        else {
            int32_t cap = ctx->term.sb_cap;
            int32_t ring_row = ((ctx->term.sb_head + logical_r) % cap + cap) % cap;
            int32_t row_start_idx = ring_row * ctx->term.cols;
            for (int16_t c = 0; c < ctx->term.cols; ++c)
                ctx->term.scrollback[row_start_idx + c].dirty = 1;
        }
#endif  // SFTE_TERM_SCROLLBACK_CAP
}

/*
    Flags a rectangular region of the grid as dirty, forcing a redraw on the next frame.
    Safely clips coordinates that fall outside the terminal boundaries.
    Routes into the scrollback buffer if start_logical_r < 0.
*/
static inline void _sfte_grid_dirty_rect(sfte_ctx *ctx, int16_t start_col,
                                         int32_t start_logical_row, int16_t cols, int16_t rows) {
    int16_t start_c = _SFTE_CLAMP(start_col, 0, ctx->term.cols - 1);
    int16_t end_c = _SFTE_CLAMP(start_col + cols - 1, 0, ctx->term.cols - 1);
    int32_t end_logical_r = start_logical_row + rows - 1;

    for (int32_t logical_r = start_logical_row; logical_r <= end_logical_r; ++logical_r)
        for (int16_t c = start_c; c <= end_c; ++c)
            _sfte_grid_get_cell(ctx, c, logical_r)->dirty = 1;
}

/*
    Flags a contiguous 1D range of grid cells [start_idx; start_idx + cnt) as dirty.
    Does NOT perform boundary checking, uses asserts for boundary safety
    to avoid branch-checking overhead in RELEASE builds.
*/
static inline void _sfte_grid_dirty_range(sfte_ctx *ctx, uint32_t start_idx, uint32_t cnt) {
    SFTE_ASSERT(start_idx + cnt <= (uint32_t)(ctx->term.cols * ctx->term.rows),
                "dirty_range index overflow");

    for (uint32_t i = 0; i < cnt; ++i) ctx->term.cells[start_idx + i].dirty = 1;
}

#if SFTE_CURSOR_TRAIL
/*
    Flags the AABB of the cursor trail as dirty.
*/
static inline void _sfte_grid_dirty_trail(sfte_ctx *ctx) {
    if (ctx->term.trail_dmg.w <= 0 || ctx->term.trail_dmg.h <= 0) return;

    int16_t start_c, end_c;
    int32_t start_logical_r, end_logical_r;
    _sfte_grid_from_px(ctx, ctx->term.trail_dmg.x, ctx->term.trail_dmg.y, &start_c,
                       &start_logical_r, NULL);
    _sfte_grid_from_px(ctx, ctx->term.trail_dmg.x + ctx->term.trail_dmg.w,
                       ctx->term.trail_dmg.y + ctx->term.trail_dmg.h, &end_c, &end_logical_r, NULL);
    if (start_logical_r < 0 || end_logical_r < 0) return;

    _sfte_grid_dirty_rect(ctx, start_c, start_logical_r, (end_c - start_c) + 1,
                          (end_logical_r - start_logical_r) + 1);
    _sfte_grid_dirty_rect(ctx, 0, 0, ctx->term.cols, ctx->term.rows);
}
#endif  // SFTE_CURSOR_TRAIL

/*
    Calculates how many terminal cells are spanned by a given pixel dimension,
    accounting for an arbitrary pixel offset within the starting cell.
*/
static inline int16_t _sfte_grid_span(int32_t px_len, int32_t px_off, int32_t cell_px) {
    return (px_len + px_off + cell_px - 1) / cell_px;
}

#if SFTE_IMG_SIXEL
/*
    Deletes Sixel image placements that intersect with a cleared grid region.
    Automatically frees Sixel image data if the reference count drops to 0.
    Kitty images are ignored as they require explicit terminal delete commands.
*/
static inline void _sfte_grid_clear_sixel(sfte_ctx *ctx, int32_t start_idx, int32_t cnt) {
    for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement *p = &ctx->term.img_placements[i];
        if (!p->is_sixel) continue;
#if SFTE_TERM_ALT_SCREEN
        // Prevent active-screen clears from wiping out hidden-screen images
        if (p->alt_screen != ctx->term.alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN

        sfte_img *img = _sfte_img_find(ctx, p->img_id);
        if (!img) continue;

        int16_t cols = _sfte_grid_span(img->width, p->x_off, ctx->font.cell_width);
        int16_t rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);

        uint8_t overlap = 0;
        for (int16_t r = p->start_row; r < p->start_row + rows && !overlap; ++r)
            for (int16_t c = p->start_col; c < p->start_col + cols; ++c) {
                int32_t cell_idx = r * ctx->term.cols + c;
                if (cell_idx >= start_idx && cell_idx < start_idx + cnt) {
                    overlap = 1;
                    break;
                }
            }

        if (overlap) {
            img->ref_cnt--;
            ctx->term.img_placements[i--] = ctx->term
                                                .img_placements[--ctx->term.img_placements_len];
        }
    }

    for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i) {
        if (ctx->term.img_pool[i].ref_cnt || !ctx->term.img_pool[i].is_sixel) continue;
        if (ctx->term.img_pool[i].pixels) SFTE_FREE(ctx->term.img_pool[i].pixels);
        ctx->term.img_pool[i--] = ctx->term.img_pool[--ctx->term.img_pool_len];
    }
}
#endif  // SFTE_IMG_SIXEL

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
/*
    Translates image placements up/down during screen scroll.
    Deletes images that scroll entirely out of the scrollback buffer.
*/
static inline void _sfte_grid_scroll_images(sfte_ctx *ctx, int16_t lines, int16_t top,
                                            int16_t bot) {
    for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement *p = &ctx->term.img_placements[i];
#if SFTE_TERM_ALT_SCREEN
        if (p->alt_screen != ctx->term.alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN

        // An image should only scroll if it falls into one of two categories:
        // 1. it is inside the active scrolling margins (or sitting just on the edge at bot+1)
        // 2. It is in the scrollback buffer (<0), AND we are doing a full-screen scroll
        // (top==0) which pushes new lines into the scrollback.
        if ((p->start_row >= top && p->start_row <= bot + 1) || (top == 0 && p->start_row < 0))
            p->start_row -= lines;

        sfte_img *img = _sfte_img_find(ctx, p->img_id);
        if (!img) continue;

        int32_t rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);
        if (p->start_row + rows <= -SFTE_TERM_SCROLLBACK_CAP) {
            img->ref_cnt--;
            ctx->term.img_placements[i--] = ctx->term
                                                .img_placements[--ctx->term.img_placements_len];
        }
    }
}
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_TERM_SCROLLBACK_CAP
/*
    Pushes lines scrolling off the top of the screen into the ring buffer.
*/
static inline void _sfte_grid_push_scrollback(sfte_ctx *ctx, int16_t lines) {
#if SFTE_TERM_ALT_SCREEN
    if (ctx->term.alt_active) return;
#endif  // SFTE_TERM_ALT_SCREEN

    int16_t cols = ctx->term.cols;
    for (int16_t i = 0; i < lines; ++i) {
        int32_t ring_idx = ctx->term.sb_head * cols;
        uint32_t screen_idx = _sfte_grid_get_idx(ctx, 0, i);
        memcpy(&ctx->term.scrollback[ring_idx], &ctx->term.cells[screen_idx],
               cols * sizeof(sfte_cell));

        ctx->term.sb_head = (ctx->term.sb_head + 1) % ctx->term.sb_cap;
        if (ctx->term.sb_len < ctx->term.sb_cap) ctx->term.sb_len++;
    }
}
#endif  // SFTE_TERM_SCROLLBACK_CAP

// -------------------------------------------------------------------------------------------------
// >grid core
// -------------------------------------------------------------------------------------------------

/*
    Wipes a 1D range of cells, resetting them to ' ' and applying the
    currently active terminal foreground, background and text attributes.
    Also triggers the deletion of any Sixel images that intersect that region.
    Does NOT move the cursor.
*/
static inline void _sfte_grid_clear_cells(sfte_ctx *ctx, uint32_t start_idx, uint32_t cnt) {
    for (uint32_t i = 0; i < cnt; ++i) {
        sfte_cell *c = &ctx->term.cells[start_idx + i];
        c->rune = ' ';
        c->fg = ctx->term.cur_fg;
        c->bg = ctx->term.cur_bg;
        c->attr = 0;
        c->dirty = 1;
#if SFTE_UNDERLINE_EXTENDED
        c->ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
#if SFTE_UNDERLINE_COLORED
        c->ul_color = _SFTE_COLOR_FG_DEFAULT;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_TERM_REFLOW
        c->wrapped = 0;
#endif  // SFTE_TERM_REFLOW
#if SFTE_INPUT_HYPERLINKS
        c->link_idx = 0;
#endif  // SFTE_INPUT_HYPERLINKS
    }

#if SFTE_IMG_SIXEL
    _sfte_grid_clear_sixel(ctx, start_idx, cnt);
#endif  // SFTE_IMG_SIXEL
}

/*
    Wipes `cnt` rows starting from `start_row`, resetting them to ' ' and applying the
    currently active terminal foreground, background and text attributes.
    Also triggers the deletion of any Sixel images that intersect these rows.
    Does NOT move the cursor.
*/
static inline void _sfte_grid_clear_rows(sfte_ctx *ctx, int16_t start_row, int16_t cnt) {
    for (int16_t r = start_row; r < start_row + cnt; ++r)
        _sfte_grid_clear_cells(ctx, _sfte_grid_get_idx(ctx, 0, r), ctx->term.cols);
}

/*
    Shifts the terminal grid up or down by the specified number of lines.
    Positive values scroll the text UP (moving the viewport down).
    Negative values scroll the text DOWN (moving the viewport up).

    Strictly respects the active scroll margins (`ctx->term.scroll_top/bottom`).
    Pushes lines that fall off the top margin into the scrollback buffer
    (only if scrolling the primary screen and starting from row 0).

    Synchronizes image placements to scroll with the text.
*/
static inline void _sfte_grid_scroll(sfte_ctx *ctx, int16_t lines) {
    int16_t top = ctx->term.scroll_top;
    int16_t bot = ctx->term.scroll_bot;
    int16_t height = bot - top + 1;
    int16_t cols = ctx->term.cols;

    // Clamp the scroll amount to the region height while preserving direction.
    lines = _SFTE_CLAMP(lines, -height, height);

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    _sfte_grid_scroll_images(ctx, lines, top, bot);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

    if (lines > 0) {  // Scroll up
#if SFTE_TERM_SCROLLBACK_CAP
        if (top == 0) _sfte_grid_push_scrollback(ctx, lines);
#endif  // SFTE_TERM_SCROLLBACK_CAP

        if (top == 0 && bot == ctx->term.rows - 1)
            ctx->term.grid_off = (ctx->term.grid_off + lines) % ctx->term.rows;
        else {
            int16_t move_cnt = height - lines;
            for (int16_t i = 0; i < move_cnt; ++i) {
                uint32_t dst_idx = _sfte_grid_get_idx(ctx, 0, top + i);
                uint32_t src_idx = _sfte_grid_get_idx(ctx, 0, top + lines + i);
                memcpy(&ctx->term.cells[dst_idx], &ctx->term.cells[src_idx],
                       cols * sizeof(sfte_cell));
            }
        }

        _sfte_grid_clear_rows(ctx, bot - lines + 1, lines);
    } else if (lines < 0) {  // Scroll down
        lines = -lines;

        if (top == 0 && bot == ctx->term.rows - 1) {
            ctx->term.grid_off = (ctx->term.grid_off - lines) % ctx->term.rows;
            if (ctx->term.grid_off < 0) ctx->term.grid_off += ctx->term.rows;
        } else {
            int16_t move_cnt = height - lines;
            for (int16_t i = move_cnt - 1; i >= 0; --i) {
                uint32_t dst_idx = _sfte_grid_get_idx(ctx, 0, top + lines + i);
                uint32_t src_idx = _sfte_grid_get_idx(ctx, 0, top + i);
                memcpy(&ctx->term.cells[dst_idx], &ctx->term.cells[src_idx],
                       cols * sizeof(sfte_cell));
            }
        }

        _sfte_grid_clear_rows(ctx, top, lines);
    }

    _sfte_grid_dirty_rows(ctx, top, bot);
}

/*
    Evaluates the cursor's X position against the terminal width and handles wrapping.
    Must be called BEFORE printing a character that might fall off the edge.

    If auto-wrap is ON, it wraps cursor to column 0 of the next line.
    If already at the bottom scroll margin, forces a 1-line scroll upwards.
    Marks the wrapped cell with a `wrapped = 1` flag (used by the reflow engine).

    If auto-wrap is OFF, it clamps the cursor to the final column, causing subsequent
    characters to overwrite each other.
*/
static inline void _sfte_grid_check_wrap(sfte_ctx *ctx) {
    if (ctx->term.cursor_col >= ctx->term.cols) {
        if (ctx->term.auto_wrap) {
#if SFTE_TERM_REFLOW
            ctx->term.cells[_sfte_grid_get_idx(ctx, ctx->term.cols - 1, ctx->term.cursor_row)]
                .wrapped = 1;
#endif  // SFTE_TERM_REFLOW
            ctx->term.cursor_col = 0;
            if (ctx->term.cursor_row == ctx->term.scroll_bot) {
                uint32_t saved_bg = ctx->term.cur_bg;
                ctx->term.cur_bg = _SFTE_COLOR_BG_DEFAULT;
                _sfte_grid_scroll(ctx, 1);
                ctx->term.cur_bg = saved_bg;
            } else if (ctx->term.cursor_row < ctx->term.rows - 1)
                ctx->term.cursor_row++;
        } else
            ctx->term.cursor_col = ctx->term.cols - 1;
    }
}

// -------------------------------------------------------------------------------------------------
// >resize
// -------------------------------------------------------------------------------------------------

#if SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW
/*
    Performs a simple 2D truncation/padding copy of a grid.
    Used for alt-screens (which don't reflow) and as the primary resizer in non-reflow builds.
    Allocates and returns a new `sfte_cell` array. Caller assumes ownership.
*/
static sfte_cell *_sfte_grid_resize_dumb_copy(sfte_cell *old_grid, int16_t old_cols,
                                              int16_t old_rows, int16_t new_cols,
                                              int16_t new_rows) {
    sfte_cell *new_grid = (sfte_cell *)SFTE_CALLOC(new_cols * new_rows, sizeof(sfte_cell));
    if (!old_grid) return new_grid;
    SFTE_ASSERT(new_grid, "failed to allocate resized grid");

    int16_t min_cols = new_cols < old_cols ? new_cols : old_cols;
    int16_t min_rows = new_rows < old_rows ? new_rows : old_rows;

    for (int16_t r = 0; r < min_rows; ++r)
        for (int16_t c = 0; c < min_cols; ++c)
            new_grid[r * new_cols + c] = old_grid[r * old_cols + c];

    return new_grid;
}
#endif  // SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW

/*
    Reallocates the tab stops array and populates new columns with default intervals.
*/
static inline void _sfte_grid_resize_tabs(sfte_ctx *ctx, int16_t old_cols, int16_t new_cols) {
    uint8_t *new_tabs = (uint8_t *)SFTE_MALLOC(new_cols);
    SFTE_ASSERT(new_tabs, "failed to allocate new tab stops");
    for (int16_t i = 0; i < new_cols; ++i) {
        if (i < old_cols)
            new_tabs[i] = ctx->term.tab_stops[i];
        else
            new_tabs[i] = (i % SFTE_TERM_TAB_WIDTH == 0);
    }
    SFTE_FREE(ctx->term.tab_stops);
    ctx->term.tab_stops = new_tabs;
}

/*
    Reallocates all terminal buffers (main, alt, scrollback, tab stops) to match new dimensions.
    Assumes ownership of freeing the old ctx->term.cells arrays.

    if SFTE_TERM_REFLOW is enabled, performs a topological wrap/unwrap of the main screen text.
    If SFTE_TERM_REFLOW is disabled, performs a simple 2D truncation/padding copy.
    Alt-screen grids are always dumb-copied and never reflowed.
*/
static inline void _sfte_grid_resize(sfte_ctx *ctx, int16_t new_cols, int16_t new_rows) {
    if (new_cols < 1 || new_rows < 1) return;

#if SFTE_FONT_LIGATURES
    ctx->term.render_shaper_ids = (uint16_t *)SFTE_REALLOC(ctx->term.render_shaper_ids,
                                                           new_cols * sizeof(uint16_t));
    ctx->term.row_hashes = (uint64_t *)SFTE_REALLOC(ctx->term.row_hashes,
                                                    new_rows * sizeof(uint64_t));
    memset(ctx->term.row_hashes, 0,
           new_rows * sizeof(uint64_t));  // Force cache misses on resize
    ctx->term.row_shaper_ids = (uint16_t *)SFTE_REALLOC(ctx->term.row_shaper_ids,
                                                        new_rows * new_cols * sizeof(uint16_t));
#endif  // SFTE_FONT_LIGATURES
    ctx->term.render_ids = (uint16_t *)SFTE_REALLOC(ctx->term.render_ids,
                                                    new_cols * sizeof(uint16_t));
    ctx->term.render_font_indices = (uint8_t *)SFTE_REALLOC(ctx->term.render_font_indices,
                                                            new_cols * sizeof(uint8_t));
    ctx->term.render_target_caches = (sfte_font_cache **)SFTE_REALLOC(
        ctx->term.render_target_caches, new_cols * sizeof(sfte_font_cache *));

    int16_t grid_off_old = ctx->term.grid_off;
    int16_t old_cols = ctx->term.cols;
#if SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW
    int16_t old_rows = ctx->term.rows;
#endif  // SFTE_TERM_ALT_SCREEN || !SFTE_TERM_REFLOW

#if SFTE_TERM_ALT_SCREEN
    sfte_cell *main_old = ctx->term.alt_active ? ctx->term.alt_cells : ctx->term.cells;
    sfte_cell *alt_old = ctx->term.alt_active ? ctx->term.cells : NULL;
    int16_t target_col = ctx->term.alt_active ? ctx->term.saved_col[0] : ctx->term.cursor_col;
    int16_t target_row = ctx->term.alt_active ? ctx->term.saved_row[0] : ctx->term.cursor_row;
#else
    sfte_cell *main_old = ctx->term.cells;
    int16_t target_col = ctx->term.cursor_col;
    int16_t target_row = ctx->term.cursor_row;
#endif  // !SFTE_TERM_ALT_SCREEN

    _sfte_resize_buffers out = {0};

#if SFTE_TERM_REFLOW
    out = _sfte_reflow_generate_buffers(ctx, main_old, grid_off_old, new_cols, new_rows, target_col,
                                        target_row);
#else  // !SFTE_TERM_REFLOW
    out.main_grid = _sfte_grid_resize_dumb_copy(main_old, old_cols, old_rows, new_cols, new_rows);
    out.new_col = _SFTE_CLAMP(target_col, 0, new_cols - 1);
    out.new_row = _SFTE_CLAMP(target_row, 0, new_rows - 1);
#if SFTE_TERM_SCROLLBACK_CAP
    out.sb_grid = (sfte_cell *)SFTE_CALLOC(ctx->term.sb_cap * new_cols, sizeof(sfte_cell));
    out.sb_lines = 0;
#endif  // SFTE_TERM_SCROLLBACK_CAP
#endif  // !SFTE_TERM_REFLOW

#if SFTE_TERM_ALT_SCREEN
    // Alt screen uses dumb copy, since it's never reflowed
    sfte_cell *alt_new = NULL;
    if (alt_old)
        alt_new = _sfte_grid_resize_dumb_copy(alt_old, old_cols, old_rows, new_cols, new_rows);

    // Only free old grids if we got a different pointer
    if (main_old && main_old != out.main_grid && main_old != alt_new) SFTE_FREE(main_old);
    if (alt_old && alt_old != out.main_grid && alt_old != alt_new) SFTE_FREE(alt_old);

    ctx->term.cells = ctx->term.alt_active ? alt_new : out.main_grid;

    // Don't destroy the alt screen if resized while on main screen
    if (ctx->term.alt_active) {
        ctx->term.alt_cells = out.main_grid;
        ctx->term.saved_col[0] = out.new_col;
        ctx->term.saved_row[0] = out.new_row;
        ctx->term.cursor_col = _SFTE_CLAMP(ctx->term.cursor_col, 0, new_cols - 1);
        ctx->term.cursor_row = _SFTE_CLAMP(ctx->term.cursor_row, 0, new_rows - 1);
    } else {
        ctx->term.alt_cells = alt_new ? alt_new : alt_old;
        ctx->term.cursor_col = out.new_col;
        ctx->term.cursor_row = out.new_row;
    }
#else   // !SFTE_TERM_ALT_SCREEN
    if (main_old && main_old != out.main_grid) SFTE_FREE(main_old);
    ctx->term.cells = out.main_grid;
    ctx->term.cursor_col = out.new_col;
    ctx->term.cursor_row = out.new_row;
#endif  // !SFTE_TERM_ALT_SCREEN

#if SFTE_TERM_SCROLLBACK_CAP
    if (ctx->term.scrollback) SFTE_FREE(ctx->term.scrollback);
    ctx->term.scrollback = out.sb_grid;
    ctx->term.sb_head = out.sb_lines % ctx->term.sb_cap;
    ctx->term.sb_offset = 0;
    ctx->term.sb_len = out.sb_lines;
#endif  // SFTE_TERM_SCROLLBACK_CAP

    ctx->term.grid_off = 0;
    ctx->term.cols = new_cols;
    ctx->term.rows = new_rows;
    ctx->term.scroll_top = 0;
    ctx->term.scroll_bot = new_rows - 1;

    _sfte_grid_resize_tabs(ctx, old_cols, new_cols);
    _sfte_grid_dirty_range(ctx, 0, new_cols * new_rows);
    _SFTE_INFO(ctx, TERM_RESIZE, new_cols, new_rows);
}
// =================================================================================================
// >>img
// =================================================================================================
#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
/*
    Inserts a new image into the global image pool.

    Takes ownership of `img.pxs`.
    If the pool is at maximum capacity, it safely frees `img.pxs` and returns NULL.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time an image is inserted. Do not store this pointer.
*/
static inline sfte_img *_sfte_img_pool_insert(sfte_ctx *ctx, sfte_img img) {
    uint8_t oom = 0;
    _SFTE_MEM_ENSURE_CAP(sfte_img, ctx->term.img_pool, ctx->term.img_pool_len,
                         ctx->term.img_pool_cap, 1, SFTE_IMG_POOL_INIT_CAP, SFTE_IMG_POOL_MAX_CAP,
                         oom);
    if (oom) {
        if (img.pixels) SFTE_FREE(img.pixels);
        return NULL;
    }

    ctx->term.img_pool[ctx->term.img_pool_len] = img;
    return &ctx->term.img_pool[ctx->term.img_pool_len++];
}

/*
    Inserts a new image placement instruction into the global placements list.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time a placement is inserted. Do not store this pointer.
*/
static inline sfte_img_placement *_sfte_img_placement_insert(sfte_ctx *ctx, sfte_img_placement p) {
    uint8_t oom = 0;
    _SFTE_MEM_ENSURE_CAP(sfte_img_placement, ctx->term.img_placements, ctx->term.img_placements_len,
                         ctx->term.img_placements_cap, 1, SFTE_IMG_PLACEMENT_INIT_CAP,
                         SFTE_IMG_PLACEMENT_MAX_CAP, oom);
    if (oom) return NULL;

    ctx->term.img_placements[ctx->term.img_placements_len] = p;
    return &ctx->term.img_placements[ctx->term.img_placements_len++];
}

/*
    Locates an image in the global pool by its ID.
    Returns NULL if the image was deleted.

    WARN: The returned pointer points directly into a dynamic array.
    It WILL be invalidated the next time a placement is inserted. Do not store this pointer.
*/
static inline sfte_img *_sfte_img_find(sfte_ctx *ctx, uint32_t id) {
    for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i)
        if (ctx->term.img_pool[i].id == id) return &ctx->term.img_pool[i];
    return NULL;
}
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
// =================================================================================================
// >>view
// =================================================================================================

#if SFTE_WINDOW_PAD_X || SFTE_WINDOW_PAD_Y
/*
    Fills the padding regions around the terminal grid with the background color.
*/
static inline void _sfte_view_clear_padding_rects(sfte_ctx *ctx, void *px_buf) {
    int32_t w = ctx->width;
    int32_t h = ctx->height;
    int32_t grid_w = ctx->term.cols * ctx->font.cell_width;
    int32_t grid_h = ctx->term.rows * ctx->font.cell_height;
    uint32_t bg = (SFTE_COLOR_BG_OPACITY << 24) | (SFTE_COLOR_BG & ~SFTE_COLOR_ALPHA_MASK);

#if SFTE_WINDOW_PAD_Y
    for (int32_t y = 0; y < SFTE_WINDOW_PAD_Y && y < h; ++y)
        for (int32_t x = 0; x < w; ++x) SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, w, h, bg);

    for (int32_t y = SFTE_WINDOW_PAD_Y + grid_h; y < h; ++y)
        for (int32_t x = 0; x < w; ++x) SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, w, h, bg);
#endif  // SFTE_WINDOW_PAD_Y

#if SFTE_WINDOW_PAD_X
    for (int32_t y = SFTE_WINDOW_PAD_Y; y < SFTE_WINDOW_PAD_Y + grid_h && y < h; ++y) {
        for (int32_t x = 0; x < SFTE_WINDOW_PAD_X && x < w; ++x)
            SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, w, h, bg);
        for (int32_t x = SFTE_WINDOW_PAD_X + grid_w; x < w; ++x)
            SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, w, h, bg);
    }
#endif  // SFTE_WINDOW_PAD_X
}
#endif  // SFTE_WINDOW_PAD_X || SFTE_WINDOW_PAD_Y

// =================================================================================================
// >>input
// =================================================================================================

#if SFTE_INPUT_SELECTION
/*
    Determines if a specific cell falls within the active selection bounds.
    Evaluates against logical rows, meaning selections correclty scroll with text history.
*/
static inline uint8_t _sfte_input_is_selected(sfte_ctx *ctx, int16_t col, int16_t row) {
    if (!ctx->term.mouse_sel_active) return 0;

    int16_t start_col = ctx->term.mouse_sel_start_col;
    int16_t start_row = ctx->term.mouse_sel_start_row;
    int16_t end_col = ctx->term.mouse_sel_end_col;
    int16_t end_row = ctx->term.mouse_sel_end_row;

    // Normalize backward drags
    if (start_row > end_row || (start_row == end_row && start_col > end_col)) {
        int16_t tmp = start_row;
        start_row = end_row, end_row = tmp;
        tmp = start_col, start_col = end_col, end_col = tmp;
    }

    if (row < start_row || row > end_row) return 0;
    if (start_row == end_row) return (col >= start_col && col <= end_col);  // same line
    if (row == start_row) return col >= start_col;                          // first line
    if (row == end_row) return col <= end_col;                              // last line
    return 1;                                                               // middle lines
}
#endif  // SFTE_INPUT_SELECTION

#if SFTE_INPUT_MOUSE
// Tracking modes (DECSET)
#define SFTE_INPUT_MOUSE_MODE_CLICK 1000   // Report button press/release only
#define SFTE_INPUT_MOUSE_MODE_DRAG 1002    // Report clicks and drag motion
#define SFTE_INPUT_MOUSE_MODE_MOTION 1003  // Report all hover and drag motion

// Formatting extensions (DECSET)
#define SFTE_INPUT_MOUSE_EXT_DEFAULT 0  // Legacy X10 encoding
#define SFTE_INPUT_MOUSE_EXT_SGR 1006   // Modern SGR encoding

// Encoding offsets
#define SFTE_INPUT_MOUSE_BTN_RELEASE 3     // The default "button released" state in legacy modes
#define SFTE_INPUT_MOUSE_MOTION_OFFSET 32  // Added to the button state to indicate motion/dragging
#define SFTE_INPUT_MOUSE_X10_OFFSET                                                                \
    32  // Added to coords to ensure they are printable ASCII characters
#define SFTE_INPUT_MOUSE_X10_MAX_COORD 223  // 255 (max byte) - 32 (offset)

/*
    Encodes and flushes mouse events back to the host application via terminal escape sequences.
    Supports both legacy X10 (max coords 223) and modern SGR 1006 formats.
    Must be fed `screen_r` coordinates, never `logical_r` coordinates.
*/
static inline void _sfte_input_send_mouse_event(sfte_ctx *ctx, uint8_t btn, uint8_t is_release,
                                                int16_t col, int16_t row, uint8_t is_motion) {
    if (!ctx->term.mouse_mode) return;

    uint8_t encoded_btn = btn;
    // Legacy modes cannot encode which button was released
    if (is_release && ctx->term.mouse_ext != SFTE_INPUT_MOUSE_EXT_SGR) {
        encoded_btn = SFTE_INPUT_MOUSE_BTN_RELEASE;
    }

    if (is_motion) {
        if (ctx->term.mouse_mode == SFTE_INPUT_MOUSE_MODE_DRAG &&
            ctx->term.mouse_btn_state != SFTE_INPUT_MOUSE_BTN_RELEASE)
            encoded_btn = ctx->term.mouse_btn_state + SFTE_INPUT_MOUSE_MOTION_OFFSET;  // Dragging
        else if (ctx->term.mouse_mode == SFTE_INPUT_MOUSE_MODE_MOTION)
            encoded_btn = ctx->term.mouse_btn_state +
                          SFTE_INPUT_MOUSE_MOTION_OFFSET;  // Hover/dragging
        else
            return;  // Mode 1000 ignores motion
    }

    char buf[16];
    size_t len = 0;
    int16_t term_col = col + 1;
    int16_t term_row = row + 1;  // 1-based indexing for terminal escape sequences

    if (ctx->term.mouse_ext == SFTE_INPUT_MOUSE_EXT_SGR) {
        // SGR: ESC [ < btn ; x ; y M/m
        char end_char = is_release ? 'm' : 'M';
        len = snprintf(buf, sizeof(buf), "\033[<%d;%d;%d%c", encoded_btn, term_col, term_row,
                       end_char);
    } else {
        // X10: ESC [ <btn+32> <x+32> <y+32>
        if (term_col > SFTE_INPUT_MOUSE_X10_MAX_COORD || term_row > SFTE_INPUT_MOUSE_X10_MAX_COORD)
            return;
        len = snprintf(buf, sizeof(buf), "\033[M%c%c%c", encoded_btn + SFTE_INPUT_MOUSE_X10_OFFSET,
                       term_col + SFTE_INPUT_MOUSE_X10_OFFSET,
                       term_row + SFTE_INPUT_MOUSE_X10_OFFSET);
    }

    if (ctx->write_cb) ctx->write_cb(ctx->user_data, buf, len);
}
#endif  // SFTE_INPUT_MOUSE

// =================================================================================================
// >>reflow
// =================================================================================================
#if SFTE_TERM_REFLOW
/*
    Pushes a single cell into the temporary linear buffer.
*/
static inline void _sfte_reflow_push(_sfte_reflow_state *st, sfte_cell cell, uint8_t is_cursor) {
#if SFTE_FONT_WIDE_CHARS
    // If we're pushing a double-width character and we're at last column, wrap early.
    if ((cell.attr & _SFTE_ATTR_WIDE) && st->reflow_col == st->new_cols - 1) {
        sfte_cell space = cell;
        space.rune = ' ';
        space.fg = _SFTE_COLOR_FG_DEFAULT;
        space.bg = _SFTE_COLOR_BG_DEFAULT;
        space.attr = 0;
        space.wrapped = 1;
        st->temp_rows[st->reflow_row * st->new_cols + st->reflow_col] = space;
        st->reflow_col = 0;
        st->reflow_row++;
    }
#endif  // SFTE_FONT_WIDE_CHARS

    if (st->reflow_col == st->new_cols) {
        st->temp_rows[st->reflow_row * st->new_cols + st->new_cols - 1].wrapped = 1;
        st->reflow_col = 0;
        st->reflow_row++;
    }

    if (is_cursor) {
        st->new_col = st->reflow_col;
        st->new_row = st->reflow_row;
    }

    cell.wrapped = 0;
    st->temp_rows[st->reflow_row * st->new_cols + st->reflow_col] = cell;
    st->reflow_col++;
}

/*
    Calculates the true length of a line by trimming empty trailing spaces.
    If the cursor is on this line (cursor_cx >= 0), it ensures the length includes the cursor.
*/
static inline int16_t _sfte_reflow_get_len(sfte_cell *row, int16_t cols, int16_t cursor_col) {
    if (row[cols - 1].wrapped) return cols;
    int16_t len = cols;

    // Terminals pad lines with empty cells. When resizing, we must trim these
    // to prevent invisible padding from wrapping and creating artificial blank lines.
    // However, a cell is only truly empty if:
    // 1. it contains a space or null rune,
    // 2. its bg is the default color (colored spaces are used by TUIs to draw UI).
    // Finally, we must NEVER trim the cell where the cursor is currently sitting,
    // even if its a blank space.
    while (len > 0 && (row[len - 1].rune == ' ' || row[len - 1].rune == 0) &&
           row[len - 1].bg == SFTE_COLOR_BG) {
        if (cursor_col >= 0 && len - 1 == cursor_col) break;
        len--;
    }

    // Ensure the cursor isn't trimmed out.
    // Handles cases where cursor_cx is beyond the actual string length, e.g., wrap pending
    // states.
    if (cursor_col >= 0 && len <= cursor_col) len = cursor_col + 1;

    return len;
}

/*
    Processes a single row, extracting length, pushing cells, and handling cursor edge cases.
*/
static inline void _sfte_reflow_process_row(sfte_cell *row, int16_t cols, int16_t cursor_col,
                                            _sfte_reflow_state *st) {
    int16_t len = _sfte_reflow_get_len(row, cols, cursor_col);

    for (int16_t c = 0; c < len; ++c) _sfte_reflow_push(st, row[c], c == cursor_col);

    if (cursor_col >= 0 && cursor_col >= len) {
        if (st->reflow_col == st->new_cols) {
            st->temp_rows[st->reflow_row * st->new_cols + st->new_cols - 1].wrapped = 1;
            st->reflow_col = 0;
            st->reflow_row++;
        }
        st->new_col = st->reflow_col;
        st->new_row = st->reflow_row;
    }

    if (!row[cols - 1].wrapped) {
        st->reflow_col = 0;
        st->reflow_row++;
    }
}

/*
    Flattens the terminals history (both the scrollback ring buffer and active 2D grid)
    into a single continuous 1D stream.

    To wrap text accurately across the boundaries of the screen and the scrollback, it
    must evaluate the text from the oldest recorded line down to the newest.
    Because the scrollback is a circular array, we must do modular rithmetic backwards
    from `sb_head` to read it in chronological order.
    The active cursor can only exist on the live screen, so we pass `-1`
    during scrollback iteration to explicitly trim trailing whitespace on all historical lines.
*/
static inline void _sfte_reflow_grid_into_linear(sfte_ctx *ctx, sfte_cell *main_old,
                                                 int16_t grid_off_old, _sfte_reflow_state *st) {
#if SFTE_TERM_SCROLLBACK_CAP
    for (int32_t i = 0; i < ctx->term.sb_len; ++i) {
        uint32_t ring_idx = (ctx->term.sb_head - ctx->term.sb_len + i + ctx->term.sb_cap) %
                            ctx->term.sb_cap;
        sfte_cell *row = &ctx->term.scrollback[ring_idx * ctx->term.cols];
        _sfte_reflow_process_row(row, ctx->term.cols, -1, st);
    }
#endif  // SFTE_TERM_SCROLLBACK_CAP

    // reflow live grid
    st->is_live = 1;
    for (int16_t r = 0; r < ctx->term.rows; ++r) {
        int32_t phys_r = (r + grid_off_old) % ctx->term.rows;
        if (phys_r < 0) phys_r += ctx->term.rows;

        sfte_cell *row = &main_old[phys_r * ctx->term.cols];
        int16_t cursor_col = (r == st->target_old_row) ? st->target_old_col : -1;

        _sfte_reflow_process_row(row, ctx->term.cols, cursor_col, st);
    }
}

/*
    Allocates the temporary linear buffer and performs the topological text reflow.
*/
static sfte_cell *_sfte_reflow_linearize(sfte_ctx *ctx, sfte_cell *main_old, int16_t grid_off_old,
                                         int16_t new_cols, int16_t new_rows,
                                         _sfte_reflow_state *st) {
    // ctx->term.cols / new_cols calculates the raw expansion factor.
    // We add +2 to to this multiplier:
    // +1 to account for integer division truncation, and
    // +1 as a safety margin for double-width characters that force early wraps.
    int16_t max_temp_rows = (
#if SFTE_TERM_SCROLLBACK_CAP
                                ctx->term.sb_len +
#endif  // SFTE_TERM_SCROLLBACK_CAP
                                ctx->term.rows) *
                            (ctx->term.cols / new_cols + 2);

    if (max_temp_rows < new_rows) max_temp_rows = new_rows;

    sfte_cell *temp_rows = (sfte_cell *)SFTE_CALLOC(max_temp_rows * new_cols, sizeof(sfte_cell));
    SFTE_ASSERT(temp_rows, "failed to allocate temporary row data");

    st->new_cols = new_cols;
    st->temp_rows = temp_rows;
    st->reflow_col = 0, st->reflow_row = 0;
    st->new_col = 0, st->new_row = 0;
    st->is_live = 0;

    // Walks the old grids and populates st->temp_rows
    _sfte_reflow_grid_into_linear(ctx, main_old, grid_off_old, st);

    return temp_rows;
}

/*
    Maps the linearized reflow buffer back into distinct 2D main and scrollback grids.
    Modifies `st->new_cy` to reflect its clamped viewport position.
*/
static inline void _sfte_reflow_extract_view(sfte_ctx *ctx, int16_t new_cols, int16_t new_rows,
                                             int16_t target_row, _sfte_reflow_state *st,
                                             _sfte_resize_buffers *out) {
    (void)ctx;
    int32_t total_lines = st->reflow_row + (st->reflow_col > 0 ? 1 : 0);

    out->main_grid = (sfte_cell *)SFTE_CALLOC(new_cols * new_rows, sizeof(sfte_cell));
    SFTE_ASSERT(out->main_grid, "failed to allocate resized terminal grid");

    // Clamp the cursors target row to the new screen height
    int16_t r = target_row < new_rows ? target_row : new_rows - 1;

    // Map the viewport to match the cursors visual row
    int32_t screen_top = st->new_row - r;

    // Clamp the viewport to the absolute limits of the text buffer
    int32_t max_top = total_lines - new_rows;
    if (max_top < 0) max_top = 0;
    screen_top = _SFTE_CLAMP(screen_top, 0, max_top);

#if SFTE_TERM_SCROLLBACK_CAP
    out->sb_grid = (sfte_cell *)SFTE_CALLOC(ctx->term.sb_cap * new_cols, sizeof(sfte_cell));
    SFTE_ASSERT(out->sb_grid, "failed to allocate resized scrollback");

    // The scrollback is above `screen_top`
    out->sb_lines = screen_top;
    if (out->sb_lines > ctx->term.sb_cap) out->sb_lines = ctx->term.sb_cap;
    int32_t sb_start = screen_top - out->sb_lines;

    if (out->sb_lines > 0)
        memcpy(out->sb_grid, &st->temp_rows[sb_start * new_cols],
               (size_t)out->sb_lines * new_cols * sizeof(sfte_cell));
#endif  // SFTE_TERM_SCROLLBACK_CAP

    int32_t copy_lines = _SFTE_CLAMP(total_lines - screen_top, 0, new_rows);
    if (copy_lines > 0)
        memcpy(out->main_grid, &st->temp_rows[screen_top * new_cols],
               (size_t)copy_lines * new_cols * sizeof(sfte_cell));

    for (int32_t i = copy_lines * new_cols; i < new_rows * new_cols; ++i) {
        out->main_grid[i].rune = ' ';
        out->main_grid[i].bg = _SFTE_COLOR_BG_DEFAULT;
        out->main_grid[i].fg = _SFTE_COLOR_FG_DEFAULT;
        out->main_grid[i].attr = 0;
        out->main_grid[i].wrapped = 0;
    }

    st->new_row = _SFTE_CLAMP(st->new_row - screen_top, 0, new_rows - 1);
}

/*
    Orchestrates the reflow pipeline and returns the newly allocated grid buffers.
*/
static _sfte_resize_buffers _sfte_reflow_generate_buffers(sfte_ctx *ctx, sfte_cell *main_old,
                                                          int16_t grid_off_old, int16_t new_cols,
                                                          int16_t new_rows, int16_t target_col,
                                                          int16_t target_row) {
    _sfte_reflow_state st = {
        .target_old_col = target_col,
        .target_old_row = target_row,
    };

    sfte_cell *temp = _sfte_reflow_linearize(ctx, main_old, grid_off_old, new_cols, new_rows, &st);

    _sfte_resize_buffers out = {0};
    _sfte_reflow_extract_view(ctx, new_cols, new_rows, target_row, &st, &out);

    SFTE_FREE(temp);

    out.new_col = st.new_col;
    out.new_row = st.new_row;

    return out;
}
#endif  // SFTE_TERM_REFLOW
// =================================================================================================
// >>sixel
// =================================================================================================
#if SFTE_IMG_SIXEL

#define _SFTE_IMG_SIXEL_OFFSET 63         // ASCII offset ('?' is 63)
#define _SFTE_IMG_SIXEL_BAND_HEIGHT 6     // Each sixel row represents 6 vertical pixels
#define _SFTE_IMG_SIXEL_MAX_PARAMS 5      // Max parameters in a color definition
#define _SFTE_IMG_SIXEL_RGB_MAX 100       // RGB values are set as percentages
#define _SFTE_IMG_SIXEL_COLORSPACE_HLS 1  // HLS color space identifier
#define _SFTE_IMG_SIXEL_COLORSPACE_RGB 2  // RGB color space identifier

/*
    Finalizes a completed sixel sequence.

    NOTE:
    Sixel requires the text cursor to be pushed to the beginning of next line
    after image finishes drawing, scrolling the screen if necessary.
*/
static inline void _sfte_sixel_commit(sfte_ctx *ctx) {
    if (ctx->sixel.width > 0 && ctx->sixel.height > 0) {
        // Slide each row backward to eliminate empty capacity gaps
        for (int32_t y = 0; y < ctx->sixel.height; ++y)
            memmove(&ctx->sixel.pixels[y * ctx->sixel.width],
                    &ctx->sixel.pixels[y * ctx->sixel.cap_w], ctx->sixel.width * sizeof(uint32_t));

        size_t final_bytes = ctx->sixel.width * ctx->sixel.height * sizeof(uint32_t);
        uint32_t *packed_pixels = (uint32_t *)SFTE_REALLOC(ctx->sixel.pixels, final_bytes);
        if (!packed_pixels) packed_pixels = ctx->sixel.pixels;

        sfte_img *img = _sfte_img_pool_insert(ctx, (sfte_img){
                                                       .pixels = packed_pixels,
                                                       .id = ++ctx->term.next_img_id,
                                                       .width = ctx->sixel.width,
                                                       .height = ctx->sixel.height,
                                                       .is_sixel = 1,
                                                   });
        if (img) {
            sfte_img_placement *p = _sfte_img_placement_insert(
                ctx, (sfte_img_placement){
                         .img_id = img->id,
                         .start_col = ctx->sixel.start_col,
                         .start_row = ctx->sixel.start_row,
                         .z_idx = 1,
                         .is_sixel = 1,
#if SFTE_TERM_ALT_SCREEN
                         .alt_screen = ctx->term.alt_active,
#endif  // SFTE_TERM_ALT_SCREEN
                     });
            if (p) {
                img->ref_cnt++;

                int16_t rows = _sfte_grid_span(ctx->sixel.height, p->y_off, ctx->font.cell_height);
                if (ctx->sixel.height % ctx->font.cell_height != 0) rows++;

                // Force the text cursor below the placed image
                ctx->term.cursor_col = 0;
                ctx->term.cursor_row = ctx->sixel.start_row + rows;

                // If the image pushed the cursor off the screen, scroll down
                while (ctx->term.cursor_row > ctx->term.scroll_bot) {
                    _sfte_grid_scroll(ctx, 1);
                    ctx->term.cursor_row--;
                }
            } else
                SFTE_FREE(packed_pixels);
        } else
            SFTE_FREE(packed_pixels);
    } else if (ctx->sixel.pixels)
        SFTE_FREE(ctx->sixel.pixels);

    ctx->sixel.pixels = NULL;
    ctx->sixel.cap_w = 0;
    ctx->sixel.cap_h = 0;
    ctx->sixel.width = 0;
    ctx->sixel.height = 0;
}

/*
    Ensures the image buffer is large enough for the incoming pixels.
    Size gets doubled on each dimension when the buffer is not big enough.
    Maxes out at `SFTE_IMG_SIXEL_MAX_SIZE` x `SFTE_IMG_SIXEL_MAX_SIZE`.
*/
static inline void _sfte_sixel_ensure_cap(sfte_ctx *ctx, int32_t req_w, int32_t req_h) {
    if (req_w < ctx->sixel.cap_w && req_h < ctx->sixel.cap_h) return;

    int32_t new_w = ctx->sixel.cap_w == 0 ? SFTE_IMG_SIXEL_INIT_SIZE : ctx->sixel.cap_w;
    int32_t new_h = ctx->sixel.cap_h == 0 ? SFTE_IMG_SIXEL_INIT_SIZE : ctx->sixel.cap_h;

    while (new_w >= req_w && new_w < SFTE_IMG_SIXEL_MAX_SIZE) new_w *= 2;
    while (new_h >= req_h && new_h < SFTE_IMG_SIXEL_MAX_SIZE) new_h *= 2;

    if (new_w > SFTE_IMG_SIXEL_MAX_SIZE) new_w = SFTE_IMG_SIXEL_MAX_SIZE;
    if (new_h > SFTE_IMG_SIXEL_MAX_SIZE) new_h = SFTE_IMG_SIXEL_MAX_SIZE;

    if (new_w <= ctx->sixel.cap_w && new_h <= ctx->sixel.cap_h) return;

    uint32_t *new_pixels = (uint32_t *)SFTE_CALLOC(new_w * new_h, sizeof(uint32_t));
    SFTE_ASSERT(new_pixels, "failed to allocate sixel buffer");

    if (ctx->sixel.pixels) {
        for (int32_t r = 0; r < ctx->sixel.height; ++r)
            memcpy(&new_pixels[r * new_w], &ctx->sixel.pixels[r * ctx->sixel.cap_w],
                   ctx->sixel.width * sizeof(uint32_t));
        SFTE_FREE(ctx->sixel.pixels);
    }

    ctx->sixel.pixels = new_pixels;
    ctx->sixel.cap_w = new_w;
    ctx->sixel.cap_h = new_h;
}

/*
    Decodes a 6-bit bitmask and paints it onto the image buffer.
    Handles buffer resizing if it's too small.

    A sixel is a vertical column of 6 pixels encoded into a single ASCII character.
    The bits (from LSB to MSB) represent pixels from top to bottom.
    ASCII '?' (value 63) minus offset 63 = 000000 (all blank).
    ASCII '~' (value 126) minus offset 63 = 111111 (all solid).
*/
static inline void _sfte_sixel_draw_pattern(sfte_ctx *ctx, uint8_t pattern, int32_t repeats) {
    int32_t max_x = ctx->sixel.x + repeats - 1;
    int32_t max_y = ctx->sixel.y + _SFTE_IMG_SIXEL_BAND_HEIGHT - 1;

    _sfte_sixel_ensure_cap(ctx, max_x, max_y);
    uint32_t col = ctx->sixel.palette[ctx->sixel.col_idx];

    for (int32_t dx = 0; dx < repeats; ++dx) {
        int32_t px_x = ctx->sixel.x + dx;

        for (uint8_t bit = 0; bit < _SFTE_IMG_SIXEL_BAND_HEIGHT; ++bit) {
            if (!(pattern & (1 << bit))) continue;
            int32_t px_y = ctx->sixel.y + bit;

            // Skip invalid sequence requests
            if (px_x >= ctx->sixel.cap_w || px_y >= ctx->sixel.cap_h) continue;

            ctx->sixel.pixels[px_y * ctx->sixel.cap_w + px_x] = col;

            if (px_x >= ctx->sixel.width) ctx->sixel.width = px_x + 1;
            if (px_y >= ctx->sixel.height) ctx->sixel.height = px_y + 1;
        }
    }

    ctx->sixel.x += repeats;
}

/*
    Converts a hue angle into a single RGB channel.
*/
static inline float _sfte_sixel_hue_to_rgb(float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

/*
    Converts HLS values to a packed 32-bit ARGB color.
    Unlike modern HSL, sixel uses integer degrees for hue (0-360) and percentages for L/S
   (0-100).
*/
static inline uint32_t _sfte_sixel_hls_to_rgb(uint16_t h_deg, uint16_t l_pct, uint16_t s_pct) {
    float h = h_deg / 360.0f;
    float l = l_pct / 100.0f;
    float s = s_pct / 100.0f;

    uint8_t r, g, b;

    if (s == 0.0f)
        r = g = b = (uint8_t)(l * 255.0f);
    else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h + 1.0f / 3.0f) * 255.0f);
        g = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h) * 255.0f);
        b = (uint8_t)(_sfte_sixel_hue_to_rgb(p, q, h - 1.0f / 3.0f) * 255.0f);
    }

    return SFTE_COLOR_ALPHA_MASK | (r << 16) | (g << 8) | b;
}

/*
    Parses the accumulated numerical parameters to update the active color or palette.
    Initiated by the pound symbol (`#`).
    If one parameter is provided, the active color index gets changed to value of that
   parameter. If five parameters are provided, it defines a new color. Parameter values are as
   follows: #<idx>;<space>;<c1>;<c2>;<c3>. Color space is 1 for HLS, 2 for RGB.
*/
static inline void _sfte_sixel_apply_color(sfte_ctx *ctx) {
    if (ctx->sixel.param_idx == 0 && ctx->sixel.params[0] < 256)
        ctx->sixel.col_idx = ctx->sixel.params[0];
    else if (ctx->sixel.param_idx == 4) {
        int idx = ctx->sixel.params[0];
        int space = ctx->sixel.params[1];

        if (idx >= 0 && idx <= 256) {
            if (space == _SFTE_IMG_SIXEL_COLORSPACE_HLS)
                ctx->sixel.palette[idx] = _sfte_sixel_hls_to_rgb(
                    ctx->sixel.params[2], ctx->sixel.params[3], ctx->sixel.params[4]);
            else if (space == _SFTE_IMG_SIXEL_COLORSPACE_RGB) {
                uint8_t r = (ctx->sixel.params[2] * 255) / _SFTE_IMG_SIXEL_RGB_MAX;
                uint8_t g = (ctx->sixel.params[3] * 255) / _SFTE_IMG_SIXEL_RGB_MAX;
                uint8_t b = (ctx->sixel.params[4] * 255) / _SFTE_IMG_SIXEL_RGB_MAX;
                ctx->sixel.palette[idx] = SFTE_COLOR_ALPHA_MASK | (r << 16) | (g << 8) | b;
            }
        }
    }
}

/*
    The core state machine for the sixel byte stream.

    When parsing repeats (e.g. `!255`) or colors (e.g. `#1;2;100;0;0`), the terminal
    receives ASCII digits one at a time. To convert these characters into integers
    without external buffers, we use:
        `acc = acc * 10 + (b - '0')`
    `b - '0'` subtracts 48 to convert an ASCII character (e.g. `5`) to an integer (5).
    `acc * 10` shifts the previously parsed value one decimal place to the left.

    To avoid growing the call stack via recursion when a state
    needs to hand a byte back to SIXEL_GROUND, it uses a while-loop based on the `cont`
   variable.
*/
static inline void _sfte_sixel_parse_byte(sfte_ctx *ctx, uint8_t b) {
    uint8_t cont = 1;
    while (cont) {
        cont = 0;

        switch (ctx->sixel.state) {
        case SIXEL_GROUND:
            if (b >= '?' && b <= '~') {
                uint8_t pattern = b - _SFTE_IMG_SIXEL_OFFSET;
                int32_t repeats = ctx->sixel.repeat_cnt > 0 ? ctx->sixel.repeat_cnt : 1;
                _sfte_sixel_draw_pattern(ctx, pattern, repeats);
                ctx->sixel.repeat_cnt = 0;
            } else if (b == '$')  // Carriage return
                ctx->sixel.x = 0;
            else if (b == '-') {  // Move down one band
                ctx->sixel.x = 0;
                ctx->sixel.y += _SFTE_IMG_SIXEL_BAND_HEIGHT;
            } else if (b == '!') {  // Start repeat sequence
                ctx->sixel.state = SIXEL_REPEAT;
                ctx->sixel.repeat_cnt = 0;
            } else if (b == '#') {  // Start color definition/selection
                ctx->sixel.state = SIXEL_COLOR_PARAM;
                ctx->sixel.param_idx = 0;
                memset(ctx->sixel.params, 0, sizeof(ctx->sixel.params));
            }
            break;
        case SIXEL_REPEAT:
            if (b >= '0' && b <= '9')
                ctx->sixel.repeat_cnt = ctx->sixel.repeat_cnt * 10 + (b - '0');
            else {
                // Done parsing repeat count, go back to ground state
                ctx->sixel.state = SIXEL_GROUND;
                cont = 1;
            }
            break;
        case SIXEL_COLOR_INTRO:
        case SIXEL_COLOR_PARAM:
            if (b >= '0' && b <= '9')
                ctx->sixel.params[ctx->sixel.param_idx] = ctx->sixel.params[ctx->sixel.param_idx] *
                                                              10 +
                                                          (b - '0');
            else if (b == ';' && ctx->sixel.param_idx < _SFTE_IMG_SIXEL_MAX_PARAMS - 1)
                // Move to next parameter
                ctx->sixel.param_idx++;
            else {  // Color sequence terminated by any non-digit/semicolon byte
                _sfte_sixel_apply_color(ctx);
                ctx->sixel.state = SIXEL_GROUND;
                cont = 1;
            }
            break;
        }
    }
}

/*
    Frees all sixel memory allocations.
*/
static inline void _sfte_sixel_deinit(sfte_ctx *ctx) {
    if (ctx->term.img_pool) {
        for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i)
            if (ctx->term.img_pool[i].pixels) SFTE_FREE(ctx->term.img_pool[i].pixels);
        SFTE_FREE(ctx->term.img_pool);
        ctx->term.img_pool = NULL;
    }
    if (ctx->term.img_placements) {
        SFTE_FREE(ctx->term.img_placements);
        ctx->term.img_placements = NULL;
    }
    if (ctx->sixel.pixels) {
        SFTE_FREE(ctx->sixel.pixels);
        ctx->sixel.pixels = NULL;
    }
}
#endif  // SFTE_IMG_SIXEL
// =================================================================================================
// >>kitty
// =================================================================================================
#if SFTE_INPUT_KITTY
/*
    Generates the kitty keyboard (CSI u) or standard CSI escape sequence for a given key.
    Returns bytes written to `out_buf`, or `0` if inactive/unhandled.
    `key` is a backend-agnostic `sfte_key` enum. may be SFTE_KEY_KONE if key is purely text.
    `codepoint` is the raw UTF-32 character value of the key (if applicable).
    `mod_mask` is a bitmask of the active modifiers using SFTE_MOD_* definitions.
*/
static inline size_t _sfte_kitty_kb_encode(sfte_ctx *ctx, sfte_key key, uint32_t codepoint,
                                           uint32_t mod_mask, char *out_buf, size_t max_bytes) {
    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = ctx->term.alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    uint16_t flags = ctx->term.kitty_kb_stack[s_idx][ctx->term.kitty_kb_stack_idx[s_idx]];
    if (flags == 0) return 0;

    uint8_t disambiguate = (flags & 1) != 0;
    uint8_t report_all = (flags & 8) != 0;

    uint32_t csi_mod = 1;
    if (mod_mask & SFTE_MOD_SHIFT) csi_mod += 1;
    if (mod_mask & SFTE_MOD_ALT) csi_mod += 2;
    if (mod_mask & SFTE_MOD_CTRL) csi_mod += 4;
    if (mod_mask & SFTE_MOD_SUPER) csi_mod += 8;

    // arrows, home, end, f1-f4 -> CSI 1 ; mods [char]
    char func_char = 0;
    switch (key) {
    case SFTE_KEY_UP: func_char = 'A'; break;
    case SFTE_KEY_DOWN: func_char = 'B'; break;
    case SFTE_KEY_RIGHT: func_char = 'C'; break;
    case SFTE_KEY_LEFT: func_char = 'D'; break;
    case SFTE_KEY_HOME: func_char = 'H'; break;
    case SFTE_KEY_END: func_char = 'F'; break;
    case SFTE_KEY_F1: func_char = 'P'; break;
    case SFTE_KEY_F2: func_char = 'Q'; break;
    case SFTE_KEY_F3: func_char = 'R'; break;
    case SFTE_KEY_F4: func_char = 'S'; break;
    default: break;
    }

    if (func_char) {
        if (csi_mod > 1 || disambiguate)
            return snprintf(out_buf, max_bytes, "\033[1;%u%c", csi_mod, func_char);
        return 0;
    }

    // insert, delete, pgup, pgdn, f5-f12 -> CSI num ; mods ~
    uint8_t tilde_num = 0;
    switch (key) {
    case SFTE_KEY_INSERT: tilde_num = 2; break;
    case SFTE_KEY_DELETE: tilde_num = 3; break;
    case SFTE_KEY_PAGE_UP: tilde_num = 5; break;
    case SFTE_KEY_PAGE_DOWN: tilde_num = 6; break;
    case SFTE_KEY_F5: tilde_num = 15; break;
    case SFTE_KEY_F6: tilde_num = 17; break;
    case SFTE_KEY_F7: tilde_num = 18; break;
    case SFTE_KEY_F8: tilde_num = 19; break;
    case SFTE_KEY_F9: tilde_num = 20; break;
    case SFTE_KEY_F10: tilde_num = 21; break;
    case SFTE_KEY_F11: tilde_num = 23; break;
    case SFTE_KEY_F12: tilde_num = 24; break;
    default: break;
    }

    if (tilde_num) {
        if (csi_mod > 1 || disambiguate)
            return snprintf(out_buf, max_bytes, "\033[%d;%u~", tilde_num, csi_mod);
        return 0;
    }

    // text keys and control keys -> CSI codepoint ; mods u
    uint32_t target_cp = codepoint ? codepoint : (uint32_t)key;
    // we can safely use SFTE_KEY_* as ASCII values
    if (target_cp > 0) {
        uint8_t is_control = (target_cp == SFTE_KEY_TAB || target_cp == SFTE_KEY_ENTER ||
                              target_cp == SFTE_KEY_ESCAPE || target_cp == SFTE_KEY_BACKSPACE);

        // Emit kitty sequence if:
        // - modifiers are pressed, or
        // - it's a special control key, or
        // - its requested all keys to be escaped.
        if (csi_mod > 1 || is_control || report_all)
            return snprintf(out_buf, max_bytes, "\033[%u;%uu", target_cp, csi_mod);
    }

    return 0;
}
#endif  // SFTE_INPUT_KITTY

#if SFTE_IMG_KITTY

#define _SFTE_KITTY_FMT_RGB 24
#define _SFTE_KITTY_FMT_RGBA 32
#define _SFTE_KITTY_FMT_PNG_JPEG 100

/*
    Performs bilinear interpolation for image scaling.
    Kitty allows the terminal to dictate the final render size in rows/columns,
    this is the main purpose for this function.
    Uses the stack allocator to safely allocate the destination buffer,
    then slides it back over the source buffer to reclaim the memory.
*/
static inline uint32_t *_sfte_kitty_scale_image_bilinear(sfte_stack *stack, uint32_t *src,
                                                         int32_t src_wid, int32_t src_hei,
                                                         int32_t dst_wid, int32_t dst_hei) {
    size_t dst_bytes = dst_wid * dst_hei * sizeof(uint32_t);
    size_t pre_off = _sfte_mem_stack_save(stack);
    uint32_t *dst = (uint32_t *)_sfte_mem_stack_alloc(stack, dst_bytes, _Alignof(uint32_t));
    if (!dst) {
        _sfte_mem_stack_rewind(stack, pre_off);
        return NULL;
    }

    float x_ratio = ((float)(src_wid - 1)) / dst_wid;
    float y_ratio = ((float)(src_hei - 1)) / dst_hei;

    for (int32_t i = 0; i < dst_hei; ++i)
        for (int32_t j = 0; j < dst_wid; ++j) {
            int32_t x = (int32_t)(x_ratio * j);
            int32_t y = (int32_t)(y_ratio * i);
            float x_diff = (x_ratio * j) - x;
            float y_diff = (y_ratio * i) - y;
            // 4 nearest pixels
            size_t idx = y * src_wid + x;
            uint32_t p1 = src[idx];
            uint32_t p2 = (x + 1 < src_wid) ? src[idx + 1] : p1;
            uint32_t p3 = (y + 1 < src_hei) ? src[idx + src_wid] : p1;
            uint32_t p4 = (x + 1 < src_wid && y + 1 < src_hei) ? src[idx + src_wid + 1] : p1;
            // Calculate weights
            float w1 = (1.0f - x_diff) * (1.0f - y_diff);
            float w2 = x_diff * (1.0f - y_diff);
            float w3 = (1.0f - x_diff) * y_diff;
            float w4 = x_diff * y_diff;
            // Interpolate
            uint32_t r = (uint32_t)(((p1 >> 16) & 0xFF) * w1 + ((p2 >> 16) & 0xFF) * w2 +
                                    ((p3 >> 16) & 0xFF) * w3 + ((p4 >> 16) & 0xFF) * w4);
            uint32_t g = (uint32_t)(((p1 >> 8) & 0xFF) * w1 + ((p2 >> 8) & 0xFF) * w2 +
                                    ((p3 >> 8) & 0xFF) * w3 + ((p4 >> 8) & 0xFF) * w4);
            uint32_t b = (uint32_t)((p1 & 0xFF) * w1 + (p2 & 0xFF) * w2 + (p3 & 0xFF) * w3 +
                                    (p4 & 0xFF) * w4);
            uint32_t a = (uint32_t)(((p1 >> 24) & 0xFF) * w1 + ((p2 >> 24) & 0xFF) * w2 +
                                    ((p3 >> 24) & 0xFF) * w3 + ((p4 >> 24) & 0xFF) * w4);
            // Store temporarily in destination
            dst[i * dst_wid + j] = (a << 24) | (r << 16) | (g << 8) | b;
        }

    // Since the source image isn't needed anymore,
    // the scaled image can be moved to original image memory address.
    memmove(src, dst, dst_bytes);

    // Now we can reclaim the stack memory
    // that was used for scaled image before `memmove`.
    size_t src_off = (size_t)((uint8_t *)src - stack->buf);
    _sfte_mem_stack_rewind(stack, src_off + dst_bytes);

    return src;
}

/*
    Parses raw pixel buffers or delegates to stb_image for PNG/JPEG decoding.
    This kitty protocol implementation supports:
    - direct base64 pixel streams (via 'd'),
    - reading from a local file path (via 'f', used by e.g. yazi),
    - reading from a temporary file that the terminal is expected to delete after reading (via
   't').
*/
static inline uint32_t *_sfte_kitty_decode_payload(sfte_ctx *ctx, uint8_t *raw_data, size_t raw_len,
                                                   uint8_t is_file, const char *file_path,
                                                   int32_t *w, int32_t *h) {
    size_t pre_off = _sfte_mem_stack_save(&ctx->stack);
    uint32_t *pxs = NULL;

    if (ctx->kitty.format == _SFTE_KITTY_FMT_PNG_JPEG) {
        int channels = 0;
        uint8_t *stb_pxs = is_file ? stbi_load(file_path, w, h, &channels, 4)
                                   : stbi_load_from_memory(raw_data, raw_len, w, h, &channels, 4);

        if (stb_pxs && *w && *h) {
            pxs = (uint32_t *)_sfte_mem_stack_alloc(&ctx->stack, *w * *h * sizeof(uint32_t),
                                                    _Alignof(uint32_t));
            if (!pxs) {
                _sfte_mem_stack_rewind(&ctx->stack, pre_off);
                stbi_image_free(stb_pxs);
                return NULL;
            }

            for (int64_t i = 0; i < *w * *h; ++i)
                pxs[i] = (stb_pxs[i * 4 + 3] << 24) | (stb_pxs[i * 4 + 0] << 16) |
                         (stb_pxs[i * 4 + 1] << 8) | stb_pxs[i * 4 + 2];
            stbi_image_free(stb_pxs);
        }
    } else if ((ctx->kitty.format == _SFTE_KITTY_FMT_RGB ||
                ctx->kitty.format == _SFTE_KITTY_FMT_RGBA) &&
               *w && *h) {
        uint8_t bpp = (ctx->kitty.format == _SFTE_KITTY_FMT_RGB) ? 3 : 4;
        pxs = (uint32_t *)_sfte_mem_stack_alloc(&ctx->stack, *w * *h * sizeof(uint32_t),
                                                _Alignof(uint32_t));
        if (!pxs) {
            _sfte_mem_stack_rewind(&ctx->stack, pre_off);
            return NULL;
        }
        size_t post_pxs_off = _sfte_mem_stack_save(&ctx->stack);

        uint8_t *pixel_src = raw_data;
        size_t pixel_len = raw_len;

        if (is_file) {
            FILE *f = fopen(file_path, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                pixel_len = ftell(f);
                fseek(f, 0, SEEK_SET);

                if (pixel_len >= (size_t)(*w * *h * bpp)) {
                    pixel_src = (uint8_t *)_sfte_mem_stack_alloc(&ctx->stack, pixel_len,
                                                                 _Alignof(uint8_t));
                    if (!pixel_src) {
                        _sfte_mem_stack_rewind(&ctx->stack, pre_off);
                        fclose(f);
                        return NULL;
                    }
                    (void)fread(pixel_src, 1, pixel_len, f);
                } else
                    pixel_src = NULL;
                fclose(f);
            } else
                pixel_src = NULL;
        }

        if (pixel_src && pixel_len >= (size_t)(*w * *h * bpp)) {
            for (int64_t i = 0; i < *w * *h; ++i) {
                uint8_t a = (bpp == 4) ? pixel_src[i * bpp + 3] : 255;
                pxs[i] = (a << 24) | (pixel_src[i * bpp + 0] << 16) |
                         (pixel_src[i * bpp + 1] << 8) | pixel_src[i * bpp + 2];
            }

            _sfte_mem_stack_rewind(&ctx->stack, post_pxs_off);
        } else {
            _sfte_mem_stack_rewind(&ctx->stack, pre_off);
            pxs = NULL;
        }
    }
    return pxs;
}

/*
    Applies requested croppping limits before placing the image.
*/
static inline uint32_t *_sfte_kitty_apply_crop(sfte_ctx *ctx, size_t pre_pxs_off, uint32_t *pxs,
                                               int32_t *w, int32_t *h) {
    int32_t cx = _SFTE_CLAMP(ctx->kitty.crop_x, 0, *w);
    int32_t cy = _SFTE_CLAMP(ctx->kitty.crop_y, 0, *h);
    int32_t cw = ctx->kitty.crop_w ? ctx->kitty.crop_w : (*w - cx);
    int32_t ch = ctx->kitty.crop_h ? ctx->kitty.crop_h : (*h - cy);
    if (cx + cw > *w) cw = *w - cx;
    if (cy + ch > *h) ch = *h - cy;
    if (cw <= 0 || ch <= 0) {
        _sfte_mem_stack_rewind(&ctx->stack, pre_pxs_off);
        return NULL;
    }
    // If only cropped from bottom, the pointer doesn't change
    // so we can return immediately without any `memmove` calls.
    if (cx == 0 && cy == 0 && cw == *w) {
        *h = ch;
        return pxs;
    }

    if (cw == *w && ch == *h && !cx && !cy) return pxs;

    // Since for a pixel at coordinates (x,y) in cropped image:
    // crop_idx = (y * crop_w) + x
    // orig_idx = ((crop_y + y) * orig_w) + crop_x + x
    // where:
    // crop_x, crop_y >= 0
    // crop_w <= orig_w
    // we know that:
    // crop_idx <= orig_idx
    // so we can crop in place iterating over original image memory.
    // The original image index "outruns" the cropped image index.
    for (int32_t y = 0; y < ch; ++y)
        memmove(&pxs[y * cw], &pxs[(cy + y) * *w + cx], cw * sizeof(uint32_t));

    *w = cw;
    *h = ch;
    _sfte_mem_stack_rewind(&ctx->stack, pre_pxs_off + (cw * ch * sizeof(uint32_t)));
    return pxs;
}

/*
    Translates terminal cell bounds (columns/rows) into physical
    pixel dimensions, and scales the raster to match.
*/
static inline uint32_t *_sfte_kitty_apply_scale(sfte_ctx *ctx, uint32_t *pxs, int32_t *w,
                                                int32_t *h) {
    if (ctx->kitty.cols <= 0 && ctx->kitty.rows <= 0) return pxs;
    int32_t target_w = *w;
    int32_t target_h = *h;
    if (ctx->kitty.cols && !ctx->kitty.rows) {
        target_w = ctx->kitty.cols * ctx->font.cell_width;
        target_h = (target_w * *h) / *w;  // Maintain aspect ratio
    } else if (!ctx->kitty.cols && ctx->kitty.rows) {
        target_h = ctx->kitty.rows * ctx->font.cell_height;
        target_w = (target_h * *w) / *h;  // Maintain aspect ratio
    } else {
        target_w = ctx->kitty.cols * ctx->font.cell_width;
        target_h = ctx->kitty.rows * ctx->font.cell_height;
    }

    if (target_w <= 0 || target_h <= 0 || (target_w == *w && target_h == *h)) return pxs;

    uint32_t *scaled = _sfte_kitty_scale_image_bilinear(&ctx->stack, pxs, *w, *h, target_w,
                                                        target_h);
    if (!scaled) return NULL;

    *w = target_w;
    *h = target_h;
    return pxs;
}

/*
    Evaluates the kitty deletion matrix to determine if a specific
    placement should be destroyed. This protocol implementation allows wiping by:
    - ID,
    - Z index,
    - viewport intersection,
    - specific cursor locations.
*/
static inline uint8_t _sfte_kitty_should_delete(sfte_ctx *ctx, sfte_img_placement *p,
                                                sfte_img *img) {
    uint8_t matches_id = (!ctx->kitty.id || ctx->kitty.id == p->img_id);
    if (!matches_id) return 0;
    int16_t cols = _sfte_grid_span(img->width, p->x_off, ctx->font.cell_width);
    int16_t rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);
    int16_t target_c = ctx->kitty.crop_x - 1;
    int16_t target_r = ctx->kitty.crop_y - 1;
    uint8_t intersects_x = (target_c >= p->start_col && target_c < p->start_col + cols);
    uint8_t intersects_y = (target_r >= p->start_row && target_r < p->start_row + rows);
    switch (ctx->kitty.d_action) {
    case 'A':
    case 'a': return 1;  // Delete all
    case 'I':
    case 'i': return !ctx->kitty.placement_id || ctx->kitty.placement_id == p->placement_id;
    case 'C':
    case 'c':  // Delete if intersecting cursor
        return ctx->term.cursor_col >= p->start_col && ctx->term.cursor_col < p->start_col + cols &&
               ctx->term.cursor_row >= p->start_row && ctx->term.cursor_row < p->start_row + rows;
    case 'P':
    case 'p': return intersects_x && intersects_y;
    case 'X':
    case 'x': return intersects_x;
    case 'Y':
    case 'y': return intersects_y;
    case 'Z':
    case 'z': return p->z_idx == ctx->kitty.z_idx;
    case 'Q':
    case 'q': return intersects_x && intersects_y && p->z_idx == ctx->kitty.z_idx;
    default: return 0;
    }
}

/*
    Garbage collection via a simple reference counting.
    The image data is separated from image placements, we only free the image data
    when its `ref_cnt` drops to 0 (no placements are actively displaying it).
*/
static inline void _sfte_kitty_gc_pool(sfte_ctx *ctx) {
    for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i) {
        sfte_img *img = &ctx->term.img_pool[i];
        if (img->is_sixel || img->ref_cnt) continue;  // Skip active and sixel
        uint8_t should_del = (ctx->kitty.d_action == 'A' || ctx->kitty.d_action == 'a') ||
                             ((ctx->kitty.d_action == 'I' || ctx->kitty.d_action == 'i') &&
                              img->id == ctx->kitty.id);
        if (!should_del) continue;
        if (img->pixels) SFTE_FREE(img->pixels);
        ctx->term.img_pool[i--] = ctx->term.img_pool[--ctx->term.img_pool_len];
    }
}

/*
    Registers a new viewport placement for an existing image buffer,
    incrementing its reference count.

    Returns a error message used for acknowledgements if 'img' is NULL or placement pool is OOM.
    Returns NULL on success.
*/
static inline const char *_sfte_kitty_apply_placement(sfte_ctx *ctx, sfte_img *img) {
    if (!img) return "EINVAL: cannot apply placement to NULL image";
    sfte_img_placement *p = _sfte_img_placement_insert(ctx,
                                                       (sfte_img_placement){
                                                           .img_id = img->id,
                                                           .placement_id = ctx->kitty.placement_id,
                                                           .start_col = ctx->term.cursor_col,
                                                           .start_row = ctx->term.cursor_row,
                                                           .x_off = ctx->kitty.x_off,
                                                           .y_off = ctx->kitty.y_off,
                                                           .z_idx = ctx->kitty.z_idx,
#if SFTE_TERM_ALT_SCREEN
                                                           .alt_screen = ctx->term.alt_active,
#endif  // SFTE_TERM_ALT_SCREEN
                                                       });

    if (!p) return "ENOMEM: placement pool capacity reached";

    img->ref_cnt++;
    int16_t cols = _sfte_grid_span(img->width, ctx->kitty.x_off, ctx->font.cell_width);
    int16_t rows = _sfte_grid_span(img->height, ctx->kitty.y_off, ctx->font.cell_height);
    _sfte_grid_dirty_rect(ctx, ctx->term.cursor_col, ctx->term.cursor_row, cols, rows);
    return NULL;
}

/*
    Queries the terminals image support or interrogates the status of a specific image ID.
    If the terminal successfully parses the request, it replies with an OK status.

    Always returns NULL.
*/
static inline const char *_sfte_kitty_exec_query(sfte_ctx *ctx) {
    char reply[64];
    size_t len = snprintf(reply, sizeof(reply), "\033_Gi=%u;OK\033\\", ctx->kitty.id);
    if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
    return NULL;
}

/*
    Iterates through active image placements and removes those that match the requested
    deletion criteria passed via a kitty sequence.

    Returns a error message used for acknowledgements if no image is found to delete.
    Returns NULL on success.
*/
static inline const char *_sfte_kitty_exec_delete(sfte_ctx *ctx) {
    for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement *p = &ctx->term.img_placements[i];
#if SFTE_TERM_ALT_SCREEN
        if (p->alt_screen != ctx->term.alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN
        if (p->is_sixel) continue;

        sfte_img *img = _sfte_img_find(ctx, p->img_id);
        if (!img) return "EINVAL: failed to find image to delete";

        if (_sfte_kitty_should_delete(ctx, p, img)) {
            img->ref_cnt--;
            int16_t cols = _sfte_grid_span(img->width, p->x_off, ctx->font.cell_width);
            int16_t rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);
            _sfte_grid_dirty_rect(ctx, p->start_col, p->start_row, cols, rows);
            ctx->term.img_placements[i--] = ctx->term
                                                .img_placements[--ctx->term.img_placements_len];
        }
    }

    _sfte_kitty_gc_pool(ctx);
    return NULL;
}

/*
    Decodes the accumulated base64 payload into raw pixels, applies any requested
    cropping and scaling, and stores the final raster in the image pool.

    WARN:
    If the transmission medium is 't', the client sent a path to a temp file containing the
   pixels. The protocol dictates that the emulator assumes ownership of this file and MUST
   delete it after decoding to prevent disk leaks.

    Returns a error message used for acknowledgements if the base64 decode fails.
    Returns NULL on success.
*/
static inline const char *_sfte_kitty_exec_transmit(sfte_ctx *ctx, sfte_img **out_img) {
    size_t raw_len = 0;
    size_t pre_b64_off = _sfte_mem_stack_save(&ctx->stack);
    uint8_t *raw_data = _sfte_b64_decode(&ctx->stack, (uint8_t *)ctx->kitty.b64_buf,
                                         ctx->kitty.b64_len, &raw_len);
    if (!raw_data) return "ENOMEM: base64 decode failed";

    uint8_t is_file = (ctx->kitty.t_medium == 'f' || ctx->kitty.t_medium == 't');
    char *file_path = NULL;
    if (is_file) {
        file_path = (char *)_sfte_mem_stack_alloc(&ctx->stack, raw_len + 1, _Alignof(char));
        if (!file_path) {
            _sfte_mem_stack_rewind(&ctx->stack, pre_b64_off);
            return "ENOMEM: file path allocation failed";
        }

        memcpy(file_path, raw_data, raw_len);
        file_path[raw_len] = '\0';
    }

    int32_t w = ctx->kitty.width;
    int32_t h = ctx->kitty.height;
    size_t pre_pxs_off = _sfte_mem_stack_save(&ctx->stack);
    uint32_t *pixels = _sfte_kitty_decode_payload(ctx, raw_data, raw_len, is_file, file_path, &w,
                                                  &h);

    if (ctx->kitty.t_medium == 't' && is_file) remove(file_path);
    if (!pixels) return "EBADFMT: failed to decode image data";

    pixels = _sfte_kitty_apply_crop(ctx, pre_pxs_off, pixels, &w, &h);
    if (!pixels) {
        _sfte_mem_stack_rewind(&ctx->stack, pre_b64_off);
        return "EINVAL: invalid crop dimensions";
    }

    pixels = _sfte_kitty_apply_scale(ctx, pixels, &w, &h);
    if (!pixels) {
        _sfte_mem_stack_rewind(&ctx->stack, pre_b64_off);
        return "ENOMEM: scaling failed";
    }

    uint32_t *final_pixels = (uint32_t *)SFTE_MALLOC(w * h * sizeof(uint32_t));
    if (!final_pixels) {
        _sfte_mem_stack_rewind(&ctx->stack, pre_b64_off);
        return "ENOMEM: failed to allocate presistent image buffer";
    }

    memcpy(final_pixels, pixels, w * h * sizeof(uint32_t));
    _sfte_mem_stack_rewind(&ctx->stack, pre_b64_off);

    if (!ctx->kitty.id) ctx->kitty.id = ++ctx->term.next_img_id;
    sfte_img *new_img = _sfte_img_pool_insert(ctx, (sfte_img){
                                                       .pixels = final_pixels,
                                                       .id = ctx->kitty.id,
                                                       .width = w,
                                                       .height = h,
                                                   });

    if (!new_img) {
        SFTE_FREE(final_pixels);
        return "ENOMEM: image pool capacity reached";
    }

    if (out_img) *out_img = new_img;
    return NULL;
}

/*
    Creates a new visual placement on the grid for an image already residing in the pool.

    Returns a error message used for acknowledgements if the requested image ID
    was never transmitted or has been garbage collected, OR if `_sfte_kitty_apply_placement`
   fails. Returns NULL on success.
*/
static inline const char *_sfte_kitty_exec_place(sfte_ctx *ctx) {
    for (uint32_t j = 0; j < ctx->term.img_pool_len; ++j)
        if (ctx->term.img_pool[j].id == ctx->kitty.id)
            return _sfte_kitty_apply_placement(ctx, &ctx->term.img_pool[j]);
    return "ENOENT: image id not found in pool";
}

/*
    Sends an acknowledgement response back to the client application.
    The `q` parameter dictates response verbosity.
    - q=0: Send a response for both success and failure.
    - q=1: Send a response ONLY on failure (defalut).
    - q=2: Silent.
*/
static inline void _sfte_kitty_send_ack(sfte_ctx *ctx, const char *err_msg) {
    if (err_msg && (ctx->kitty.quiet == 0 || ctx->kitty.quiet == 1)) {
        char reply[256];
        size_t len = ctx->kitty.id > 0 ? snprintf(reply, sizeof(reply), "\033_Gi=%u;%s\033\\",
                                                  ctx->kitty.id, err_msg)
                                       : snprintf(reply, sizeof(reply), "\033_G;%s\033\\", err_msg);
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
    } else if (!err_msg && !ctx->kitty.quiet) {
        char reply[64];
        size_t len = ctx->kitty.id > 0
                         ? snprintf(reply, sizeof(reply), "\033_Gi=%u;OK\033\\", ctx->kitty.id)
                         : snprintf(reply, sizeof(reply), "\033_G;OK\033\\");
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
    }
}

/*
    The main entrypoint for kitty graphics sequences.
    Because terminal parsers usually have hard limits on escape sequence length,
    kitty protocol can send big image files across multiple escape sequences.

    The `m=1` parameter indicates that more data is coming.
    The base64 string is accumulated in `ctx->kitty.b64_buf` across multiple calls,
    and the evalutaion is triggered the moment a sequence with `m=0` is received.
*/
static inline void _sfte_kitty_parse_graphics(sfte_ctx *ctx, const char *payload) {
    const char *semi = strchr(payload, ';');
    // if there's no semicolon, the dictionary spans the entire payload
    const char *dict_end = semi ? semi : payload + strlen(payload);

    uint8_t more = 0;
    uint8_t is_new = 0;

    // Lookahead to check if its a new transmission
    for (const char *p = payload; p < semi; ++p) {
        if (*p == 'a' || *p == 'f' || *p == 'i' || *p == 's' || *p == 'v' || *p == 'z')
            if (p + 1 < dict_end && p[1] == '=') {
                is_new = 1;
                break;
            }
    }

    // reset state if fresh
    if (is_new || !ctx->kitty.action) {
        char *saved_buf = ctx->kitty.b64_buf;
        size_t saved_cap = ctx->kitty.b64_cap;
        ctx->kitty = (sfte_kitty_state){
            .b64_buf = saved_buf,
            .b64_cap = saved_cap,
            .format = _SFTE_KITTY_FMT_RGBA,
            .quiet = 1,
            .action = 'T',
            .t_medium = 'd',
            // rest 0-initialized
        };
    }

    // parse params into state
    const char *p = payload;
    while (p && p < dict_end) {
        char key = p[0];
        if (p + 1 >= dict_end || p[1] != '=') break;
        const char *val = p + 2;

        switch (key) {
        case 'a': ctx->kitty.action = val[0]; break;
        case 'c': ctx->kitty.cols = (int16_t)atoi(val); break;
        case 'd': ctx->kitty.d_action = val[0]; break;
        case 'f': ctx->kitty.format = (uint8_t)atoi(val); break;
        case 'h': ctx->kitty.crop_h = (int32_t)atoi(val); break;
        case 'i': ctx->kitty.id = (uint32_t)atoi(val); break;
        case 'm': more = (uint8_t)atoi(val); break;
        case 'p': ctx->kitty.placement_id = (uint32_t)atoi(val); break;
        case 'q': ctx->kitty.quiet = (uint8_t)atoi(val); break;
        case 'r': ctx->kitty.rows = (int16_t)atoi(val); break;
        case 's': ctx->kitty.width = (int32_t)atoi(val); break;
        case 't': ctx->kitty.t_medium = val[0]; break;
        case 'v': ctx->kitty.height = (int32_t)atoi(val); break;
        case 'w': ctx->kitty.crop_w = (int32_t)atoi(val); break;
        case 'x': ctx->kitty.crop_x = (int32_t)atoi(val); break;
        case 'y': ctx->kitty.crop_y = (int32_t)atoi(val); break;
        case 'z': ctx->kitty.z_idx = (int8_t)atoi(val); break;
        case 'X': ctx->kitty.x_off = (int16_t)atoi(val); break;
        case 'Y': ctx->kitty.y_off = (int16_t)atoi(val); break;
        default: break;
        }

        p = strchr(p, ',');
        if (!p || p >= dict_end) break;
        p++;  // skip comma
    }

    if (semi) {
        const char *b64_start = semi + 1;
        size_t b64_len = strlen(b64_start);

        uint8_t oom = 0;
        _SFTE_MEM_ENSURE_CAP(char, ctx->kitty.b64_buf, ctx->kitty.b64_len, ctx->kitty.b64_cap,
                             b64_len + 1, SFTE_KITTY_B64_INIT_CAP, SFTE_KITTY_B64_MAX_CAP, oom);

        if (oom) return;

        memcpy(ctx->kitty.b64_buf + ctx->kitty.b64_len, b64_start, b64_len);
        ctx->kitty.b64_len += b64_len;
    }

    if (more) return;  // Abort and wait for next sequence

    ctx->kitty.b64_buf[ctx->kitty.b64_len] = '\0';
    const char *err_msg = NULL;

    switch (ctx->kitty.action) {
    case 'q': err_msg = _sfte_kitty_exec_query(ctx); break;
    case 'd': err_msg = _sfte_kitty_exec_delete(ctx); break;
    case 't': err_msg = _sfte_kitty_exec_transmit(ctx, NULL); break;
    case 'T': {
        sfte_img *img = NULL;
        err_msg = _sfte_kitty_exec_transmit(ctx, &img);
        if (err_msg || !img) break;
        _sfte_kitty_apply_placement(ctx, img);
        break;
    }
    case 'p': _sfte_kitty_exec_place(ctx); break;
    default: err_msg = "EINVAL: unknown action"; break;
    }

    _sfte_kitty_send_ack(ctx, err_msg);

    ctx->kitty.b64_len = 0;
}

static inline void _sfte_kitty_deinit(sfte_ctx *ctx) {
    if (ctx->term.img_pool) {
        for (uint32_t i = 0; i < ctx->term.img_pool_len; ++i)
            if (ctx->term.img_pool[i].pixels) SFTE_FREE(ctx->term.img_pool[i].pixels);
        SFTE_FREE(ctx->term.img_pool);
        ctx->term.img_pool = NULL;
    }
    if (ctx->term.img_placements) {
        SFTE_FREE(ctx->term.img_placements);
        ctx->term.img_placements = NULL;
    }
    if (ctx->kitty.b64_buf) {
        SFTE_FREE(ctx->kitty.b64_buf);
        ctx->kitty.b64_buf = NULL;
    }
}
#endif  // SFTE_IMG_KITTY
// =================================================================================================
// >>csi
// =================================================================================================

/*
    VT500 parameters default to 1 if omitted or set to 0.
*/
#define _SFTE_P(val) ((val) > 0 ? (val) : 1)

/*
    Converts 1-based VT500 coordinates to 0-based array indices.
*/
#define _SFTE_P_IDX(val) (_SFTE_P(val) - 1)

/*
    Unpacks a 24-bit TrueColor RGB sequence from the parameter array.

    NOTE:
    It's not wrapped in `SFTE_COLOR_TRUECOLOR`,
    because this value is converted to the closest 256-color palette value
    if TrueColor is disabled.
*/
static inline uint32_t _sfte_csi_parse_truecolor(uint16_t *p, uint16_t i) {
    return SFTE_COLOR_ALPHA_MASK | (p[i + 2] << 16) | (p[i + 3] << 8) | p[i + 4];
}

/*
    Handles Insert Character / CSI @.

    Inserts n spaces at the cursor, shifting text right.
*/
static inline void _sfte_csi_exec_ich(sfte_ctx *ctx, uint16_t *p, int16_t col) {
    uint16_t n = _SFTE_P(p[0]);
    int16_t rem = ctx->term.cols - col;
    if (n > rem) n = rem;
    int16_t move_cnt = rem - n;
    int32_t base_idx = _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row);
    if (move_cnt > 0)
        memmove(&ctx->term.cells[base_idx + col + n], &ctx->term.cells[base_idx + col],
                move_cnt * sizeof(sfte_cell));
    _sfte_grid_clear_cells(ctx, base_idx + col, n);
    _sfte_grid_dirty_range(ctx, base_idx + col, rem);
}

/*
    Handles Cursor Next Line / CSI E.

    Moves cursor to the beginning of the line n lines down.
*/
static inline void _sfte_csi_exec_cnl(sfte_ctx *ctx, uint16_t *p) {
    ctx->term.cursor_col = 0;
    ctx->term.cursor_row = _SFTE_CLAMP(ctx->term.cursor_row + _SFTE_P(p[0]), 0, ctx->term.rows - 1);
}

/*
    Handles Cursor Previous Line / CSI F.

    Moves cursor to the beginning of the line n lines up.
*/
static inline void _sfte_csi_exec_cpl(sfte_ctx *ctx, uint16_t *p) {
    ctx->term.cursor_col = 0;
    ctx->term.cursor_row = _SFTE_CLAMP(ctx->term.cursor_row - _SFTE_P(p[0]), 0, ctx->term.rows - 1);
}

/*
    Handles Erase in Display / CSI J.
*/
static inline void _sfte_csi_exec_ed(sfte_ctx *ctx, int16_t mode, int16_t col) {
    // If we clear entire screen and scrollback exists,
    // push the data to scrollback instead of erasing it in its entirety
    if (mode == 2 || (mode == 0 && ctx->term.cursor_col == 0 && ctx->term.cursor_row == 0)) {
#if SFTE_TERM_SCROLLBACK_CAP
        // Find last populated row
        int16_t last_r = ctx->term.cursor_row;
        for (int16_t r = ctx->term.rows - 1; r > last_r; --r) {
            for (int16_t c = 0; c < ctx->term.cols; ++c) {
                sfte_cell *cell = &ctx->term.cells[_sfte_grid_get_idx(ctx, c, r)];
                if (cell->rune != ' ' && cell->rune != '\0') {
                    last_r = r;
                    break;
                }
            }

            if (last_r == r) break;
        }

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
        for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
            sfte_img_placement *p = &ctx->term.img_placements[i];
#if SFTE_TERM_ALT_SCREEN
            if (p->alt_screen != ctx->term.alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN

            sfte_img *img = _sfte_img_find(ctx, p->img_id);
            if (!img) continue;

            int16_t rows = _sfte_grid_span(img->height, p->y_off, ctx->font.cell_height);
            int16_t img_bot = p->start_row + rows - 1;
            if (img_bot > last_r) last_r = img_bot;
        }
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

        if (last_r >= ctx->term.rows) last_r = ctx->term.rows - 1;
        int16_t lines_to_push = last_r + 1;

        // Temporarily bypass scroll margins to ensure full-screen push
        int16_t old_top = ctx->term.scroll_top;
        int16_t old_bot = ctx->term.scroll_bot;
        ctx->term.scroll_top = 0;
        ctx->term.scroll_bot = ctx->term.rows - 1;

        if (lines_to_push > 0) _sfte_grid_scroll(ctx, lines_to_push);

        ctx->term.scroll_top = old_top;
        ctx->term.scroll_bot = old_bot;
#endif  // SFTE_TERM_SCROLLBACK_CAP

        _sfte_grid_clear_rows(ctx, 0, ctx->term.rows);
        return;
    }
    if (mode == 0) {
        // Clear the rest of the current line
        _sfte_grid_clear_cells(ctx, _sfte_grid_get_idx(ctx, col, ctx->term.cursor_row),
                               ctx->term.cols - col);
        // Clear all rows below the cursor
        if (ctx->term.cursor_row < ctx->term.rows - 1)
            _sfte_grid_clear_rows(ctx, ctx->term.cursor_row + 1,
                                  ctx->term.rows - 1 - ctx->term.cursor_row);
    } else if (mode == 1) {
        // Clear all rows above the cursor
        if (ctx->term.cursor_row > 0) _sfte_grid_clear_rows(ctx, 0, ctx->term.cursor_row);
        // Clear the start of the current line
        _sfte_grid_clear_cells(ctx, _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row), col + 1);
    } else if (mode == 2) {
        _sfte_grid_clear_rows(ctx, 0, ctx->term.rows);
    } else if (mode == 3) {
#if SFTE_TERM_SCROLLBACK_CAP && SFTE_TERM_SCROLLBACK_CLEAR
        ctx->term.sb_len = 0;
        ctx->term.sb_head = 0;
        ctx->term.sb_offset = 0;
#endif  // SFTE_TERM_SCROLLBACK_CAP && SFTE_TERM_SCROLLBACK_CLEAR
    }
}

/*
    Handles Erase Line / CSI K.

    Erases part or all of the current line.
*/
static inline void _sfte_csi_exec_el(sfte_ctx *ctx, int16_t mode, int16_t col) {
    if (mode == 0)
        _sfte_grid_clear_cells(ctx, _sfte_grid_get_idx(ctx, col, ctx->term.cursor_row),
                               ctx->term.cols - col);
    else if (mode == 1)
        _sfte_grid_clear_cells(ctx, _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row), col + 1);
    else if (mode == 2)
        _sfte_grid_clear_cells(ctx, _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row),
                               ctx->term.cols);
}

/*
    Handles Insert Line / CSI L.

    Inserts n blank lines at cursor position, pushing bottom lines off.
*/
static inline void _sfte_csi_exec_il(sfte_ctx *ctx, uint16_t *p) {
    uint16_t n = _SFTE_P(p[0]);
    int16_t bot = ctx->term.scroll_bot;
    if (ctx->term.cursor_row > bot) return;

    int16_t rem = bot - ctx->term.cursor_row + 1;
    if (n > rem) n = rem;
    int16_t move_cnt = rem - n;

    for (int16_t i = move_cnt - 1; i >= 0; --i) {
        uint32_t dst_idx = _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row + n + i);
        uint32_t src_idx = _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row + i);
        memcpy(&ctx->term.cells[dst_idx], &ctx->term.cells[src_idx],
               ctx->term.cols * sizeof(sfte_cell));
    }

    _sfte_grid_clear_rows(ctx, ctx->term.cursor_row, n);
}

/*
    Handles Delete Line / CSI M.

    Deletes n lines at the cursor, pulling bottom lines up.
*/
static inline void _sfte_csi_exec_dl(sfte_ctx *ctx, uint16_t *p) {
    uint16_t n = _SFTE_P(p[0]);
    int16_t bot = ctx->term.scroll_bot;
    if (ctx->term.cursor_row > bot) return;

    int16_t rem = bot - ctx->term.cursor_row + 1;
    if (n > rem) n = rem;
    int16_t move_cnt = rem - n;

    for (int16_t i = 0; i < move_cnt; ++i) {
        uint32_t dst_idx = _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row + i);
        uint32_t src_idx = _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row + n + i);
        memcpy(&ctx->term.cells[dst_idx], &ctx->term.cells[src_idx],
               ctx->term.cols * sizeof(sfte_cell));
    }

    _sfte_grid_clear_rows(ctx, bot - n + 1, n);
}

/*
    Handles Delete Character / CSI P.

    Deletes n characters at the cursor, shifting right text left.
*/
static inline void _sfte_csi_exec_dch(sfte_ctx *ctx, uint16_t *p, int16_t col) {
    uint16_t n = _SFTE_P(p[0]);
    int16_t rem = ctx->term.cols - col;
    if (n > rem) n = rem;
    int16_t move_cnt = rem - n;
    int32_t base_idx = _sfte_grid_get_idx(ctx, 0, ctx->term.cursor_row);
    if (move_cnt > 0)
        memmove(&ctx->term.cells[base_idx + col], &ctx->term.cells[base_idx + col + n],
                move_cnt * sizeof(sfte_cell));
    _sfte_grid_clear_cells(ctx, base_idx + ctx->term.cols - n, n);
    _sfte_grid_dirty_range(ctx, base_idx + col, rem);
}

/*
    Handles Erase Character / CSI X.

    Replaces n characters with spaces starting at the cursor.
*/
static inline void _sfte_csi_exec_ech(sfte_ctx *ctx, uint16_t *p, int16_t col) {
    uint16_t n = _SFTE_P(p[0]);
    int16_t rem = ctx->term.cols - col;
    if (n > rem) n = rem;
    _sfte_grid_clear_cells(ctx, _sfte_grid_get_idx(ctx, col, ctx->term.cursor_row), n);
}

/*
    Handles Device Attributes / CSI c.

    Reports the terminals identity and capabilities to the host.

    TODO:
    Add identity to customization. This might be useful especially for custom backends and such.
*/
static inline void _sfte_csi_exec_da(sfte_ctx *ctx) {
    if (ctx->term.vt_dec_priv == 2) {
        const char *sda = "\033[>0;95;0c";
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, sda, strlen(sda));
    } else {
        const char *da = "\033[?62c";
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, da, strlen(da));
    }
}

/*
    Handles Vertical Position Absolute / CSI d.

    Moves cursor to the specific row n.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_vpa(sfte_ctx *ctx, uint16_t *p) {
    if (ctx->term.origin_mode)
        ctx->term.cursor_row = _SFTE_CLAMP(_SFTE_P_IDX(p[0]) + ctx->term.scroll_top,
                                           ctx->term.scroll_top, ctx->term.scroll_bot);
    else
        ctx->term.cursor_row = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, ctx->term.rows - 1);
}

/*
    Handles Horizontal Vertical Position / CSI f.

    Moves cursor to row n, column m.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_hvp(sfte_ctx *ctx, uint16_t *p) {
    ctx->term.cursor_col = _SFTE_CLAMP(_SFTE_P_IDX(p[1]), 0, ctx->term.cols - 1);
    if (ctx->term.origin_mode)
        ctx->term.cursor_row = _SFTE_CLAMP(_SFTE_P_IDX(p[0]) + ctx->term.scroll_top,
                                           ctx->term.scroll_top, ctx->term.scroll_bot);
    else
        ctx->term.cursor_row = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, ctx->term.rows - 1);
#if SFTE_CURSOR_TRAIL
    ctx->term.warp_tail = 1;
#endif  // SFTE_CURSOR_TRAIL
}

/*
    Handles Tab Clear / CSI g.

    Clears tab stops at current column (0) or all columns (3).
*/
static inline void _sfte_csi_exec_tbc(sfte_ctx *ctx, uint16_t *p) {
    if (p[0] == 0)
        ctx->term.tab_stops[ctx->term.cursor_col] = 0;
    else if (p[0] == 3)
        memset(ctx->term.tab_stops, 0, ctx->term.cols);
}

/*
    Handles Set Mode / CSI h.

    Enables various terminal modes.
    Supports DECTCEM (Cursor Show), DECAWM (Auto-Wrap),
    DECOM (Origin Mode), and alt screen buffer toggles.
*/
static inline void _sfte_csi_set_mode(sfte_ctx *ctx, uint16_t *p, uint16_t cnt, int16_t col) {
    if (!ctx->term.vt_dec_priv) return;

    for (int i = 0; i < cnt; ++i) {
        if (p[i] == 25) {
            ctx->term.hide_cursor = 0;
            ctx->term.cells[_sfte_grid_get_idx(ctx, col, ctx->term.cursor_row)].dirty = 1;
        }
#if SFTE_TERM_FOCUS
        else if (p[i] == 1004)
            ctx->term.report_focus = 1;
#endif  // SFTE_TERM_FOCUS
        else if (p[i] == 2004)
            ctx->term.bracketed_paste = 1;
        else if (p[i] == 7)
            ctx->term.auto_wrap = 1;
        else if (p[i] == 6) {
            ctx->term.origin_mode = 1;
            ctx->term.cursor_col = 0;
            ctx->term.cursor_row = ctx->term.scroll_top;
        } else if (p[i] == 1047 || p[i] == 1048 || p[i] == 1049) {
            // 1048 / 1049 save cursor
            if (p[i] == 1048 || p[i] == 1049) {
                uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
                s_idx = ctx->term.alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
                ctx->term.saved_grid_off[s_idx] = ctx->term.grid_off;
                ctx->term.saved_col[s_idx] = ctx->term.cursor_col;
                ctx->term.saved_row[s_idx] = ctx->term.cursor_row;
                ctx->term.saved_fg[s_idx] = ctx->term.cur_fg;
                ctx->term.saved_bg[s_idx] = ctx->term.cur_bg;
                ctx->term.saved_attr[s_idx] = ctx->term.cur_attr;
            }

#if SFTE_TERM_ALT_SCREEN
            // 1047 / 1049 switch to alt screen
            if ((p[i] == 1047 || p[i] == 1049) && !ctx->term.alt_active) {
                ctx->term.alt_active = 1;
#if SFTE_INPUT_KITTY
                ctx->term.kitty_kb_stack_idx[1] = 0;
                ctx->term.kitty_kb_stack[1][0] = 0;
#endif  // SFTE_INPUT_KITTY
#if SFTE_CURSOR_TRAIL
                ctx->term.last_move_ms = 0;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
                ctx->term.is_animating = 1;
                ctx->term.anim_dir = 1;
                ctx->term.anim_start_ms = SFTE_TIME_MS();
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN

                if (!ctx->term.alt_cells)
                    ctx->term.alt_cells = (sfte_cell *)SFTE_CALLOC(ctx->term.cols * ctx->term.rows,
                                                                   sizeof(sfte_cell));

                sfte_cell *tmp = ctx->term.cells;
                ctx->term.cells = ctx->term.alt_cells;
                ctx->term.alt_cells = tmp;
            }
#endif  // SFTE_TERM_ALT_SCREEN

            if (p[i] == 1049) {
                _sfte_grid_clear_cells(ctx, 0, ctx->term.cols * ctx->term.rows);
                ctx->term.cursor_col = 0;
                ctx->term.cursor_row = 0;
            } else if (p[i] == 1047) {
                _sfte_grid_dirty_range(ctx, 0, ctx->term.cols * ctx->term.rows);
            }
        }
#if SFTE_INPUT_MOUSE
        else if (p[i] == 1000 || p[i] == 1002 || p[i] == 1003)
            ctx->term.mouse_mode = p[i];
        else if (p[i] == 1006)
            ctx->term.mouse_ext = 1006;
#endif  // SFTE_INPUT_MOUSE
    }
}

/*
    Handles Reset Mode / CSI l

    Disables various terminal modes.
    Matches the implementations found in SM.
*/
static inline void _sfte_csi_reset_mode(sfte_ctx *ctx, uint16_t *p, uint16_t cnt, int16_t col) {
    if (!ctx->term.vt_dec_priv) return;

    for (int i = 0; i < cnt; ++i) {
        if (p[i] == 25) {
            ctx->term.hide_cursor = 1;
            ctx->term.cells[_sfte_grid_get_idx(ctx, col, ctx->term.cursor_row)].dirty = 1;
        }
#if SFTE_TERM_FOCUS
        else if (p[i] == 1004)
            ctx->term.report_focus = 0;
#endif  // SFTE_TERM_FOCUS
        else if (p[i] == 2004)
            ctx->term.bracketed_paste = 0;
        else if (p[i] == 7)
            ctx->term.auto_wrap = 0;
        else if (p[i] == 6) {
            ctx->term.origin_mode = 0;
            ctx->term.cursor_col = 0;
            ctx->term.cursor_row = 0;
        } else if (p[i] == 1047 || p[i] == 1048 || p[i] == 1049) {
#if SFTE_TERM_ALT_SCREEN
            if ((p[i] == 1047 || p[i] == 1049) && ctx->term.alt_active) {
                ctx->term.alt_active = 0;
#if SFTE_INPUT_KITTY
                ctx->term.kitty_kb_stack_idx[1] = 0;
                ctx->term.kitty_kb_stack[1][0] = 0;
#endif  // SFTE_INPUT_KITTY
#if SFTE_TERM_ANIMATE_SCREEN
                ctx->term.is_animating = 1;
                ctx->term.anim_dir = -1;
                ctx->term.anim_start_ms = SFTE_TIME_MS();
#endif  // SFTE_TERM_ANIMATE_SCREEN

                if (ctx->term.alt_cells) {
                    sfte_cell *tmp = ctx->term.cells;
                    ctx->term.cells = ctx->term.alt_cells;
                    ctx->term.alt_cells = tmp;
                    _sfte_grid_dirty_range(ctx, 0, ctx->term.cols * ctx->term.rows);
                }

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
                // Destroy all images created on alt screen
                for (uint32_t j = 0; j < ctx->term.img_placements_len; ++j) {
                    if (!ctx->term.img_placements[j].alt_screen) continue;
                    for (uint32_t k = 0; k < ctx->term.img_pool_len; ++k)
                        if (ctx->term.img_pool[k].id == ctx->term.img_placements[j].img_id) {
                            ctx->term.img_pool[k].ref_cnt--;
                            break;
                        }
                    ctx->term
                        .img_placements[j--] = ctx->term
                                                   .img_placements[--ctx->term.img_placements_len];
                }
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
            }
#endif  // SFTE_TERM_ALT_SCREEN

            if (p[i] == 1048 || p[i] == 1049) {
                uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
                s_idx = ctx->term.alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
                ctx->term.grid_off = ctx->term.saved_grid_off[s_idx];
                ctx->term.cursor_col = _SFTE_CLAMP(ctx->term.saved_col[s_idx], 0,
                                                   ctx->term.cols - 1);
                ctx->term.cursor_row = _SFTE_CLAMP(ctx->term.saved_row[s_idx], 0,
                                                   ctx->term.rows - 1);

                // force external output below prompt
                if (p[i] == 1049) {
                    ctx->term.cursor_col = 0;
                    if (ctx->term.cursor_row == ctx->term.scroll_bot)
                        _sfte_grid_scroll(ctx, 1);
                    else if (ctx->term.cursor_row == ctx->term.rows - 1) {
                        int old_t = ctx->term.scroll_top;
                        int old_b = ctx->term.scroll_bot;
                        ctx->term.scroll_top = 0;
                        ctx->term.scroll_bot = ctx->term.rows - 1;
                        _sfte_grid_scroll(ctx, 1);
                        ctx->term.scroll_top = old_t;
                        ctx->term.scroll_bot = old_b;
                    } else if (ctx->term.cursor_row < ctx->term.rows - 1)
                        ctx->term.cursor_row++;
                }

                ctx->term.cur_fg = ctx->term.saved_fg[s_idx];
                ctx->term.cur_bg = ctx->term.saved_bg[s_idx];
                ctx->term.cur_attr = ctx->term.saved_attr[s_idx];
                ctx->term.cells[_sfte_grid_get_idx(ctx, ctx->term.cursor_col, ctx->term.cursor_row)]
                    .dirty = 1;

#if SFTE_CURSOR_TRAIL
                ctx->term.last_move_ms = 0;
                ctx->term.is_trailing = 0;
                ctx->term.tail_rx = ctx->term.cursor_col * ctx->font.cell_width;
                ctx->term.tail_ry = ctx->term.cursor_row * ctx->font.cell_height;
                ctx->term.trail_dmg.w = 0;
#endif  // SFTE_CURSOR_TRAIL
            }
        }
#if SFTE_INPUT_MOUSE
        else if (p[i] == 1000 || p[i] == 1002 || p[i] == 1003)
            ctx->term.mouse_mode = 0;
        else if (p[i] == 1006)
            ctx->term.mouse_ext = 0;
#endif  // SFTE_INPUT_MOUSE
    }
}

/*
    Handles Select Graphic Rendition / CSI m

    Sets colors and style of the characters following this code.
*/
static inline void _sfte_csi_exec_sgr(sfte_ctx *ctx, uint16_t *p, uint16_t cnt) {
    if (cnt == 0) {
        cnt = 1;
        p[0] = 0;
    }

    for (int16_t i = 0; i < cnt; ++i) {
        uint16_t code = p[i];

        switch (code) {
        case 0:  // Reset all
            ctx->term.cur_fg = _SFTE_COLOR_FG_DEFAULT;
            ctx->term.cur_bg = _SFTE_COLOR_BG_DEFAULT;
            ctx->term.cur_attr = 0;
#if SFTE_UNDERLINE_COLORED
            ctx->term.cur_ul_color = _SFTE_COLOR_FG_DEFAULT;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_UNDERLINE_EXTENDED
            ctx->term.cur_ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
            break;
        case 1: ctx->term.cur_attr |= _SFTE_ATTR_BOLD; break;
        case 2: /* TODO: add _SFTE_ATTR_DIM */ break;
        case 3: ctx->term.cur_attr |= _SFTE_ATTR_ITALIC; break;
        case 4: ctx->term.cur_attr |= _SFTE_ATTR_UNDERLINE;
#if SFTE_UNDERLINE_EXTENDED
            ctx->term.cur_ul_style = _SFTE_UNDERLINE_STYLE_STRAIGHT;
#endif  // SFTE_UNDERLINE_EXTENDED
            break;
        case 7: ctx->term.cur_attr |= _SFTE_ATTR_REVERSE; break;
        case 22: ctx->term.cur_attr &= ~_SFTE_ATTR_BOLD; break;
        case 23: ctx->term.cur_attr &= ~_SFTE_ATTR_ITALIC; break;
        case 24: ctx->term.cur_attr &= ~_SFTE_ATTR_UNDERLINE;
#if SFTE_UNDERLINE_EXTENDED
            ctx->term.cur_ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
            break;
        case 27: ctx->term.cur_attr &= ~_SFTE_ATTR_REVERSE; break;
        case 39: ctx->term.cur_fg = _SFTE_COLOR_FG_DEFAULT; break;
        case 49: ctx->term.cur_bg = _SFTE_COLOR_BG_DEFAULT; break;
#if SFTE_UNDERLINE_COLORED
        case 59: ctx->term.cur_ul_color = ctx->term.cur_fg; break;
#endif            // SFTE_UNDERLINE_COLORED
        case 38:  // FG
        case 48:  // BG
#if SFTE_UNDERLINE_COLORED
        case 58:  // Underline
#endif            // SFTE_UNDERLINE_COLORED
        {
            if (i + 2 < cnt && p[i + 1] == 5) {
                uint32_t color = _sfte_color_from_idx(p[i + 2]);
                if (code == 38)
                    ctx->term.cur_fg = color;
                else if (code == 48)
                    ctx->term.cur_bg = color;
                i += 2;
            } else if (i + 4 < cnt && p[i + 1] == 2) {
                // Truecolor
                uint32_t rgb = _sfte_csi_parse_truecolor(p, i);
                uint32_t color = _sfte_color_from_rgb(rgb);
                if (code == 38)
                    ctx->term.cur_fg = color;
                else if (code == 48)
                    ctx->term.cur_bg = color;
#if SFTE_UNDERLINE_COLORED
                else if (code == 58)
                    ctx->term.cur_ul_color = color;
#endif  // SFTE_UNDERLINE_COLORED
                i += 4;
            }
            break;
        }
        default: {
            if (code >= 30 && code <= 37)
                ctx->term.cur_fg = _sfte_color_from_idx(code - 30);
            else if (code >= 90 && code <= 97)
                ctx->term.cur_fg = _sfte_color_from_idx((code - 90) + 8);
            else if (code >= 40 && code <= 47)
                ctx->term.cur_bg = _sfte_color_from_idx(code - 40);
            else if (code >= 100 && code <= 107)
                ctx->term.cur_bg = _sfte_color_from_idx((code - 100) + 8);
            break;
        }
        }
    }
}

/*
    Handles Device Status Report / CSI n.

    Reports cursor position (6) or terminal status (5).
*/
static inline void _sfte_csi_exec_dsr(sfte_ctx *ctx, uint16_t *p) {
    if (p[0] == 5) {
        const char *reply = "\033[0n";
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, strlen(reply));
    } else if (p[0] == 6) {
        char buf[32];
        size_t len = snprintf(buf, sizeof(buf), "\033[%d;%dR", ctx->term.cursor_row + 1,
                              ctx->term.cursor_col + 1);
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, buf, len);
    }
}

/*
    Handles Soft Terminal Reset / CSI p.

    Resets terminal state to default values.
*/
static inline void _sfte_csi_exec_decstr(sfte_ctx *ctx, int16_t col) {
#if SFTE_INPUT_MOUSE
    ctx->term.mouse_mode = 0;
    ctx->term.mouse_ext = 0;
#endif  // SFTE_INPUT_MOUSE
#if SFTE_INPUT_KITTY
    ctx->term.kitty_kb_stack_idx[0] = 0;
    ctx->term.kitty_kb_stack_idx[1] = 0;
    ctx->term.kitty_kb_stack[0][0] = 0;
    ctx->term.kitty_kb_stack[1][0] = 0;
#endif  // SFTE_INPUT_KITTY
#if SFTE_CURSOR_BLINK
    ctx->term.blink_enabled = 1;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    ctx->term.last_move_ms = 0;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_CURSOR_DYNAMIC
    ctx->term.cursor_color = SFTE_CURSOR_COLOR;
    ctx->term.cursor_style = SFTE_CURSOR_STYLE;
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_UNDERLINE_COLORED
    ctx->term.cur_ul_color = _SFTE_COLOR_FG_DEFAULT;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_UNDERLINE_EXTENDED
    ctx->term.cur_ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
#if SFTE_INPUT_HYPERLINKS
    ctx->term.cur_link_idx = 0;
#endif  // SFTE_INPUT_HYPERLINKS
    ctx->term.scroll_top = 0;
    ctx->term.scroll_bot = ctx->term.rows - 1;
    ctx->term.cur_fg = _SFTE_COLOR_FG_DEFAULT;
    ctx->term.cur_bg = _SFTE_COLOR_BG_DEFAULT;
    ctx->term.cur_attr = 0;
    ctx->term.hide_cursor = 0;
    ctx->term.cells[_sfte_grid_get_idx(ctx, col, ctx->term.cursor_row)].dirty = 1;
}

/*
    Handles Set Cursor Style / CSI q.

    Changes the cursor shape and blinking style.
*/
static inline void _sfte_csi_exec_decscusr(sfte_ctx *ctx, uint16_t *p, int16_t col) {
#if SFTE_CURSOR_BLINK
    switch (p[0]) {
    case 0:
    case 1:
    case 3:
    case 5: ctx->term.blink_enabled = 1; break;
    case 2:
    case 4:
    case 6: ctx->term.blink_enabled = 0; break;
    }
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_DYNAMIC
    switch (p[0]) {
    case 0: ctx->term.cursor_style = SFTE_CURSOR_STYLE; break;
    case 1:
    case 2: ctx->term.cursor_style = SFTE_CURSOR_STYLE_BLOCK; break;
    case 3:
    case 4: ctx->term.cursor_style = SFTE_CURSOR_STYLE_UNDERLINE; break;
    case 5:
    case 6: ctx->term.cursor_style = SFTE_CURSOR_STYLE_BAR; break;
    }
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_CURSOR_BLINK || SFTE_CURSOR_DYNAMIC
    ctx->term.cells[_sfte_grid_get_idx(ctx, col, ctx->term.cursor_row)].dirty = 1;
#endif  // SFTE_CURSOR_BLINK || SFTE_CURSOR_DYNAMIC
}

/*
    Handles Set Top and Bottom Margins / CSI r.

    Sets the scrolling region top and bottom margins.

    NOTE:
    Respects origin mode.
*/
static inline void _sfte_csi_exec_decstbm(sfte_ctx *ctx, uint16_t *p, uint16_t cnt) {
    uint16_t top = _SFTE_P_IDX(p[0]);
    int16_t bot = (cnt > 1 && p[1] > 0 ? p[1] : ctx->term.rows) - 1;
    if (bot >= ctx->term.rows) bot = ctx->term.rows - 1;
    if (top < bot) {
        ctx->term.scroll_top = top;
        ctx->term.scroll_bot = bot;
    }
    ctx->term.cursor_col = 0;
    ctx->term.cursor_row = ctx->term.origin_mode ? ctx->term.scroll_top : 0;
}

/*
    Handles Save Cursor / CSI s.

    Saves the current cursor position and attributes.
*/
static inline void _sfte_csi_exec_scosc(sfte_ctx *ctx, uint16_t *p) {
    if (p[0] != 0) return;  // Avoid colliding with kitty support command
    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = ctx->term.alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    ctx->term.saved_col[s_idx] = ctx->term.cursor_col;
    ctx->term.saved_row[s_idx] = ctx->term.cursor_row;
    ctx->term.saved_fg[s_idx] = ctx->term.cur_fg;
    ctx->term.saved_bg[s_idx] = ctx->term.cur_bg;
    ctx->term.saved_attr[s_idx] = ctx->term.cur_attr;
}

/*
    Handles Window Manipulation / CSI t.

    Xterm extension for querying or pushing/popping window titles.
*/
static inline void _sfte_csi_exec_xtwinops(sfte_ctx *ctx, uint16_t *p) {
    if (p[1] != 0 && p[1] != 2) return;
    if (p[0] == 22)
        snprintf(ctx->term.saved_title, sizeof(ctx->term.saved_title), "%s", ctx->term.title);
    else if (p[0] == 23) {
        snprintf(ctx->term.title, sizeof(ctx->term.title), "%s", ctx->term.saved_title);
        // TODO:
        // Make a callback that gets called here for custom backends support.
#if SFTE_WAYLAND
        sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;
        xdg_toplevel_set_title(app->xdg_toplevel, ctx->term.title);
#endif  // SFTE_WAYLAND
    }
}

/*
    Handles Restore Cursor / CSI u.

    Restores the previously saved cursor position and attributes.
*/
static inline void _sfte_csi_exec_scorc(sfte_ctx *ctx, uint16_t *p) {
    if (ctx->term.vt_dec_priv != 0 || p[0] != 0) return;
    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = ctx->term.alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    ctx->term.cursor_col = _SFTE_CLAMP(ctx->term.saved_col[s_idx], 0, ctx->term.cols - 1);
    ctx->term.cursor_row = _SFTE_CLAMP(ctx->term.saved_row[s_idx], 0, ctx->term.rows - 1);
    ctx->term.cur_fg = ctx->term.saved_fg[s_idx];
    ctx->term.cur_bg = ctx->term.saved_bg[s_idx];
    ctx->term.cur_attr = ctx->term.saved_attr[s_idx];
}

#if SFTE_INPUT_KITTY
/*
    Handles Kitty Keyboard Protocol / CSI u extension.

    The VT500 standard defines 'u' as SCORC (Restore Cursor) if no modifiers are present.
    However, the modern kitty keyboard protocol overloads 'u' to manage
    the keyboard flag stack when preceded by >, =, < or ?.
*/
static inline void _sfte_csi_exec_kitty(sfte_ctx *ctx, uint16_t *p) {
    if (ctx->term.vt_dec_priv == 0) return;
    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = ctx->term.alt_active;
#endif                                 // SFTE_TERM_ALT_SCREEN
    if (ctx->term.vt_dec_priv == 1) {  // CSI ? u (query)
        char buf[32];
        uint16_t flags = ctx->term.kitty_kb_stack[s_idx][ctx->term.kitty_kb_stack_idx[s_idx]];
        size_t len = snprintf(buf, sizeof(buf), "\033[?%du", flags);
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, buf, len);
    } else if (ctx->term.vt_dec_priv == 2) {  // CSI > flags u (push)
        if (ctx->term.kitty_kb_stack_idx[s_idx] < 15) ctx->term.kitty_kb_stack_idx[s_idx]++;
        ctx->term.kitty_kb_stack[s_idx][ctx->term.kitty_kb_stack_idx[s_idx]] = p[0];
    } else if (ctx->term.vt_dec_priv == 3)  // CSI < n u (pop)
        ctx->term.kitty_kb_stack_idx[s_idx] -= (p[0] > 0) ? p[0] : 1;
    else if (ctx->term.vt_dec_priv == 4)  // CSI = flags u (set/overwrite)
        ctx->term.kitty_kb_stack[s_idx][ctx->term.kitty_kb_stack_idx[s_idx]] = p[0];
}
#endif  // SFTE_INPUT_KITTY

/*
    The main routing switch for Control Sequence Introducer events.
*/
static inline void _sfte_csi_dispatch(sfte_ctx *ctx, uint8_t cmd) {
    uint16_t *p = ctx->term.vt_params;
    int16_t cnt = ctx->term.vt_param_idx + 1;
    int16_t col = ctx->term.cursor_col >= ctx->term.cols ? ctx->term.cols - 1
                                                         : ctx->term.cursor_col;

    switch (cmd) {
    case '@': _sfte_csi_exec_ich(ctx, p, col); break;
    case 'A':  // CUU / Cursor Up
        ctx->term.cursor_row = _SFTE_CLAMP(ctx->term.cursor_row - _SFTE_P(p[0]), 0,
                                           ctx->term.rows - 1);
        break;
    case 'B':  // CUD / Cursor Down
        ctx->term.cursor_row = _SFTE_CLAMP(ctx->term.cursor_row + _SFTE_P(p[0]), 0,
                                           ctx->term.rows - 1);
        break;
    case 'C':  // CUF / Cursor Forward
        ctx->term.cursor_col = _SFTE_CLAMP(ctx->term.cursor_col + _SFTE_P(p[0]), 0,
                                           ctx->term.cols - 1);
        break;
    case 'D':  // CUB / Cursor Back
        ctx->term.cursor_col = _SFTE_CLAMP(ctx->term.cursor_col - _SFTE_P(p[0]), 0,
                                           ctx->term.cols - 1);
        break;
    case 'E': _sfte_csi_exec_cnl(ctx, p); break;
    case 'F': _sfte_csi_exec_cpl(ctx, p); break;
    case 'G':  // CHA / Cursor Horizontal Absolute
        ctx->term.cursor_col = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, ctx->term.cols - 1);
        break;
    case 'H':  // CUP / Cursor Position
        ctx->term.cursor_col = _SFTE_CLAMP(_SFTE_P_IDX(p[1]), 0, ctx->term.cols - 1);
        ctx->term.cursor_row = _SFTE_CLAMP(_SFTE_P_IDX(p[0]), 0, ctx->term.rows - 1);
        break;
    case 'J':
        if (ctx->term.vt_dec_priv == 0)
            for (int i = 0; i < cnt; ++i) _sfte_csi_exec_ed(ctx, p[i], col);
        break;
    case 'K':
        if (ctx->term.vt_dec_priv == 0)
            for (int i = 0; i < cnt; ++i) _sfte_csi_exec_el(ctx, p[i], col);
        break;
    case 'L': _sfte_csi_exec_il(ctx, p); break;
    case 'M': _sfte_csi_exec_dl(ctx, p); break;
    case 'P': _sfte_csi_exec_dch(ctx, p, col); break;
    case 'S': _sfte_grid_scroll(ctx, _SFTE_P(p[0])); break;
    case 'T': _sfte_grid_scroll(ctx, -_SFTE_P(p[0])); break;
    case 'X': _sfte_csi_exec_ech(ctx, p, col); break;
    case 'c':
        if (ctx->term.vt_dec_priv == 0) _sfte_csi_exec_da(ctx);
        break;
    case 'd': _sfte_csi_exec_vpa(ctx, p); break;
    case 'f': _sfte_csi_exec_hvp(ctx, p); break;
    case 'g': _sfte_csi_exec_tbc(ctx, p); break;
    case 'h': _sfte_csi_set_mode(ctx, p, cnt, col); break;
    case 'l': _sfte_csi_reset_mode(ctx, p, cnt, col); break;
    case 'm':
        if (ctx->term.vt_dec_priv == 0) _sfte_csi_exec_sgr(ctx, p, cnt);
        break;
    case 'n': _sfte_csi_exec_dsr(ctx, p); break;
    case 'p': _sfte_csi_exec_decstr(ctx, col); break;
    case 'q': _sfte_csi_exec_decscusr(ctx, p, col); break;
    case 'r': _sfte_csi_exec_decstbm(ctx, p, cnt); break;
    case 's':
        if (ctx->term.vt_dec_priv == 0) _sfte_csi_exec_scosc(ctx, p);
        break;
    case 't': _sfte_csi_exec_xtwinops(ctx, p); break;
    case 'u':
        if (ctx->term.vt_dec_priv == 0) _sfte_csi_exec_scorc(ctx, p);
#if SFTE_INPUT_KITTY
        else
            _sfte_csi_exec_kitty(ctx, p);
#endif  // SFTE_INPUT_KITTY
        break;
    }
    _SFTE_WARN(ctx, UNHANDLED_CSI, cmd, cnt);
}
// =================================================================================================
// >>parser
// =================================================================================================
typedef enum {
    VT_GROUND,     // normal
    VT_ESCAPE,     // \033
    VT_CSI_ENTRY,  // \033[
    VT_CSI_PARAM,  // nums
    VT_OSC,        // \033]
    VT_CHARSET,    // \033( \033)
    VT_HASH,       // #
    VT_DCS,        // P / _ / ^
#if SFTE_IMG_SIXEL
    VT_SIXEL,
#endif  // SFTE_IMG_SIXEL
} sfte_vt_state;

#if SFTE_CURSOR_DYNAMIC
/*
    Parses an X11/OSC color string into a 24-bit RGB uint32_t.
    Supports #RRGGBB and rgb:R/G/B.
    Returns the `fallback` color if parsing fails.
*/
static inline uint32_t _sfte_parser_osc_color(const char *str, uint32_t fallback) {
    if (!str || !*str) return fallback;

    if (str[0] == '#') {  // #RRGGBB
        uint32_t val = 0;
        for (uint8_t i = 1; i <= 6; ++i) {
            char c = str[i];
            val <<= 4;
            if (c >= '0' && c <= '9')
                val |= (c - '0');
            else if (c >= 'a' && c <= 'f')
                val |= (c - 'a' + 10);
            else if (c >= 'A' && c <= 'F')
                val |= (c - 'A' + 10);
            else
                return fallback;
        }
        return val;
    }

    if (strncmp(str, "rgb:", 4) == 0) {  // rgb:R/G/B
        const char *p = str + strlen("rgb:");
        uint32_t rgb = 0;
        for (uint8_t c = 0; c < 3; ++c) {
            uint32_t channel = 0;
            uint8_t digits = 0;
            while (*p != '/' && *p != '\0' && digits < 4) {
                char ch = *p++;
                channel <<= 4;
                if (ch >= '0' && ch <= '9')
                    channel |= (ch - '0');
                else if (ch >= 'a' && ch <= 'f')
                    channel |= (ch - 'a' + 10);
                else if (ch >= 'A' && ch <= 'F')
                    channel |= (ch - 'A' + 10);
                else
                    return fallback;
                digits++;
            }
            if (*p == '/') p++;

            // If 16 bit channel, truncate to 8 bit
            if (digits > 2) channel >>= (digits - 2) * 4;
            rgb = (rgb << 8) | (channel & 0xFF);
        }
        return rgb;
    }

    return fallback;
}
#endif  // SFTE_CURSOR_DYNAMIC

/*
    Appends a byte to the shared OSC/DCS payload buffer.
    If the buffer is too small, reallocates it with size doubled,
    unless it's already at its max capacity.
*/
static inline void _sfte_parser_append_payload(sfte_ctx *ctx, uint8_t b) {
    if (ctx->term.osc_len + 1 >= ctx->term.osc_cap && ctx->term.osc_cap < SFTE_OSC_MAX_CAP) {
        ctx->term.osc_cap *= 2;
        ctx->term.osc_payload = (char *)SFTE_REALLOC(ctx->term.osc_payload, ctx->term.osc_cap);
    }
    if (ctx->term.osc_len + 1 < ctx->term.osc_cap) ctx->term.osc_payload[ctx->term.osc_len++] = b;
}

/*
    Handles LF, VT, FF (Linefeed).

    Scrolls if at bottom margin.
*/
static inline void _sfte_parser_c0_lf(sfte_ctx *ctx) {
    if (ctx->term.cursor_row == ctx->term.scroll_bot)
        _sfte_grid_scroll(ctx, 1);
    else if (ctx->term.cursor_row == ctx->term.rows - 1) {
        // Temporarily set scroll values to bottom to safely scroll
        int16_t old_top = ctx->term.scroll_top;
        int16_t old_bot = ctx->term.scroll_bot;
        ctx->term.scroll_top = 0;
        ctx->term.scroll_bot = ctx->term.rows - 1;
        _sfte_grid_scroll(ctx, 1);
        ctx->term.scroll_top = old_top;
        ctx->term.scroll_bot = old_bot;
    } else if (ctx->term.cursor_row < ctx->term.rows - 1)
        ctx->term.cursor_row++;
}

/*
    Handles HT (Horizontal Tab).

    Advances to the next set tab stop.
*/
static inline void _sfte_parser_c0_ht(sfte_ctx *ctx) {
    while (ctx->term.cursor_col < ctx->term.cols - 1)
        if (ctx->term.tab_stops[ctx->term.cursor_col++]) break;
}

/*
    Handles RIS (Reset to Initial State) ESC c.

    Reuses DECSTR logic.
*/
static inline void _sfte_parser_esc_ris(sfte_ctx *ctx) {
    _sfte_csi_dispatch(ctx, 'p');
    ctx->term.cursor_col = 0, ctx->term.cursor_row = 0;
    _sfte_grid_clear_cells(ctx, 0, ctx->term.cols * ctx->term.rows);
}

/*
    Handles SC (Save Cursor) / ESC 7.

    Temporarily stores the data (cursor position, foreground/background color, attribute)
    to load it via RC later.

    The terminal can store one set of data per screen (main/alt).
*/
static inline void _sfte_parser_esc_sc(sfte_ctx *ctx) {
    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = ctx->term.alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    ctx->term.saved_col[s_idx] = ctx->term.cursor_col;
    ctx->term.saved_row[s_idx] = ctx->term.cursor_row;
    ctx->term.saved_fg[s_idx] = ctx->term.cur_fg;
    ctx->term.saved_bg[s_idx] = ctx->term.cur_bg;
    ctx->term.saved_attr[s_idx] = ctx->term.cur_attr;
}

/*
    Handles RC (Restore Cursor) / ESC 8.

    Loads the data (cursor position, foreground/background color, attribute)
    stored by SC in sequence.

    The terminal can store one set of data per screen (main/alt).
*/
static inline void _sfte_parser_esc_rc(sfte_ctx *ctx) {
    uint8_t s_idx = 0;
#if SFTE_TERM_ALT_SCREEN
    s_idx = ctx->term.alt_active;
#endif  // SFTE_TERM_ALT_SCREEN
    // NOTE:
    // We need to clamp here since between SC and RC a resize could've occured.
    ctx->term.cursor_col = _SFTE_CLAMP(ctx->term.saved_col[s_idx], 0, ctx->term.cols - 1);
    ctx->term.cursor_row = _SFTE_CLAMP(ctx->term.saved_row[s_idx], 0, ctx->term.rows - 1);
    ctx->term.cur_fg = ctx->term.saved_fg[s_idx];
    ctx->term.cur_bg = ctx->term.saved_bg[s_idx];
    ctx->term.cur_attr = ctx->term.saved_attr[s_idx];
}

/*
    Handles IND (Index) / ESC D.

    Moves down, scrolling if at margin.
*/
static inline void _sfte_parser_esc_ind(sfte_ctx *ctx) {
    if (ctx->term.cursor_row == ctx->term.scroll_bot)
        _sfte_grid_scroll(ctx, 1);
    else if (ctx->term.cursor_row < ctx->term.rows - 1)
        ctx->term.cursor_row++;
}

/*
    Handles RI (Reverse Index) / ESC M.

    Moves up, scrolling (up) if at margin.
*/
static inline void _sfte_parser_esc_ri(sfte_ctx *ctx) {
    if (ctx->term.cursor_row == ctx->term.scroll_top)
        _sfte_grid_scroll(ctx, -1);
    else if (ctx->term.cursor_row > 0)
        ctx->term.cursor_row--;
}

/*
    Handles NEL (Next Line) / ESC E.

    Moves to start of the next line.

    Reuses Index logic to move down.
*/
static inline void _sfte_parser_esc_nel(sfte_ctx *ctx) {
    _sfte_parser_esc_ind(ctx);
    ctx->term.cursor_col = 0;
}

/*
    Handles DECALN (Screen Alignment Pattern) / ESC # 8.
*/
static inline void _sfte_parser_hash_decaln(sfte_ctx *ctx) {
    for (int32_t i = 0; i < ctx->term.cols * ctx->term.rows; ++i) {
        ctx->term.cells[i].rune = 'E';
        ctx->term.cells[i].fg = _SFTE_COLOR_FG_DEFAULT;
        ctx->term.cells[i].bg = _SFTE_COLOR_BG_DEFAULT;
        ctx->term.cells[i].attr = 0;
        ctx->term.cells[i].dirty = 1;
    }
    ctx->term.cursor_col = 0, ctx->term.cursor_row = 0;
}

/*
    Parses and executes completed OSC strings.
*/
static inline void _sfte_parser_osc_dispatch(sfte_ctx *ctx, uint8_t terminator) {
    const char *term = (terminator == '\x1b') ? "\033\\" : "\x07";
    ctx->term.osc_payload[ctx->term.osc_len] = '\0';

    if (strncmp(ctx->term.osc_payload, "10;?", 4) == 0 ||
        strncmp(ctx->term.osc_payload, "11;?", 4) == 0
#if SFTE_CURSOR_DYNAMIC
        || strncmp(ctx->term.osc_payload, "12;?", 4) == 0
#endif  // SFTE_CURSOR_DYNAMIC
    ) {
        uint8_t code_char = ctx->term.osc_payload[1];
        uint8_t code = (code_char == '0') ? 10 : ((code_char == '1') ? 11 : 12);
        uint32_t col = (code == 11) ? SFTE_COLOR_BG : SFTE_COLOR_FG;
#if SFTE_CURSOR_DYNAMIC
        if (code == 12) col = ctx->term.cursor_color;
#endif  // SFTE_CURSOR_DYNAMIC
        uint8_t cr = (col >> 16) & 0xFF, cg = (col >> 8) & 0xFF, cb = col & 0xFF;

        char reply[64];
        size_t len = snprintf(reply, sizeof(reply), "\033]%d;rgb:%02x%02x/%02x%02x/%02x%02x%s",
                              code, cr, cr, cg, cg, cb, cb, term);
        if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
    }
#if SFTE_CURSOR_DYNAMIC
    else if (strncmp(ctx->term.osc_payload, "12;", 3) == 0)
        ctx->term.cursor_color = _sfte_parser_osc_color(ctx->term.osc_payload + 3,
                                                        ctx->term.cursor_color);
    else if (strncmp(ctx->term.osc_payload, "112", 3) == 0)
        ctx->term.cursor_color = SFTE_CURSOR_COLOR;
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52
    else if (strncmp(ctx->term.osc_payload, "52;", 3) == 0) {  // Remote Clipboard (OSC 52)
        char *p = ctx->term.osc_payload + (strlen("52;") - 1);
        char target = (*p && *p != ';') ? *p : 'c';
        while (*p && *p != ';') p++;
        if (*p == ';' && *++p != '?' /* Skips read requests */) {
            size_t b64_len = ctx->term.osc_len - (p - ctx->term.osc_payload);
            size_t raw_len = 0;
            uint8_t *raw_data = _sfte_b64_decode(&ctx->stack, (uint8_t *)p, b64_len, &raw_len);
            if (raw_data) {
                size_t pre_off = _sfte_mem_stack_save(&ctx->stack);
                char *data = (char *)_sfte_mem_stack_alloc(&ctx->stack, raw_len + 1,
                                                           _Alignof(char));
                if (!data) {
                    _sfte_mem_stack_rewind(&ctx->stack, pre_off);
                    return;
                }

                memcpy(data, raw_data, raw_len);
                data[raw_len] = '\0';
                if (ctx->osc52_clipboard_cb) ctx->osc52_clipboard_cb(ctx->user_data, target, data);
                _sfte_mem_stack_rewind(&ctx->stack, pre_off);
            }
        }
    }
#endif  // SFTE_CLIPBOARD && SFTE_CLIPBOARD_OSC52
#if SFTE_INPUT_HYPERLINKS
    else if (strncmp(ctx->term.osc_payload, "8;", 2) == 0) {  // hyperlink
        char *p = ctx->term.osc_payload + (strlen("8;") - 1);
        while (*p && *p != ';') p++;
        if (*p++ == ';') {
            if (*p == '\0')  // Empty URI means close the link
                ctx->term.cur_link_idx = 0;
            else {  // Check if URI is already in pool
                uint16_t found_idx = 0;
                for (uint16_t i = 1; i < ctx->term.link_pool_len; ++i)
                    if (strcmp(ctx->term.link_pool[i], p) == 0) {
                        found_idx = i;
                        break;
                    }

                // Add new URI if not found
                if (found_idx == 0 && ctx->term.link_pool_len < SFTE_INPUT_HYPERLINKS_MAX_CAP) {
                    if (ctx->term.link_pool_len >= ctx->term.link_pool_cap) {
                        ctx->term.link_pool_cap *= 2;
                        ctx->term.link_pool = (char **)SFTE_REALLOC(
                            ctx->term.link_pool, ctx->term.link_pool_cap * sizeof(char *));
                    }
                    size_t ulen = strlen(p);
                    char *uri = (char *)SFTE_MALLOC(ulen + 1);
                    memcpy(uri, p, ulen + 1);
                    found_idx = ctx->term.link_pool_len++;
                    ctx->term.link_pool[found_idx] = uri;
                }
                ctx->term.cur_link_idx = found_idx;
            }
        }
    }
#endif  // SFTE_INPUT_HYPERLINKS
    else if (strncmp(ctx->term.osc_payload, "0;", 2) == 0 ||
             strncmp(ctx->term.osc_payload, "1;", 2) == 0 ||
             strncmp(ctx->term.osc_payload, "2;", 2) == 0) {
        char *title = ctx->term.osc_payload + 2;
        if (ctx->title_cb) ctx->title_cb(ctx->user_data, title);
    } else if (strncmp(ctx->term.osc_payload, "9;4;", 4) == 0) {
        char *p = ctx->term.osc_payload + 4;
        uint8_t state = *p - '0';
        while (*p && *p != ';') p++;
        uint8_t progress = 0;
        if (*p == ';') progress = atoi(p + 1);
        if (progress > 100) progress = 100;
        if (ctx->progress_cb) ctx->progress_cb(ctx->user_data, state, progress);
    } else
        _SFTE_WARN(ctx, UNHANDLED_OSC, ctx->term.osc_payload);
}

/*
    Parses and executes completed DCS payloads.

    If payload begins with 'G', payload is passed to kitty graphics extension.
*/
static inline void _sfte_parser_dcs_dispatch(sfte_ctx *ctx, uint8_t terminator) {
    const char *term = (terminator == '\x1b') ? "\033\\" : "\x07";
    ctx->term.osc_payload[ctx->term.osc_len] = '\0';

#if SFTE_IMG_KITTY
    if (ctx->term.osc_payload[0] == 'G')
        _sfte_kitty_parse_graphics(ctx, ctx->term.osc_payload + strlen("G"));
    else
#endif  // SFTE_IMG_KITTY
        if (strncmp(ctx->term.osc_payload, "+q", 2) == 0) {
            char reply[128];
            size_t len = snprintf(reply, sizeof(reply), "\033P0+r%s%s",
                                  ctx->term.osc_payload + strlen("+q"), term);
            if (ctx->write_cb) ctx->write_cb(ctx->user_data, reply, len);
        }
}

/*
    Core VT500 byte routing table.

    TODO:
    In-depth documentation of how this function behaves and why
*/
static inline void _sfte_parser_feed_byte(sfte_ctx *ctx, uint8_t b) {
    switch (b) {
    case '\n':
    case '\x0B':
    case '\x0C': _sfte_parser_c0_lf(ctx); return;
    case '\r': ctx->term.cursor_col = 0; return;
    case '\t': _sfte_parser_c0_ht(ctx); return;
    case '\b':
    case '\x7f':
        if (ctx->term.cursor_col > 0) ctx->term.cursor_col--;
        return;
    case '\a':
        if (ctx->bell_cb) ctx->bell_cb(ctx->user_data);
        return;
    default: break;
    }

    switch (ctx->term.parser_state) {
    case VT_GROUND:
        if (b == '\033' || b == '\x1b')
            ctx->term.parser_state = VT_ESCAPE;
        else if (b >= 0x20) {
#if SFTE_TERM_ASCII_CHARSET
            if (b >= 0x80) {
                if (_sfte_rune_utf8_decode(ctx, b))
                    b = '?';
                else
                    break;
            }
            _sfte_rune_insert(ctx, b);
#else   // !SFTE_TERM_ASCII_CHARSET
            if (_sfte_rune_utf8_decode(ctx, b)) _sfte_rune_insert(ctx, ctx->term.utf8_rune_acc);
#endif  // !SFTE_TERM_ASCII_CHARSET
        }
        break;
    case VT_ESCAPE:
        if (b == '[') {
            ctx->term.parser_state = VT_CSI_ENTRY;
            ctx->term.vt_param_idx = 0;
            ctx->term.vt_dec_priv = 0;
            memset(ctx->term.vt_params, 0, sizeof(ctx->term.vt_params));
        } else if (b == ']') {
            ctx->term.parser_state = VT_OSC;
            ctx->term.osc_len = 0;
        } else if (b == 'c') {
            _sfte_parser_esc_ris(ctx);
            ctx->term.parser_state = VT_GROUND;
        } else if (b == '\\')
            ctx->term.parser_state = VT_GROUND;
        else if (b == 'P' || b == '_' || b == '^') {
            ctx->term.parser_state = VT_DCS;
            ctx->term.osc_len = 0;
        } else if (b == '(' || b == ')')
            ctx->term.parser_state = VT_CHARSET;
        else if (b == '7') {
            _sfte_parser_esc_sc(ctx);
            ctx->term.parser_state = VT_GROUND;
        } else if (b == '8') {
            _sfte_parser_esc_rc(ctx);
            ctx->term.parser_state = VT_GROUND;
        } else if (b == '#')
            ctx->term.parser_state = VT_HASH;
        else if (b == 'D') {
            _sfte_parser_esc_ind(ctx);
            ctx->term.parser_state = VT_GROUND;
        } else if (b == 'M') {
            _sfte_parser_esc_ri(ctx);
            ctx->term.parser_state = VT_GROUND;
        } else if (b == 'E') {
            _sfte_parser_esc_nel(ctx);
            ctx->term.parser_state = VT_GROUND;
        } else
            ctx->term.parser_state = VT_GROUND;
        break;
    case VT_HASH:
        if (b == '8') _sfte_parser_hash_decaln(ctx);
        ctx->term.parser_state = VT_GROUND;
        break;
    case VT_CHARSET: ctx->term.parser_state = VT_GROUND; break;
    case VT_OSC:
        if (b == '\x07' || b == '\x1b') {
            _sfte_parser_osc_dispatch(ctx, b);
            ctx->term.parser_state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        } else
            _sfte_parser_append_payload(ctx, b);
        break;
    case VT_DCS:
        if (b == '\x07' || b == '\x1b') {
            _sfte_parser_dcs_dispatch(ctx, b);
            ctx->term.parser_state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        }
#if SFTE_IMG_SIXEL
        else if (b == 'q') {
            // Detect if this dcs header is strictly sixel params (nums/semicols)
            uint8_t is_sixel = 1;
            for (size_t i = 0; i < ctx->term.osc_len; ++i) {
                char pb = ctx->term.osc_payload[i];
                if (pb != ';' && (pb < '0' || pb > '9')) {
                    is_sixel = 0;
                    break;
                }
            }
            if (is_sixel) {
                ctx->term.parser_state = VT_SIXEL;
                ctx->sixel = (sfte_sixel_state){
                    .start_col = ctx->term.cursor_col,
                    .start_row = ctx->term.cursor_row,
                    .state = SIXEL_GROUND,
                };
            } else
                _sfte_parser_append_payload(ctx, b);
        }
#endif  // SFTE_IMG_SIXEL
        else
            _sfte_parser_append_payload(ctx, b);
        break;
#if SFTE_IMG_SIXEL
    case VT_SIXEL:
        if (b == '\x1b' || b == '\x07') {
            _sfte_sixel_commit(ctx);
            ctx->term.parser_state = (b == '\x1b') ? VT_ESCAPE : VT_GROUND;
        } else
            _sfte_sixel_parse_byte(ctx, b);
        break;
#endif  // SFTE_IMG_SIXEL
    case VT_CSI_ENTRY:
    case VT_CSI_PARAM:
        if (b == '?') {  // private marker
            ctx->term.parser_state = VT_CSI_PARAM;
            ctx->term.vt_dec_priv = 1;
        } else if (b == '>') {
            ctx->term.parser_state = VT_CSI_PARAM;
            ctx->term.vt_dec_priv = 2;
        } else if (b >= '0' && b <= '9') {
            ctx->term.parser_state = VT_CSI_PARAM;
            ctx->term.vt_params[ctx->term.vt_param_idx] *= 10;
            ctx->term.vt_params[ctx->term.vt_param_idx] += (b - '0');
        } else if (b == ';' || b == ':') {
            if (ctx->term.vt_param_idx < 15) ctx->term.vt_param_idx++;  // Move to next parameter
        } else if (b >= 0x40 && b <= 0x7E) {
            _sfte_csi_dispatch(ctx, b);
            ctx->term.parser_state = VT_GROUND;
        }
    }
}
// =================================================================================================
// >>shaper
// =================================================================================================
#if SFTE_FONT_LIGATURES
#define _SFTE_MATCH_TAG(t1, t2)                                                                    \
    (t1)[0] == (t2)[0] && (t1)[1] == (t2)[1] && (t1)[2] == (t2)[2] && (t1)[3] == (t2)[3]

/*
    Grabs a 16-bit big-endian value from `offset` and places the `offset` after grabbed value.
*/
static inline uint16_t _sfte_shaper_consume16(const uint8_t *ttf_data, uint32_t *off) {
    uint16_t val = _SFTE_R16BE(ttf_data, *off);
    *off += sizeof(uint16_t);
    return val;
}

/*
    Grabs a 32-bit big-endian value from `offset` and places the `offset` after grabbed value.
*/
static inline uint32_t _sfte_shaper_consume32(const uint8_t *ttf_data, uint32_t *off) {
    uint32_t val = _SFTE_R32BE(ttf_data, *off);
    *off += sizeof(uint32_t);
    return val;
}

/*
    Returns the byte offset of a specific table in TTF font data.
    Returns 0 if not found.
*/
static inline uint32_t _sfte_shaper_get_font_table_off(const uint8_t *ttf_data, const char tag[4]) {
    // 0-3 - sfnt version
    // 4-7 - number of tables
    // 12+ - tables
    uint32_t off = sizeof(uint32_t);
    uint32_t num_tables = _sfte_shaper_consume16(ttf_data, &off);
    off += 3 * sizeof(uint16_t);

    for (uint32_t i = 0; i < num_tables; ++i, off += 4 * sizeof(uint8_t) + 3 * sizeof(uint32_t))
        if (_SFTE_MATCH_TAG(&ttf_data[off], tag))
            return _SFTE_R32BE(ttf_data, off + 2 * sizeof(uint32_t));

    return 0;
}

/*
    Locates a specific GSUB feature (like 'calt' or 'liga') and extracts its lookup indices.
*/
static inline void _sfte_shaper_load_feature(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                             const char tag[4], sfte_shaper_feature *out_feat) {
    out_feat->lookup_cnt = 0;
    if (ctx->feat_list_off == 0) return;

    // FeatureList header data:
    // 0-1 - FeatureCount
    // 2+  - FeatureRecords
    uint32_t fl_off = ctx->feat_list_off;
    uint16_t feat_cnt = _sfte_shaper_consume16(ttf_data, &fl_off);

    for (uint32_t i = 0, rec_off = fl_off; i < feat_cnt;
         ++i, rec_off += 4 * sizeof(uint8_t) + sizeof(uint16_t))

        if (_SFTE_MATCH_TAG(&ttf_data[rec_off], tag)) {
            rec_off += 4 * sizeof(uint8_t);
            uint32_t feat_tab_off = ctx->feat_list_off + _sfte_shaper_consume16(ttf_data, &rec_off);

            // FeatureTable header data:
            // 0-1 - FeatureParams offset
            // 2-3 - LookupIndexCount
            // 4+  - LookupListIndices
            feat_tab_off += sizeof(uint16_t);  // Skip FeatureParams
            uint16_t lookup_cnt = _sfte_shaper_consume16(ttf_data, &feat_tab_off);

            for (uint16_t j = 0; j < lookup_cnt && j < SFTE_FONT_MAX_LIGATURE_LOOKUPS; ++j)
                out_feat->lookup_indices[j] = _SFTE_R16BE(ttf_data,
                                                          feat_tab_off + (j * sizeof(uint16_t)));

            out_feat->lookup_cnt = lookup_cnt > SFTE_FONT_MAX_LIGATURE_LOOKUPS
                                       ? SFTE_FONT_MAX_LIGATURE_LOOKUPS
                                       : lookup_cnt;
            return;
        }
}

/*
    Parses inner subtables and extension lookups (type 8 -> type 6).
*/
static inline void _sfte_shaper_parse_subtables(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint16_t lookup_idx, uint32_t lookup_tab_off,
                                                uint16_t lookup_type, uint16_t subtable_cnt) {
    ctx->calt.subtable_cnts[lookup_idx] = subtable_cnt > SFTE_FONT_MAX_LIGATURE_LOOKUPS
                                              ? SFTE_FONT_MAX_LIGATURE_LOOKUPS
                                              : subtable_cnt;

    for (uint16_t s = 0; s < ctx->calt.subtable_cnts[lookup_idx]; ++s) {
        uint16_t st_off_rel = _SFTE_R16BE(ttf_data, lookup_tab_off + (s * sizeof(uint16_t)));
        uint32_t st_off_base = lookup_tab_off - 3 * sizeof(uint16_t) + st_off_rel;
        uint32_t st_off = st_off_base;

        uint16_t type = lookup_type;
        if (lookup_type == 8) {  // Extension lookup
            uint16_t ext_format = _sfte_shaper_consume16(ttf_data, &st_off);
            if (ext_format == 1) {
                type = _sfte_shaper_consume16(ttf_data, &st_off);
                uint32_t ext_off_rel = _sfte_shaper_consume32(ttf_data, &st_off);
                st_off = st_off_base + ext_off_rel;
            }
        }

        ctx->calt.lookup_types[lookup_idx] = type;
        ctx->calt.subtable_offs[lookup_idx][s] = st_off;
    }
}

/*
    Parses the GSUB LookupList and stores offsets for contextual rules.
*/
static inline void _sfte_shaper_parse_lookups(sfte_shaper_ctx *ctx, const uint8_t *ttf_data) {
    if (ctx->lookup_list_off == 0) return;

    // LookupList header data:
    // 0-1 - LookupCount
    // 2+  - offsets to lookup tables
    uint32_t ll_off = ctx->lookup_list_off;
    uint16_t l_cnt = _sfte_shaper_consume16(ttf_data, &ll_off);

    for (uint16_t i = 0; i < ctx->calt.lookup_cnt; ++i) {
        uint16_t idx = ctx->calt.lookup_indices[i];
        if (idx >= l_cnt) continue;

        uint32_t lt_off = ctx->lookup_list_off +
                          _SFTE_R16BE(ttf_data, ll_off + (idx * sizeof(uint16_t)));

        // LookupTable header data:
        // 0-1 - LookupType
        // 2-3 - LookupFlag
        // 4-5 - SubTableCount
        uint16_t l_type = _sfte_shaper_consume16(ttf_data, &lt_off);
        lt_off += sizeof(uint16_t);  // Skip LookupFlag
        uint16_t st_cnt = _sfte_shaper_consume16(ttf_data, &lt_off);

        if (st_cnt > 0)
            _sfte_shaper_parse_subtables(ctx, ttf_data, i, lt_off, l_type, st_cnt);
        else {
            ctx->calt.lookup_types[i] = 0;
            for (uint16_t s = 0; s < SFTE_FONT_MAX_LIGATURE_SUBTABLES; ++s)
                ctx->calt.subtable_offs[i][s] = 0;
        }
    }
}

/*
    Initializes the OpenType GSUB shaper state machine from raw TTF data.
*/
static inline void _sfte_shaper_init(sfte_shaper_ctx *ctx, const uint8_t *ttf_data) {
    ctx->table_off = _sfte_shaper_get_font_table_off(ttf_data, "GSUB");
    if (ctx->table_off == 0) return;

    uint32_t t_off = ctx->table_off;

    // GSUB 1.0 header data:
    // 0-1 - major version
    // 2-3 - minor version
    // 4-5 - ScriptList offset
    // 6-7 - FeatureList offset
    // 8-9 - LookupList offset

    uint16_t major_ver = _sfte_shaper_consume16(ttf_data, &t_off);
    t_off += sizeof(uint16_t);  // Skip minor
    if (major_ver != 1) return;

    ctx->script_list_off = ctx->table_off + _sfte_shaper_consume16(ttf_data, &t_off);
    ctx->feat_list_off = ctx->table_off + _sfte_shaper_consume16(ttf_data, &t_off);
    ctx->lookup_list_off = ctx->table_off + _sfte_shaper_consume16(ttf_data, &t_off);

    _sfte_shaper_load_feature(ctx, ttf_data, "calt", &ctx->calt);
    _sfte_shaper_load_feature(ctx, ttf_data, "liga", &ctx->liga);

    _sfte_shaper_parse_lookups(ctx, ttf_data);
}

/*
    Resolves a glyphs index within a Coverage table (format 1 or 2).
    Returns -1 if the glyph is not covered by the rule.
*/
static inline int32_t _sfte_shaper_get_coverage_index(const uint8_t *ttf_data, uint32_t cov_off,
                                                      uint16_t glyph_id) {
    if (cov_off == 0) return -1;

    uint16_t format = _sfte_shaper_consume16(ttf_data, &cov_off);

    if (format == 1) {  // Array of individual glyph IDs
        uint16_t g_cnt = _sfte_shaper_consume16(ttf_data, &cov_off);
        for (uint16_t i = 0; i < g_cnt; ++i)
            if (_SFTE_R16BE(ttf_data, cov_off + (i * sizeof(uint16_t))) == glyph_id) return i;

    } else if (format == 2) {  // Array of glyph ID ranges
        uint16_t gr_cnt = _sfte_shaper_consume16(ttf_data, &cov_off);
        for (uint32_t i = 0, r_off = cov_off; i < gr_cnt; ++i, r_off += 3 * sizeof(uint16_t))
            if (glyph_id >= _SFTE_R16BE(ttf_data, r_off) &&
                glyph_id <= _SFTE_R16BE(ttf_data, r_off + sizeof(uint16_t)))
                return _SFTE_R16BE(ttf_data, r_off + 2 * sizeof(uint16_t)) +
                       (glyph_id - _SFTE_R16BE(ttf_data, r_off));
    }

    return -1;
}

/*
    Resolves a glyphs class within a ClassDef table.
*/
static inline uint16_t _sfte_shaper_get_class(const uint8_t *ttf_data, uint32_t class_off,
                                              uint16_t glyph_id) {
    if (class_off == 0) return 0;

    uint16_t format = _sfte_shaper_consume16(ttf_data, &class_off);

    if (format == 1) {  // Array of classses for a contiguous range of glyph IDs
        uint16_t c_start = _sfte_shaper_consume16(ttf_data, &class_off);
        uint16_t c_cnt = _sfte_shaper_consume16(ttf_data, &class_off);

        if (glyph_id >= c_start && glyph_id < c_start + c_cnt)
            return _SFTE_R16BE(ttf_data, class_off + ((glyph_id - c_start) * sizeof(uint16_t)));

    } else if (format == 2) {  // Array of glyph ID ranges mapping to specific classes
        uint16_t g_cnt = _sfte_shaper_consume16(ttf_data, &class_off);

        for (uint16_t i = 0; i < g_cnt; ++i, class_off += 3 * sizeof(uint16_t))
            if (glyph_id >= _SFTE_R16BE(ttf_data, class_off) &&
                glyph_id <= _SFTE_R16BE(ttf_data, class_off + sizeof(uint16_t)))
                return _SFTE_R16BE(ttf_data, class_off + 2 * sizeof(uint16_t));
    }

    return 0;
}

/*
    Evaluates a single substitution rule (LookupType=1).
*/
static inline uint16_t _sfte_shaper_get_type1_subst(const uint8_t *ttf_data, uint32_t t1_off,
                                                    uint16_t glyph_id) {
    uint32_t t_off = t1_off;
    uint16_t format = _sfte_shaper_consume16(ttf_data, &t_off);
    if (format != 1 && format != 2) return glyph_id;

    uint32_t c_off = t1_off + _sfte_shaper_consume16(ttf_data, &t_off);

    int32_t c_idx = _sfte_shaper_get_coverage_index(ttf_data, c_off, glyph_id);
    if (c_idx < 0) return glyph_id;

    if (format == 1)
        return (uint16_t)(glyph_id + _SFTE_R16BE(ttf_data, t_off));
    else if (c_idx < _sfte_shaper_consume16(ttf_data, &t_off))
        return _SFTE_R16BE(ttf_data, t_off + (c_idx * sizeof(uint16_t)));

    return glyph_id;
}

/*
    Evaluates a specific rule sequence.
    Returns 1 if sequence matches, fills out `out_records` and `out_cnt`.
    Returns 0 if sequence doesn't match.
*/
static inline uint8_t _sfte_shaper_match_rule_format1(const uint8_t *ttf_data, uint32_t rule_off,
                                                      uint16_t *grid, size_t grid_len, size_t pos,
                                                      sfte_shaper_subst_record *out_records,
                                                      uint16_t *out_cnt) {
    // Backtrack, stored in reverse visual order
    uint16_t b_cnt = _sfte_shaper_consume16(ttf_data, &rule_off);
    if (pos < b_cnt) return 0;  // Not enough cells behind to match
    for (uint16_t i = 0; i < b_cnt; ++i)
        if (grid[pos - (i + 1)] != _sfte_shaper_consume16(ttf_data, &rule_off)) return 0;

    // Input, 0 is implicit via Coverage, check 1 to input_cnt-1
    uint16_t i_cnt = _sfte_shaper_consume16(ttf_data, &rule_off);
    uint16_t ia_len = (i_cnt > 0) ? i_cnt - 1 : 0;
    if (pos + i_cnt > grid_len) return 0;  // Not enough cells ahead to match
    for (uint16_t i = 0; i < ia_len; ++i)
        if (grid[pos + (i + 1)] != _sfte_shaper_consume16(ttf_data, &rule_off)) return 0;

    // Lookahead, starts immediately after the input sequence
    uint16_t l_cnt = _sfte_shaper_consume16(ttf_data, &rule_off);
    if (pos + i_cnt + l_cnt > grid_len) return 0;  // Not enough cells for lookahead
    for (uint16_t i = 0; i < l_cnt; ++i)
        if (grid[pos + (i + i_cnt)] != _sfte_shaper_consume16(ttf_data, &rule_off)) return 0;

    // Matched, extract substitution records
    *out_cnt = _sfte_shaper_consume16(ttf_data, &rule_off);
    for (uint16_t i = 0; i < *out_cnt && i < SFTE_FONT_MAX_LIGATURE_RECORDS; ++i) {
        out_records[i].sequence_idx = _sfte_shaper_consume16(ttf_data, &rule_off);
        out_records[i].lookup_idx = _sfte_shaper_consume16(ttf_data, &rule_off);
    }

    return 1;
}

/*
    Applies substitution records to the temporary glyph ID buffer.
*/
static inline void _sfte_shaper_apply_subst(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                            uint16_t *grid, size_t pos,
                                            const sfte_shaper_subst_record *records,
                                            uint16_t records_cnt) {
    for (uint16_t i = 0; i < records_cnt; ++i) {
        uint16_t t_pos = pos + records[i].sequence_idx;
        uint32_t tl_off = ctx->lookup_list_off +
                          _SFTE_R16BE(ttf_data, ctx->lookup_list_off + sizeof(uint16_t) +
                                                    (records[i].lookup_idx * sizeof(uint16_t)));

        uint16_t ts_rel_off = _SFTE_R16BE(ttf_data, tl_off + 3 * sizeof(uint16_t));
        uint32_t ts_off = tl_off + ts_rel_off;

        grid[t_pos] = _sfte_shaper_get_type1_subst(ttf_data, ts_off, grid[t_pos]);
    }
}

/*
    Evaluates a format 1 contextual lookup (Rule-based).
    Returns 1 on success.
    Returns 0 if failed to match a rule.
*/
static inline uint8_t _sfte_shaper_eval_format1(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint32_t sub_off, uint16_t cur_glyph,
                                                uint16_t *grid, size_t grid_len, size_t pos) {
    uint32_t s_off = sub_off + sizeof(uint16_t);  // Skip format

    uint32_t c_off = sub_off + _sfte_shaper_consume16(ttf_data, &s_off);
    int32_t c_idx = _sfte_shaper_get_coverage_index(ttf_data, c_off, cur_glyph);
    if (c_idx < 0) return 0;

    uint16_t rs_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    if (c_idx >= rs_cnt) return 0;

    uint32_t rs_off = sub_off + 3 * sizeof(uint16_t) + (c_idx * sizeof(uint16_t));
    uint16_t rs_rel_off = _SFTE_R16BE(ttf_data, rs_off);
    if (!rs_rel_off) return 0;

    uint32_t rs_base_off = sub_off + rs_rel_off;
    rs_off = rs_base_off;
    uint16_t r_cnt = _sfte_shaper_consume16(ttf_data, &rs_off);

    for (uint16_t r = 0; r < r_cnt; ++r) {
        uint32_t r_off = rs_base_off + _sfte_shaper_consume16(ttf_data, &rs_off);

        sfte_shaper_subst_record records[SFTE_FONT_MAX_LIGATURE_RECORDS];
        uint16_t rec_cnt = 0;

        if (_sfte_shaper_match_rule_format1(ttf_data, r_off, grid, grid_len, pos, records,
                                            &rec_cnt)) {
            _sfte_shaper_apply_subst(ctx, ttf_data, grid, pos, records, rec_cnt);
            return 1;
        }
    }

    return 0;
}

/*
    Evaluates a format 3 contextual lookup (Coverage-based).
    Returns 1 on success.
    Returns 0 if backtrack/input/lookahead failed.
*/
static inline uint8_t _sfte_shaper_eval_format3(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                                uint32_t sub_off, uint16_t *grid, size_t grid_len,
                                                size_t pos) {
    uint32_t s_off = sub_off + sizeof(uint16_t);  // Skip format

    // Backtrack, stored in reverse visual order
    uint16_t b_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    if (pos < b_cnt) return 0;
    for (uint16_t i = 0; i < b_cnt; ++i)
        if (_sfte_shaper_get_coverage_index(ttf_data,
                                            sub_off + _sfte_shaper_consume16(ttf_data, &s_off),
                                            grid[pos - (i + 1)]) < 0)
            return 0;

    // Input, includes the current glyph at index 0
    uint16_t i_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    if (pos + i_cnt > grid_len) return 0;
    for (uint16_t i = 0; i < i_cnt; ++i)
        if (_sfte_shaper_get_coverage_index(
                ttf_data, sub_off + _sfte_shaper_consume16(ttf_data, &s_off), grid[pos + i]) < 0)
            return 0;

    // Lookahead, starts immediately after the input sequence
    uint16_t l_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    if (pos + i_cnt + l_cnt > grid_len) return 0;
    for (uint16_t i = 0; i < l_cnt; ++i)
        if (_sfte_shaper_get_coverage_index(ttf_data,
                                            sub_off + _sfte_shaper_consume16(ttf_data, &s_off),
                                            grid[pos + i_cnt + i]) < 0)
            return 0;

    // Matched, extract and apply substitutions
    uint16_t rec_cnt = _sfte_shaper_consume16(ttf_data, &s_off);
    sfte_shaper_subst_record records[SFTE_FONT_MAX_LIGATURE_RECORDS];

    for (uint16_t i = 0; i < rec_cnt && i < SFTE_FONT_MAX_LIGATURE_RECORDS; ++i) {
        records[i].sequence_idx = _sfte_shaper_consume16(ttf_data, &s_off);
        records[i].lookup_idx = _sfte_shaper_consume16(ttf_data, &s_off);
    }

    _sfte_shaper_apply_subst(ctx, ttf_data, grid, pos, records, rec_cnt);

    return 1;
}

/*
    Shapes a continuous block of uniform text against all lookups.
    OpenType dictates a cascading loop.
    A lookup must evaluate the entire string before yielding to the next lookup.
    A simple loop would fail on something like '==='.
*/
static inline void _sfte_shaper_shape_row(sfte_shaper_ctx *ctx, const uint8_t *ttf_data,
                                          uint16_t *grid, size_t grid_len) {
    for (uint16_t i = 0; i < ctx->calt.lookup_cnt; ++i) {
        if (ctx->calt.lookup_types[i] != 6) continue;

        for (size_t pos = 0; pos < grid_len; ++pos) {
            uint16_t g = grid[pos];
            if (!g) continue;

            for (uint16_t s = 0; s < ctx->calt.subtable_cnts[i]; ++s) {
                uint32_t s_off = ctx->calt.subtable_offs[i][s];
                if (!s_off) continue;

                uint16_t format = _SFTE_R16BE(ttf_data, s_off);
                uint8_t matched = 0;

                if (format == 1)
                    matched = _sfte_shaper_eval_format1(ctx, ttf_data, s_off, g, grid, grid_len,
                                                        pos);
                else if (format == 3)
                    matched = _sfte_shaper_eval_format3(ctx, ttf_data, s_off, grid, grid_len, pos);

                if (matched) break;
            }
        }
    }
}

#undef _SFTE_MATCH_TAG
#endif  // SFTE_FONT_LIGATURES
// =================================================================================================
// >>font
// =================================================================================================
#ifndef SFTE_FONT_CUSTOM_BACKEND
/*
    Default stb_truetype wrappers.
    Can be overriden by defining SFTE_FONT_CUSTOM_BACKEND.
*/
static inline void _sfte_stb_init(sfte_font_backend_info *info, const uint8_t *data) {
    stbtt_InitFont(info, data, 0);
}

static inline float _sfte_stb_get_scale(sfte_font_backend_info *info, float px_hei) {
    return stbtt_ScaleForPixelHeight(info, px_hei);
}

static inline void _sfte_stb_vmetrics(sfte_font_backend_info *info, int *ascent, int *descent,
                                      int *linegap) {
    stbtt_GetFontVMetrics(info, ascent, descent, linegap);
}

static inline void _sfte_stb_bounds(sfte_font_backend_info *info, int glyph_id, float scale,
                                    int *adv, int *x0, int *y0, int *x1, int *y1) {
    int lsb;
    stbtt_GetGlyphHMetrics(info, glyph_id, adv, &lsb);
    stbtt_GetGlyphBitmapBox(info, glyph_id, scale, scale, x0, y0, x1, y1);
}

static inline void _sfte_stb_bake(sfte_ctx *ctx, sfte_font_backend_info *info, int glyph_idx,
                                  float scale, uint8_t *atlas_ptr, int gw, int gh,
                                  int atlas_stride) {
#if SFTE_FONT_OVERSAMPLE <= 1
    (void)ctx;
    stbtt_MakeGlyphBitmap(info, atlas_ptr, gw, gh, atlas_stride, scale, scale, glyph_idx);
#else   // SFTE_FONT_OVERSAMPLE > 1
    int bw = gw * SFTE_FONT_OVERSAMPLE;
    int bh = gh;

    size_t pre_off = _sfte_mem_stack_save(&ctx->stack);
    uint8_t *temp_buf = (uint8_t *)_sfte_mem_stack_alloc(&ctx->stack, bw * bh * sizeof(uint8_t),
                                                         _Alignof(uint8_t));

    stbtt_MakeGlyphBitmap(info, temp_buf, bw, bh, bw, scale * SFTE_FONT_OVERSAMPLE, scale,
                          glyph_idx);

    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x) {
            int32_t sum = 0;
            for (int ox = 0; ox < SFTE_FONT_OVERSAMPLE; ++ox)
                sum += temp_buf[y * bw + (x * SFTE_FONT_OVERSAMPLE + ox)];
            atlas_ptr[y * atlas_stride + x] = (uint8_t)(sum / SFTE_FONT_OVERSAMPLE);
        }

    _sfte_mem_stack_rewind(&ctx->stack, pre_off);
#endif  // SFTE_FONT_OVERSAMPLE > 1
}

static inline int _sfte_stb_get_id(sfte_font_backend_info *info, uint32_t rune) {
    return stbtt_FindGlyphIndex(info, rune);
}
#endif  // !SFTE_FONT_CUSTOM_BACKEND

/*
    Distance in pixels between each glyph in the atlas 2D texture.
*/
#define _SFTE_FONT_PADDING 1

/*
    Retrieves the active cache pool for a specific font style (regular/bold/italic/bold italic).
*/
static inline sfte_font_cache *_sfte_font_get_cache(sfte_ctx *ctx, sfte_font_style style) {
    if (style == SFTE_FONT_STYLE_REGULAR) return &ctx->font.regular;
#ifdef SFTE_FONT_BOLD
    if (style == SFTE_FONT_STYLE_BOLD) return &ctx->font.bold;
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    if (style == SFTE_FONT_STYLE_ITALIC) return &ctx->font.italic;
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    if (style == SFTE_FONT_STYLE_BOLD_ITALIC) return &ctx->font.bold_italic;
#endif  // SFTE_FONT_BOLD_ITALIC
    return NULL;
}

/*
    Clears a font cache's atlas texture and glyph hash map.
*/
static inline void _sfte_font_clear_cache(sfte_font_cache *cache) {
    if (cache->atlas_pxs) memset(cache->atlas_pxs, 0, SFTE_FONT_ATLAS_SIZE * SFTE_FONT_ATLAS_SIZE);
    if (cache->glyphs) memset(cache->glyphs, 0, SFTE_FONT_GLYPH_CAP * sizeof(sfte_glyph));
    cache->atlas_x = 0;
    cache->atlas_y = 0;
    cache->atlas_row_h = 0;
}

/*
    Recalculates font scales for the primary font and all fallbacks based on current size.
*/
static inline void _sfte_font_update_scales(sfte_ctx *ctx, sfte_font_cache *cache) {
    for (uint8_t i = 0; i < cache->num_fonts; ++i) {
        float tweak = _sfte_font_scales[i];
        if (tweak <= 0.0f) tweak = 1.0f;
        cache->scales[i] = SFTE_FONT_GET_SCALE(&cache->info[i], ctx->font.cur_size * tweak);
    }
}

/*
    Allocates space in the 2D texture atlas for a new glyph and bakes the pixels.
    Glyphs are packed sequentially into rows. If a row runs out of horizontal space,
    we step down by the height of the tallest glyph in that row.
*/
static inline void _sfte_font_pack_and_bake(sfte_ctx *ctx, sfte_font_cache *cache, sfte_glyph *g,
                                            int32_t font_idx, int32_t glyph_id, int32_t gw,
                                            int32_t gh) {
    if (cache->atlas_x + gw >= SFTE_FONT_ATLAS_SIZE) {
        cache->atlas_x = 0;
        cache->atlas_y += cache->atlas_row_h + _SFTE_FONT_PADDING;
        cache->atlas_row_h = 0;
    }

    SFTE_ASSERT(cache->atlas_y + gh < SFTE_FONT_ATLAS_SIZE, "glyph atlas full");

    if (gh > cache->atlas_row_h) cache->atlas_row_h = gh;

    g->x0 = cache->atlas_x;
    g->y0 = cache->atlas_y;
    g->x1 = g->x0 + gw;
    g->y1 = g->y0 + gh;

    if (gw > 0 && gh > 0) {
        int32_t atlas_idx = g->y0 * SFTE_FONT_ATLAS_SIZE + g->x0;
        SFTE_FONT_BAKE(ctx, &cache->info[font_idx], glyph_id, cache->scales[font_idx],
                       &cache->atlas_pxs[atlas_idx], gw, gh, SFTE_FONT_ATLAS_SIZE);
    }

    cache->atlas_x += gw + _SFTE_FONT_PADDING;
}

/*
    Retrieves a cached glyph raster, or bakes a new one on cache miss.
    This function is blind to unicode, it expects a pre-resolved TrueType ID and font index from
   `_sfte_font_resolve_rune`.
*/
static inline sfte_glyph *_sfte_font_get_glyph(sfte_ctx *ctx, sfte_font_cache *cache,
                                               uint16_t glyph_id, uint8_t font_idx) {
    if (glyph_id == 0) return NULL;

    uint32_t h = (glyph_id ^ (font_idx << 16)) % SFTE_FONT_GLYPH_CAP;

    // hash map logic
    for (uint16_t i = 0; i < SFTE_FONT_GLYPH_CAP; ++i) {
        uint16_t idx = (h + i) % SFTE_FONT_GLYPH_CAP;

        if (cache->glyphs[idx].glyph_id == glyph_id && cache->glyphs[idx].font_idx == font_idx)
            return &cache->glyphs[idx];                  // Cache hit
        if (cache->glyphs[idx].glyph_id != 0) continue;  // Collision

        // Cache miss, look up bounds across primary and fallback fonts
        int32_t adv = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        SFTE_FONT_BOUNDS(&cache->info[font_idx], glyph_id, cache->scales[font_idx], &adv, &x0, &y0,
                         &x1, &y1);

        sfte_glyph *g = &cache->glyphs[idx];
        g->glyph_id = glyph_id;
        g->xadvance = (int)(adv * cache->scales[font_idx] + 0.5f);
        g->xoff = x0;
        g->yoff = y0;
        g->font_idx = font_idx;

        _sfte_font_pack_and_bake(ctx, cache, g, font_idx, glyph_id, x1 - x0, y1 - y0);

        return g;
    }

    return NULL;
}

/*
    Resolves a unicode rune to a specific font index and TrueType glyph ID.
    Handles cascading fallback fonts.
    Sets both values to 0 if glyph is not found.
*/
static inline void _sfte_font_resolve_rune(sfte_font_cache *cache, sfte_rune rune,
                                           uint8_t *out_font_idx, uint16_t *out_glyph_id) {
    if (rune == 0) rune = ' ';

    for (uint8_t f = 0; f < cache->num_fonts; ++f) {
        uint16_t id = (uint16_t)SFTE_FONT_GET_ID(&cache->info[f], rune);
        if (id != 0) {
            *out_font_idx = f;
            *out_glyph_id = id;
            return;
        }
    }

    *out_font_idx = 0;
    *out_glyph_id = 0;
}

/*
    Purges all glyph atlases and recalculates strict terminal grid metrics.
    Must be called on startup, and whenever the DPI or font size changes.
*/
static inline void _sfte_font_reset_cache(sfte_ctx *ctx) {
    _sfte_font_clear_cache(&ctx->font.regular);
    _sfte_font_update_scales(ctx, &ctx->font.regular);

#ifdef SFTE_FONT_BOLD
    _sfte_font_clear_cache(&ctx->font.bold);
    _sfte_font_update_scales(ctx, &ctx->font.bold);
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    _sfte_font_clear_cache(&ctx->font.italic);
    _sfte_font_update_scales(ctx, &ctx->font.italic);
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    _sfte_font_clear_cache(&ctx->font.bold_italic);
    _sfte_font_update_scales(ctx, &ctx->font.bold_italic);
#endif  // SFTE_FONT_BOLD_ITALIC

    int32_t ascent_u, descent_u, line_gap_u;
    SFTE_FONT_VMETRICS(&ctx->font.regular.info[0], &ascent_u, &descent_u, &line_gap_u);

    float primary_scale = ctx->font.regular.scales[0];
    ctx->font.ascent = (int)(ascent_u * primary_scale + 0.5f);
    ctx->font.descent = (int)(descent_u * primary_scale -
                              0.5f /* - instead of + because descent is natively negative */);
    ctx->font.line_gap = (int)(line_gap_u * primary_scale + 0.5f);
    ctx->font.cell_height = ctx->font.ascent - ctx->font.descent + ctx->font.line_gap;

    // NOTE:
    // Terminal column width is locked to advance of 'M'.
    uint8_t m_font_idx = 0;
    uint16_t m_glyph_id = 0;
    _sfte_font_resolve_rune(&ctx->font.regular, 'M', &m_font_idx, &m_glyph_id);
    sfte_glyph *m = _sfte_font_get_glyph(ctx, &ctx->font.regular, m_glyph_id, m_font_idx);
    ctx->font.cell_width = m->xadvance;
}
// =================================================================================================
// >>render
// =================================================================================================
#if SFTE_CURSOR_DYNAMIC
#define _SFTE_CUR_STYLE(ctx) (ctx->term.cursor_style)
#else
#define _SFTE_CUR_STYLE(ctx) (SFTE_CURSOR_STYLE)
#endif  // !SFTE_CURSOR_DYNAMIC

/*
    Safely expands a bounding box to encompass a new dirty region.
*/
static inline void _sfte_render_damage_add(sfte_damage_rect *dmg, int32_t x, int32_t y, int32_t w,
                                           int32_t h) {
    if (w <= 0 || h <= 0) return;

    if (dmg->w <= 0 || dmg->h <= 0) {
        dmg->x = x, dmg->y = y;
        dmg->w = w, dmg->h = h;
        return;
    }

    dmg->x = (dmg->x < x) ? dmg->x : x;
    dmg->y = (dmg->y < y) ? dmg->y : y;

    int32_t max_r = (dmg->x + dmg->w > x + w) ? dmg->x + dmg->w : x + w;
    int32_t max_b = (dmg->y + dmg->h > y + h) ? dmg->y + dmg->h : y + h;

    dmg->w = max_r - dmg->x;
    dmg->h = max_b - dmg->y;
}

/*
    Evaluates cursor movement and font bleed.

    Fonts often spill slightly outside their strict grid cell bounds.
    If we only redraw the specific cell that changed, we slice off the edges of adjacent
   letters. This pass detects damaged cells and intentionally bleeds the dirty flag to adjacent
   rows and columns to guarantee seamless redrawing.
*/
static inline void _sfte_render_propagate_damage(sfte_ctx *ctx, int16_t vis_col, int16_t vis_row) {
    int32_t logical_r_curr = _sfte_grid_vis2log(ctx, vis_row);
    int32_t logical_r_last = _sfte_grid_vis2log(ctx, ctx->term.last_drawn_row);

    if (ctx->term.last_drawn_col != vis_col || ctx->term.last_drawn_row != vis_row) {
        if (ctx->term.last_drawn_col >= 0 && ctx->term.last_drawn_col < ctx->term.cols &&
            ctx->term.last_drawn_row >= 0 && ctx->term.last_drawn_row < ctx->term.rows)
            _sfte_grid_get_cell(ctx, ctx->term.last_drawn_col, logical_r_last)->dirty = 1;

        if (vis_col >= 0 && vis_col < ctx->term.cols && vis_row >= 0 && vis_row < ctx->term.rows)
            _sfte_grid_get_cell(ctx, vis_col, logical_r_curr)->dirty = 1;

        ctx->term.last_drawn_col = vis_col;
        ctx->term.last_drawn_row = vis_row;
    }

#if SFTE_FONT_BLEED
    for (int16_t r = 0; r < ctx->term.rows; ++r) {
        int32_t logical_r = _sfte_grid_vis2log(ctx, r);
        uint8_t row_has_damage = 0;
        for (int16_t c = 0; c < ctx->term.cols; ++c)
            if (_sfte_grid_get_cell(ctx, c, logical_r)->dirty) {
                row_has_damage = 1;
                break;
            }

        if (row_has_damage) {
            int16_t logical_r_min = r > 0 ? logical_r - 1 : logical_r;
            int16_t logical_r_max = r < ctx->term.rows - 1 ? logical_r + 1 : logical_r;

            // NOTE:
            // Mark padding as dirty to clear the bleed area.
            // it's not hidden behind an `if (r_min == 0 || r_max == ctx->term.rows - 1)`,
            // because if damage is at left or right edge, the bleed area will be visible there
            // too.
            ctx->padding_dirty = 1;

            for (int32_t lr = logical_r_min; lr <= logical_r_max; ++lr)
                for (int16_t c = 0; c < ctx->term.cols; ++c) {
                    sfte_cell *cell = _sfte_grid_get_cell(ctx, c, lr);
                    if (!cell->dirty) cell->dirty = 2;  // bleed-dirty
                }
        }
    }

    // Normalize bleed-dirty flags back to standard dirty flags
    for (int16_t r = 0; r < ctx->term.rows; ++r) {
        int32_t logical_r = _sfte_grid_vis2log(ctx, r);
        for (int16_t c = 0; c < ctx->term.cols; ++c) {
            sfte_cell *cell = _sfte_grid_get_cell(ctx, c, logical_r);
            if (cell->dirty == 2) cell->dirty = 1;
        }
    }
#endif  // SFTE_FONT_BLEED
}

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
/*
    Sorts active image placements by z index using a fast insertion sort.
*/
static inline void _sfte_render_sort_images(sfte_ctx *ctx) {
    for (uint32_t i = 1; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement key = ctx->term.img_placements[i];
        int j = i - 1;
        while (j >= 0 && ctx->term.img_placements[j].z_idx > key.z_idx) {
            ctx->term.img_placements[j + 1] = ctx->term.img_placements[j];
            j--;
        }
        ctx->term.img_placements[j + 1] = key;
    }
}
/*
    Compositor pass for images.
    Images can be drawn behind text (is_bg_pass = 1) or in front of text (is_bg_pass = 0).
*/
static inline void _sfte_render_images(sfte_ctx *ctx, void *px_buf, uint8_t is_bg_pass,
                                       int32_t base_y_off, uint8_t pad_was_dirty) {
    for (uint32_t i = 0; i < ctx->term.img_placements_len; ++i) {
        sfte_img_placement *p = &ctx->term.img_placements[i];
#if SFTE_TERM_ALT_SCREEN
        if (p->alt_screen != ctx->term.alt_active) continue;
#endif  // SFTE_TERM_ALT_SCREEN

        uint8_t is_bg_img = (p->z_idx < 0);
        if (is_bg_img != is_bg_pass) continue;

        sfte_img *img = _sfte_img_find(ctx, p->img_id);
        if (!img) continue;

        int32_t base_x = (p->start_col * ctx->font.cell_width) + SFTE_WINDOW_PAD_X + p->x_off;
        int32_t base_y = (p->start_row * ctx->font.cell_height) + SFTE_WINDOW_PAD_Y + p->y_off +
                         base_y_off;

        for (int32_t iy = 0; iy < img->height; ++iy) {
            int32_t out_y = base_y + iy;
            if (out_y < 0 || out_y >= ctx->height) continue;

            for (int32_t ix = 0; ix < img->width; ++ix) {
                int32_t out_x = base_x + ix;
                if (out_x < 0 || out_x >= ctx->width) continue;

                uint8_t is_dirty = 0;
                if (out_x < SFTE_WINDOW_PAD_X || out_y < SFTE_WINDOW_PAD_Y ||
                    out_x >= ctx->width - SFTE_WINDOW_PAD_X ||
                    out_y >= ctx->height - SFTE_WINDOW_PAD_Y) {
                    is_dirty = pad_was_dirty;
                } else {
                    int16_t grid_c = (out_x - SFTE_WINDOW_PAD_X) / ctx->font.cell_width;
                    int16_t grid_r = (out_y - SFTE_WINDOW_PAD_Y) / ctx->font.cell_height;

                    grid_c = _SFTE_CLAMP(grid_c, 0, ctx->term.cols - 1);
                    grid_r = _SFTE_CLAMP(grid_r, 0, ctx->term.rows - 1);

                    int32_t logical_r = _sfte_grid_vis2log(ctx, grid_r);
                    is_dirty = _sfte_grid_get_cell(ctx, grid_c, logical_r)->dirty;
                }
                if (!is_dirty) continue;

                uint32_t img_pxs = img->pixels[iy * img->width + ix];
                if (!(img_pxs & SFTE_COLOR_ALPHA_MASK)) continue;

                SFTE_COLOR_BLEND_PIXEL(px_buf, out_x, out_y, ctx->width, img_pxs,
                                       (uint8_t)(img_pxs >> 24));
            }
        }
    }
}
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_CURSOR_TRAIL
/*
    Compositor pass for the cursor trail.
    Drawn as the last element in the rendering loop.
*/
static inline void _sfte_render_trail(sfte_ctx *ctx, void *px_buf, sfte_damage_rect *out_dmg) {
    if (ctx->term.trail_dmg.w > 0 && ctx->term.trail_dmg.h > 0)
        _sfte_render_damage_add(out_dmg, ctx->term.trail_dmg.x, ctx->term.trail_dmg.y,
                                ctx->term.trail_dmg.w, ctx->term.trail_dmg.h);

    float target_x = ctx->term.cursor_col * ctx->font.cell_width;
    float target_y = ctx->term.cursor_row * ctx->font.cell_height;

    if (ctx->term.hide_cursor || ctx->term.warp_tail || !ctx->term.is_trailing) {
        ctx->term.tail_rx = target_x;
        ctx->term.tail_ry = target_y;
        ctx->term.warp_tail = 0;

        ctx->term.trail_dmg.w = 0;
        ctx->term.trail_dmg.h = 0;
        return;
    }

    float trail_w = ctx->font.cell_width;
    float trail_h = ctx->font.cell_height;
    float y_off = 0;

    if (_SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_STYLE_UNDERLINE) {
        trail_h = ctx->font.cell_height * SFTE_CURSOR_THICK_RATIO;
        if (trail_h < 1.0f) trail_h = 1.0f;
        y_off = ctx->font.cell_height - trail_h;
    } else if (_SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_STYLE_BAR) {
        trail_w = ctx->font.cell_width * SFTE_CURSOR_THICK_RATIO;
        if (trail_w < 1.0f) trail_w = 1.0f;
    }

    float rx = trail_w * 0.5f;
    float ry = trail_h * 0.5f;

    float cx0 = ctx->term.tail_rx + rx;
    float cx1 = target_x + rx;
    float cy0 = ctx->term.tail_ry + y_off + ry;
    float cy1 = target_y + y_off + ry;

    float ab_x = cx1 - cx0, ab_y = cy1 - cy0;

    float l2 = ab_x * ab_x + ab_y * ab_y;
    if (l2 < 0.001f) return;

    float inv_l2 = 1.0f / l2;

    int32_t min_x = _SFTE_CLAMP((cx0 < cx1 ? cx0 : cx1) - rx + SFTE_WINDOW_PAD_X, 0, ctx->width);
    int32_t max_x = _SFTE_CLAMP((cx0 > cx1 ? cx0 : cx1) + rx + SFTE_WINDOW_PAD_X, 0, ctx->width);
    int32_t min_y = _SFTE_CLAMP((cy0 < cy1 ? cy0 : cy1) - ry + SFTE_WINDOW_PAD_Y, 0, ctx->height);
    int32_t max_y = _SFTE_CLAMP((cy0 > cy1 ? cy0 : cy1) + ry + SFTE_WINDOW_PAD_Y, 0, ctx->height);

#if SFTE_CURSOR_DYNAMIC
    uint32_t trail_color = ctx->term.cursor_color;
#else   // !SFTE_CURSOR_DYNAMIC
    uint32_t trail_color = SFTE_CURSOR_COLOR;
#endif  // !SFTE_CURSOR_DYNAMIC

    for (int32_t y = min_y; y < max_y; ++y) {
        float up_y = (float)(y - SFTE_WINDOW_PAD_Y) + 0.5f;
        float dy_from_cy0 = up_y - cy0;

        for (int32_t x = min_x; x < max_x; ++x) {
            float up_x = (float)(x - SFTE_WINDOW_PAD_X) + 0.5f;

            if (up_x >= target_x && up_x < target_x + trail_w && up_y >= target_y + y_off &&
                up_y < target_y + y_off + trail_h)
                continue;

            float dx_from_cx0 = up_x - cx0;
            float t = _SFTE_CLAMP((dx_from_cx0 * ab_x + dy_from_cy0 * ab_y) * inv_l2, 0.0f, 1.0f);

            if (fabsf(up_x - cx0 - t * ab_x) <= rx && fabsf(up_y - cy0 - t * ab_y) <= ry) {
                uint8_t alpha = (uint8_t)(128.0f * t);
                SFTE_COLOR_BLEND_PIXEL(px_buf, x, y, ctx->width, trail_color, alpha);
            }
        }
    }

    ctx->term.trail_dmg.x = min_x;
    ctx->term.trail_dmg.y = min_y;
    ctx->term.trail_dmg.w = max_x - min_x;
    ctx->term.trail_dmg.h = max_y - min_y;

    _sfte_render_damage_add(out_dmg, ctx->term.trail_dmg.x, ctx->term.trail_dmg.y,
                            ctx->term.trail_dmg.w, ctx->term.trail_dmg.h);
}
#endif  // SFTE_CURSOR_TRAIL

#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
/*
    Calculates the Y offsets for the outgoing and incoming screens during an alt-screen
   transition. Returns 2 when finished animating. Returns 1 when currently animating. Returns 0
   if static.
*/
static inline uint8_t _sfte_render_get_anim_offsets(sfte_ctx *ctx, int32_t *out_y, int32_t *in_y) {
    if (!ctx->term.is_animating) return 0;

    uint64_t now = SFTE_TIME_MS();
    float progress = (now - ctx->term.anim_start_ms) / SFTE_TERM_ANIM_DUR_MS;
    if (progress >= 1.0f) {
        ctx->term.is_animating = 0;
        return 2;
    }

    float ease = 1.0f - powf(1.0f - progress, 3.0f);
    int32_t slide = (uint32_t)(ease * ctx->height);

    if (ctx->term.anim_dir == 1) {  // entering alt / sliding up
        *out_y = -slide;
        *in_y = ctx->height - slide;
    } else {  // exiting alt / sliding down
        *out_y = slide;
        *in_y = -ctx->height + slide;
    }
    return 1;
}
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN

/*
    Prepares render passes.
    Returns the amount of passes in `passes` array.
*/
static inline uint8_t _sfte_render_prepare_passes(sfte_ctx *ctx, void *px_buf,
                                                  _sfte_pass_info *passes,
                                                  sfte_damage_rect *out_dmg) {
    passes[0].y_off = 0;
    passes[0].grid = ctx->term.cells;
    passes[0].hide_cursor = ctx->term.hide_cursor;

    uint8_t needs_wipe = 0;
    uint8_t passes_cnt = 1;
#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
    int32_t out_y = 0, in_y = 0;
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN

#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
    uint8_t anim_state = _sfte_render_get_anim_offsets(ctx, &out_y, &in_y);
    if (anim_state == 1 || anim_state == 2) needs_wipe = 1;
    if (anim_state == 1) passes_cnt = 2;
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN

#if SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP
    int32_t sb_diff = ctx->term.sb_offset - ctx->term.last_sb_offset;
    if (sb_diff != 0) {
        ctx->term.scroll_y_offset -= sb_diff * ctx->font.cell_height;
        ctx->term.last_sb_offset = ctx->term.sb_offset;
        ctx->term.is_scrolling = 1;
        ctx->term.last_scroll_ms = SFTE_TIME_MS();
    }

    if (ctx->term.is_scrolling) {
        uint64_t now = SFTE_TIME_MS();
        float dt = (float)(now - ctx->term.last_scroll_ms);
        ctx->term.last_scroll_ms = now;

        float decay = dt * SFTE_TERM_SCROLL_DECAY;
        if (decay > 1.0f) decay = 1.0f;

        ctx->term.scroll_y_offset -= ctx->term.scroll_y_offset * decay;

        if (fabsf(ctx->term.scroll_y_offset) < 0.5f) {
            ctx->term.scroll_y_offset = 0.0f;
            ctx->term.is_scrolling = 0;
        }
        needs_wipe = 1;
    }
#endif  // SFTE_TERM_SCROLL_SMOOTH && SFTE_TERM_SCROLLBACK_CAP

    if (needs_wipe) {
        _sfte_render_damage_add(out_dmg, 0, 0, ctx->width, ctx->height);
        ctx->padding_dirty = 1;
        uint32_t clear_bg = (SFTE_COLOR_BG_OPACITY << 24) |
                            (SFTE_COLOR_BG & ~SFTE_COLOR_ALPHA_MASK);
        for (int32_t y = 0; y < ctx->height; ++y)
            for (int32_t x = 0; x < ctx->width; ++x)
                SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, ctx->width, ctx->height, clear_bg);

        for (int32_t i = 0; i < ctx->term.rows * ctx->term.cols; ++i) {
            ctx->term.cells[i].dirty = 1;
#if SFTE_TERM_ALT_SCREEN
            if (passes_cnt == 2 && ctx->term.alt_cells) ctx->term.alt_cells[i].dirty = 1;
#endif  // SFTE_TERM_ALT_SCREEN
        }
    }

#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
    if (passes_cnt == 2) {
        passes[0].y_off = out_y;
        passes[0].grid = ctx->term.alt_cells;
        passes[0].hide_cursor = 1;

        passes[1].y_off = in_y;
#if SFTE_TERM_SCROLL_SMOOTH
        passes[1].y_off += (int32_t)ctx->term.scroll_y_offset;
#endif  // SFTE_TERM_SCROLL_SMOOTH
        passes[1].grid = ctx->term.cells;
        passes[1].hide_cursor = ctx->term.hide_cursor;
    }
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
#if SFTE_TERM_SCROLL_SMOOTH
    passes[0].y_off = (int32_t)ctx->term.scroll_y_offset;
#endif  // SFTE_TERM_SCROLL_SMOOTH

    return passes_cnt;
}

/*
    Fast integer-based alpha blending.
    Used for cursor trails and antialiased font rendering to avoid slow floating-point math.
*/
static inline uint32_t _sfte_render_blend_argb(uint32_t dst, uint32_t src_col, uint8_t src_a) {
    if (src_a == 0) return dst;  // no trail
    if (src_a == 255)
        return SFTE_COLOR_ALPHA_MASK | (src_col & ~SFTE_COLOR_ALPHA_MASK);  // solid trail

    uint8_t da = (dst >> 24) & 0xFF;
    uint8_t dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
    uint8_t sr = (src_col >> 16) & 0xFF, sg = (src_col >> 8) & 0xFF, sb = src_col & 0xFF;

    uint8_t out_r = (sr * src_a + dr * (255 - src_a)) >> 8;
    uint8_t out_g = (sg * src_a + dg * (255 - src_a)) >> 8;
    uint8_t out_b = (sb * src_a + db * (255 - src_a)) >> 8;
    uint8_t out_a = da + ((src_a * (255 - da)) >> 8);

    return (out_a << 24) | (out_r << 16) | (out_g << 8) | out_b;
}

/*
    Paints the solid background color for a terminal cell.
*/
static inline void _sfte_render_bg_cell(sfte_ctx *ctx, void *px_buf, int16_t col, int16_t row,
                                        int32_t y_off, uint32_t bg) {
    int32_t cx = col * ctx->font.cell_width + SFTE_WINDOW_PAD_X;
    int32_t cy = row * ctx->font.cell_height + SFTE_WINDOW_PAD_Y;
    uint32_t final_bg = (SFTE_COLOR_BG_OPACITY << 24) | (bg & ~SFTE_COLOR_ALPHA_MASK);

    for (int32_t y = 0; y < ctx->font.cell_height; ++y)
        for (int32_t x = 0; x < ctx->font.cell_width; ++x)
            SFTE_COLOR_DRAW_PIXEL(px_buf, cx + x, cy + y + y_off, ctx->width, ctx->height,
                                  final_bg);
}

/*
    Samples the font atlas and paints a glyph.
    The texture atlas only stores alpha values.
    It blends the requested foreground color into the existing background using this alpha mask.
*/
static inline void _sfte_render_fg_cell(sfte_ctx *ctx, void *px_buf, int16_t col, int16_t row,
                                        int32_t y_off, sfte_rune rune, uint16_t glyph_id,
                                        uint8_t font_idx, uint32_t fg,
                                        sfte_font_cache *target_cache) {
    if (rune == ' ') return;

    int32_t cx = col * ctx->font.cell_width + SFTE_WINDOW_PAD_X;
    int32_t cy = row * ctx->font.cell_height + SFTE_WINDOW_PAD_Y;

#if SFTE_TERM_CUSTOM_BOXES && !SFTE_TERM_ASCII_CHARSET
    if ((rune >= 0x2500 && rune <= 0x259F) || (rune >= 0x2800 && rune <= 0x28FF))
        if (_sfte_render_box_char(ctx, px_buf, cx, cy, fg, rune, y_off)) return;
#endif  // SFTE_TERM_CUSTOM_BOXES

    sfte_glyph *g = _sfte_font_get_glyph(ctx, target_cache, glyph_id, font_idx);
    if (!g) return;

    int32_t glyph_width = g->x1 - g->x0;
    int32_t glyph_height = g->y1 - g->y0;

    int32_t draw_x = cx + (int)g->xoff;
    int32_t draw_y = cy + ctx->font.ascent + (int)g->yoff + y_off;

    for (int32_t y = 0; y < glyph_height; ++y) {
        for (int32_t x = 0; x < glyph_width; ++x) {
            int32_t screen_x = draw_x + x;
            int32_t screen_y = draw_y + y;
            if (screen_x < 0 || screen_x >= ctx->width || screen_y < 0 || screen_y >= ctx->height)
                continue;

            uint8_t alpha = target_cache
                                ->atlas_pxs[(g->y0 + y) * SFTE_FONT_ATLAS_SIZE + (g->x0 + x)] &
                            0xFF;

            SFTE_COLOR_BLEND_PIXEL(px_buf, screen_x, screen_y, ctx->width, fg, alpha);
        }
    }
}

/*
    Renders extended underline styles (straight, double, undercurl, dotted, dotted, dashed).

    Undercurls require a periodic wave. To avoid slow math calls per-pixel, this implementation
    approximates a triangle wave using integer mod arithmetic.
*/
static inline void _sfte_render_underline_cell(sfte_ctx *ctx, void *px_buf, int32_t cx, int32_t cy,
                                               int32_t render_w, sfte_cell *vcell) {
    uint32_t base_ul_col = _sfte_grid_get_ul(vcell);
    uint32_t underline_col = SFTE_COLOR_ALPHA_MASK | (base_ul_col & ~SFTE_COLOR_ALPHA_MASK);

    int32_t thick = ctx->font.cell_height * SFTE_UNDERLINE_THICK_RATIO;
    if (thick < 1) thick = 1;

    uint8_t style = _SFTE_UNDERLINE_STYLE_STRAIGHT;
#if SFTE_UNDERLINE_EXTENDED
    style = _SFTE_CLAMP(vcell->ul_style, _SFTE_UNDERLINE_STYLE_STRAIGHT,
                        _SFTE_UNDERLINE_STYLE_DASHED);
#endif  // SFTE_UNDERLINE_EXTENDED

    int32_t offset = ctx->font.cell_height * SFTE_UNDERLINE_OFFSET_RATIO;
    if (offset < 1) offset = 1;

    int32_t base_y = cy + ctx->font.ascent + offset;
    if (base_y + thick > cy + ctx->font.cell_height) base_y = cy + ctx->font.cell_height - thick;

    for (int32_t x = cx; x < cx + render_w; ++x) {
        if (x >= ctx->width) break;

        int32_t grid_x = x - SFTE_WINDOW_PAD_X;
        int32_t local_x = grid_x % ctx->font.cell_width;

        switch (style) {
        case _SFTE_UNDERLINE_STYLE_CURLY: {
            int32_t half_w = ctx->font.cell_width / 2;
            if (half_w == 0) half_w = 1;

            int32_t amp = thick + 1;
            int32_t dist = local_x > half_w ? local_x - half_w : half_w - local_x;
            int32_t y_off = (dist * amp) / half_w - (amp / 2);

            for (int32_t dy = 0; dy < thick; ++dy) {
                int32_t py = base_y + y_off + dy;
                if (py >= 0 && py < ctx->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, py, ctx->width, ctx->height, underline_col);
            }
            break;
        }
        case _SFTE_UNDERLINE_STYLE_DOUBLE: {
            int32_t half_thick = thick / 2;
            if (half_thick < 1) half_thick = 1;
            int32_t gap = half_thick < 2 ? 1 : half_thick;
            for (int32_t dy = 0; dy < half_thick; ++dy) {
                int32_t py1 = base_y - half_thick + dy;
                int32_t py2 = base_y + gap + dy;
                if (py1 >= 0 && py1 < ctx->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, py1, ctx->width, ctx->height, underline_col);
                if (py2 >= 0 && py2 < ctx->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, py2, ctx->width, ctx->height, underline_col);
            }
            break;
        }
        case _SFTE_UNDERLINE_STYLE_DOTTED:
            if ((grid_x / thick) % 2 != 0) break;
            // fallthrough
        case _SFTE_UNDERLINE_STYLE_DASHED:
            if (style == _SFTE_UNDERLINE_STYLE_DASHED && ((grid_x / thick) % 5 >= 3)) break;
            // fallthrough
        case _SFTE_UNDERLINE_STYLE_STRAIGHT:
        default:
            for (int32_t dy = 0; dy < thick; ++dy) {
                int32_t py = base_y + dy;
                if (py >= 0 && py < ctx->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, py, ctx->width, ctx->height, underline_col);
            }
        }
    }
}

/*
    Renders non-block cursors (bar/underline).
*/
static inline void _sfte_render_cursor_shape(sfte_ctx *ctx, void *px_buf, int32_t cx, int32_t cy,
                                             int32_t render_w) {
#if SFTE_CURSOR_DYNAMIC
    uint32_t active_cur_color = ctx->term.cursor_color;
#else   // !SFTE_CURSOR_DYNAMIC
    uint32_t active_cur_color = SFTE_CURSOR_COLOR;
#endif  // !SFTE_CURSOR_DYNAMIC
    uint32_t cur_col = SFTE_COLOR_ALPHA_MASK | (active_cur_color & ~SFTE_COLOR_ALPHA_MASK);

#if SFTE_TERM_FOCUS
    if (!ctx->term.is_focused) {
        for (int32_t y = cy; y < cy + ctx->font.cell_height; ++y) {
            SFTE_COLOR_DRAW_PIXEL(px_buf, cx, y, ctx->width, ctx->height, cur_col);
            SFTE_COLOR_DRAW_PIXEL(px_buf, cx + render_w - 1, y, ctx->width, ctx->height, cur_col);
        }
        for (int32_t x = cx; x < cx + render_w; ++x) {
            SFTE_COLOR_DRAW_PIXEL(px_buf, x, cy, ctx->width, ctx->height, cur_col);
            SFTE_COLOR_DRAW_PIXEL(px_buf, x, cy + ctx->font.cell_height - 1, ctx->width,
                                  ctx->height, cur_col);
        }
        return;
    }
#endif  // SFTE_TERM_FOCUS

    if (_SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_STYLE_UNDERLINE) {
        int32_t thick = ctx->font.cell_height * SFTE_CURSOR_THICK_RATIO;
        if (thick < 1) thick = 1;

        for (int32_t y = cy + ctx->font.cell_height - thick; y < cy + ctx->font.cell_height; ++y)
            for (int32_t x = cx; x < cx + render_w; ++x)
                if (x < ctx->width && y < ctx->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, ctx->width, ctx->height, cur_col);
    } else if (_SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_STYLE_BAR) {
        int32_t thick = ctx->font.cell_width * SFTE_CURSOR_THICK_RATIO;
        if (thick < 1) thick = 1;

        for (int32_t y = cy; y < cy + ctx->font.cell_height; ++y)
            for (int32_t x = cx; x < cx + thick; ++x)
                if (x < ctx->width && y < ctx->height)
                    SFTE_COLOR_DRAW_PIXEL(px_buf, x, y, ctx->width, ctx->height, cur_col);
    }
}

#if SFTE_TERM_CUSTOM_BOXES && !SFTE_TERM_ASCII_CHARSET
/*
    A stripped down Xialoin Wu line algorithm optimized for integer endpoints.
    Draws anti-aliased lines for terminal cell diagonals.
*/
static inline void _sfte_render_line(sfte_ctx *ctx, void *px_buf, int32_t x0, int32_t y0,
                                     int32_t x1, int32_t y1, int32_t thickness, uint32_t col) {
    if (thickness < 1) thickness = 1;
    uint8_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        int32_t tmp = x0;
        x0 = y0;
        y0 = tmp;
        tmp = x1;
        x1 = y1;
        y1 = tmp;
    }
    if (x0 > x1) {
        int32_t tmp = x0;
        x0 = x1;
        x1 = tmp;
        tmp = y0;
        y0 = y1;
        y1 = tmp;
    }

    float dx = (float)(x1 - x0);
    float dy = (float)(y1 - y0);
    float grad = (dx == 0.0f) ? 1.0f : (dy / dx);
    float intery = y0;

#define _SFTE_SAFE_BLEND(_x, _y, _a)                                                               \
    if ((_x) >= 0 && (_x) < ctx->width && (_y) >= 0 && (_y) < ctx->height)                         \
        SFTE_COLOR_BLEND_PIXEL(px_buf, _x, _y, ctx->width, col, _a);

    for (int32_t x = x0; x <= x1; ++x) {
        float intery_frac = intery - floorf(intery);
        uint8_t a1 = (uint8_t)((1.0f - intery_frac) * 255.0f);
        uint8_t a2 = (uint8_t)(intery_frac * 255.0f);
        int32_t y = (int32_t)intery;

        int32_t bound_min = y - (thickness / 2);
        int32_t bound_max = bound_min + thickness;

        if (steep) {
            _SFTE_SAFE_BLEND(bound_min, x, a1);
            for (int32_t w = 1; w < thickness; ++w) _SFTE_SAFE_BLEND(bound_min + w, x, 255);
            _SFTE_SAFE_BLEND(bound_max, x, a2);
        } else {
            _SFTE_SAFE_BLEND(x, bound_min, a1);
            for (int32_t w = 1; w < thickness; ++w) _SFTE_SAFE_BLEND(x, bound_min + w, 255);
            _SFTE_SAFE_BLEND(x, bound_max, a2);
        }
        intery += grad;
    }
}

/*
    Renders box characters (0x2500-0x259F) to ensure there's no gaps between characters.
    Returns 0 for unhandled cases, ensuring that they're drawn using font data.
 */
static inline uint8_t _sfte_render_box_char(sfte_ctx *ctx, void *px_buf, int32_t cx, int32_t cy,
                                            uint32_t col, uint32_t rune, int32_t y_off) {
    int16_t cw = ctx->font.cell_width;
    int16_t ch = ctx->font.cell_height;

#define _SFTE_RECT(rx, ry, rw, rh)                                                                 \
    for (int16_t y = (ry); y < (ry) + (rh); ++y)                                                   \
        for (int16_t x = (rx); x < (rx) + (rw); ++x)                                               \
    SFTE_COLOR_DRAW_PIXEL(px_buf, x, y + y_off, ctx->width, ctx->height, col)

    // Block elements
    if (rune >= 0x2580 && rune <= 0x259F) {
#define _BLOCK_B(f)                                                                                \
    _SFTE_RECT(cx, cy + ch - ((((f) * ch) / 8 > 0) ? (((f) * ch) / 8) : 1), cw,                    \
               (((f) * ch) / 8 > 0) ? (((f) * ch) / 8) : 1)
#define _BLOCK_L(f) _SFTE_RECT(cx, cy, (((f) * cw) / 8 > 0) ? (((f) * cw) / 8) : 1, ch)
#define _BLOCK_T(f) _SFTE_RECT(cx, cy, cw, (((f) * ch) / 8 > 0) ? (((f) * ch) / 8) : 1)
#define _BLOCK_R(f)                                                                                \
    _SFTE_RECT(cx + cw - ((((f) * cw) / 8 > 0) ? (((f) * cw) / 8) : 1), cy,                        \
               (((f) * cw) / 8 > 0) ? (((f) * cw) / 8) : 1, ch)
#define _QUAD(tl, tr, bl, br)                                                                      \
    do {                                                                                           \
        if (tl) _SFTE_RECT(cx, cy, cw / 2, ch / 2);                                                \
        if (tr) _SFTE_RECT(cx + cw / 2, cy, cw / 2, ch / 2);                                       \
        if (bl) _SFTE_RECT(cx, cy + ch / 2, cw / 2, ch / 2);                                       \
        if (br) _SFTE_RECT(cx + cw / 2, cy + ch / 2, cw / 2, ch / 2);                              \
    } while (0)

        switch (rune) {
        case 0x2580: _BLOCK_T(4); break;        // ▀
        case 0x2581: _BLOCK_B(1); break;        // ▁
        case 0x2582: _BLOCK_B(2); break;        // ▂
        case 0x2583: _BLOCK_B(3); break;        // ▃
        case 0x2584: _BLOCK_B(4); break;        // ▄
        case 0x2585: _BLOCK_B(5); break;        // ▅
        case 0x2586: _BLOCK_B(6); break;        // ▆
        case 0x2587: _BLOCK_B(7); break;        // ▇
        case 0x2588: _BLOCK_B(8); break;        // █
        case 0x2589: _BLOCK_L(7); break;        // ▉
        case 0x258A: _BLOCK_L(6); break;        // ▊
        case 0x258B: _BLOCK_L(5); break;        // ▋
        case 0x258C: _BLOCK_L(4); break;        // ▌
        case 0x258D: _BLOCK_L(3); break;        // ▍
        case 0x258E: _BLOCK_L(2); break;        // ▎
        case 0x258F: _BLOCK_L(1); break;        // ▏
        case 0x2590: _BLOCK_R(4); break;        // ▐
        case 0x2594: _BLOCK_T(1); break;        // ▔
        case 0x2595: _BLOCK_R(1); break;        // ▕
        case 0x2596: _QUAD(0, 0, 1, 0); break;  // ▖
        case 0x2597: _QUAD(0, 0, 0, 1); break;  // ▗
        case 0x2598: _QUAD(1, 0, 0, 0); break;  // ▘
        case 0x2599: _QUAD(1, 0, 1, 1); break;  // ▙
        case 0x259A: _QUAD(1, 0, 0, 1); break;  // ▚
        case 0x259B: _QUAD(1, 1, 1, 0); break;  // ▛
        case 0x259C: _QUAD(1, 1, 0, 1); break;  // ▜
        case 0x259D: _QUAD(0, 1, 0, 0); break;  // ▝
        case 0x259E: _QUAD(0, 1, 1, 0); break;  // ▞
        case 0x259F: _QUAD(0, 1, 1, 1); break;  // ▟
        default: return 0;
        }
        return 1;
    }

    // Braille patterns
    if (rune >= 0x2800 && rune <= 0x28FF) {
        uint8_t dots = rune - 0x2800;
        if (dots == 0) return 1;

        // 2 columns, 4 rows
        int16_t sw = cw / 2;
        int16_t sh = ch / 4;

        int16_t dot_w = (cw / 4 > 0) ? cw / 4 : 1;
        int16_t dot_h = (ch / 8 > 0) ? ch / 8 : 1;

        int16_t ox = (sw - dot_w) / 2;
        int16_t oy = (sh - dot_h) / 2;

#define _DRAW_DOT(bit, col, row)                                                                   \
    if (dots & (bit)) _SFTE_RECT(cx + ((col) * sw) + ox, cy + ((row) * sh) + oy, dot_w, dot_h);

        _DRAW_DOT(0x01, 0, 0);  // ⠁
        _DRAW_DOT(0x02, 0, 1);  // ⠂
        _DRAW_DOT(0x04, 0, 2);  // ⠄
        _DRAW_DOT(0x08, 1, 0);  // ⠈
        _DRAW_DOT(0x10, 1, 1);  // ⠐
        _DRAW_DOT(0x20, 1, 2);  // ⠠
        _DRAW_DOT(0x40, 0, 3);  // ⡀
        _DRAW_DOT(0x80, 1, 3);  // ⢀

#undef _DRAW_DOT
        return 1;
    }

    uint8_t up = 0, down = 0, left = 0, right = 0;
    uint8_t heavy = 0;

#define _SET_LINE(u, d, l, r, h) up = u, down = d, left = l, right = r, heavy = h

    switch (rune) {
    case 0x2500: _SET_LINE(0, 0, 1, 1, 0); break;  // ─
    case 0x2501: _SET_LINE(0, 0, 1, 1, 1); break;  // ━
    case 0x2502: _SET_LINE(1, 1, 0, 0, 0); break;  // │
    case 0x2503: _SET_LINE(1, 1, 0, 0, 1); break;  // ┃
    case 0x250C: _SET_LINE(0, 1, 0, 1, 0); break;  // ┌
    case 0x250F: _SET_LINE(0, 1, 0, 1, 1); break;  // ┏
    case 0x2510: _SET_LINE(0, 1, 1, 0, 0); break;  // ┐
    case 0x2513: _SET_LINE(0, 1, 1, 0, 1); break;  // ┓
    case 0x2514: _SET_LINE(1, 0, 0, 1, 0); break;  // └
    case 0x2517: _SET_LINE(1, 0, 0, 1, 1); break;  // ┗
    case 0x2518: _SET_LINE(1, 0, 1, 0, 0); break;  // ┘
    case 0x251B: _SET_LINE(1, 0, 1, 0, 1); break;  // ┛
    case 0x251C: _SET_LINE(1, 1, 0, 1, 0); break;  // ├
    case 0x2523: _SET_LINE(1, 1, 0, 1, 1); break;  // ┣
    case 0x2524: _SET_LINE(1, 1, 1, 0, 0); break;  // ┤
    case 0x252B: _SET_LINE(1, 1, 1, 0, 1); break;  // ┫
    case 0x252C: _SET_LINE(0, 1, 1, 1, 0); break;  // ┬
    case 0x2533: _SET_LINE(0, 1, 1, 1, 1); break;  // ┳
    case 0x2534: _SET_LINE(1, 0, 1, 1, 0); break;  // ┴
    case 0x253B: _SET_LINE(1, 0, 1, 1, 1); break;  // ┻
    case 0x253C: _SET_LINE(1, 1, 1, 1, 0); break;  // ┼
    case 0x254B: _SET_LINE(1, 1, 1, 1, 1); break;  // ╋
    case 0x2571:                                   // ╱
    case 0x2572:                                   // ╲
    case 0x2573:                                   // ╳
    {
        int16_t b_lw = (cw / 8 > 0) ? cw / 8 : 1;
        if (rune == 0x2571 || rune == 0x2573)
            _sfte_render_line(ctx, px_buf, cx, cy + ch - 1 + y_off, cx + cw - 1, cy + y_off, b_lw,
                              col);
        if (rune == 0x2572 || rune == 0x2573)
            _sfte_render_line(ctx, px_buf, cx, cy + y_off, cx + cw - 1, cy + ch - 1 + y_off, b_lw,
                              col);
        return 1;
    }
    default: return 0;
    }

    int16_t base_lw = (cw / 8 > 0) ? cw / 8 : 1;
    int16_t lw = heavy ? (base_lw * 3) : base_lw;
    int16_t hw = lw / 2;
    int16_t mx = cx + cw / 2;
    int16_t my = cy + ch / 2;

    if (up) _SFTE_RECT(mx - hw, cy, lw, my - cy + hw);
    if (down) _SFTE_RECT(mx - hw, my - hw, lw, cy + ch - my + hw);
    if (left) _SFTE_RECT(cx, my - hw, mx - cx + hw, lw);
    if (right) _SFTE_RECT(mx - hw, my - hw, cx + cw - mx + hw, lw);

#undef _SET_LINE
#undef _SFTE_RECT

    return 1;
}
#endif  // SFTE_TERM_CUSTOM_BOXES && !SFTE_TERM_ASCII_CHARSET

/*
    Dispatcher for terminal text decorations (underlines, cursor).
*/
static inline void _sfte_render_decorations_cell(sfte_ctx *ctx, void *px_buf, int16_t col,
                                                 int16_t row, int32_t y_off, sfte_cell *vcell,
                                                 uint8_t is_cursor) {
    int32_t cx = col * ctx->font.cell_width + SFTE_WINDOW_PAD_X;
    int32_t cy = row * ctx->font.cell_height + SFTE_WINDOW_PAD_Y;

    int32_t render_w = ctx->font.cell_width;
#if SFTE_FONT_WIDE_CHARS
    render_w *= (vcell->attr & _SFTE_ATTR_WIDE) ? 2 : 1;
#endif  // SFTE_FONT_WIDE_CHARS

    if (vcell->attr & _SFTE_ATTR_UNDERLINE)
        _sfte_render_underline_cell(ctx, px_buf, cx, cy + y_off, render_w, vcell);

    uint8_t draw_shape = is_cursor && (_SFTE_CUR_STYLE(ctx) != SFTE_CURSOR_STYLE_BLOCK
#if SFTE_TERM_FOCUS
                                       || !ctx->term.is_focused
#endif  // SFTE_TERM_FOCUS
                                      );
    if (draw_shape) _sfte_render_cursor_shape(ctx, px_buf, cx, cy + y_off, render_w);
}

/*
    Background rendering pass.
    Renders the whole grid, contrary to `_sfte_render_bg_cell`.
*/
static inline void _sfte_render_bg_grid(sfte_ctx *ctx, void *px_buf, int16_t vis_col,
                                        int16_t vis_row, int32_t y_off) {
    for (int16_t r = 0; r < ctx->term.rows; ++r) {
        int32_t logical_r = _sfte_grid_vis2log(ctx, r);
        for (int16_t c = 0; c < ctx->term.cols; ++c) {
            sfte_cell *vcell = _sfte_grid_get_cell(ctx, c, logical_r);
            if (!vcell->dirty) continue;

            uint32_t fg = _sfte_grid_get_fg(vcell);
            uint32_t bg = _sfte_grid_get_bg(vcell);
            uint8_t attr = vcell->attr;

#if SFTE_INPUT_SELECTION
            if (_sfte_input_is_selected(ctx, c, logical_r)) attr |= _SFTE_ATTR_REVERSE;
#endif  // SFTE_INPUT_SELECTION

            if (attr & _SFTE_ATTR_REVERSE) {
                uint32_t tmp = fg;
                fg = bg;
                bg = tmp;
            }

            uint8_t is_cursor = (c == vis_col && r == vis_row && !ctx->term.hide_cursor);

#if SFTE_TERM_SCROLLBACK_CAP
            // Hide active cursor when viewing scrollback history
            if (ctx->term.sb_offset > 0) is_cursor = 0;
#endif  // SFTE_TERM_SCROLLBACK_CAP

#if SFTE_FONT_WIDE_CHARS
            if (!is_cursor && (attr & _SFTE_ATTR_DUMMY) && c > 0 && c - 1 == vis_col &&
                r == vis_row && !ctx->term.hide_cursor)
                is_cursor = 1;
#endif  // SFTE_FONT_WIDE_CHARS
#if SFTE_CURSOR_BLINK
            if (!ctx->term.blink_visible) is_cursor = 0;
#endif  // SFTE_CURSOR_BLINK

            uint8_t is_solid_block = is_cursor && _SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_STYLE_BLOCK;
#if SFTE_TERM_FOCUS
            is_solid_block &= ctx->term.is_focused;
#endif  // SFTE_TERM_FOCUS

            if (is_solid_block) {
#if SFTE_CURSOR_DYNAMIC
                _sfte_render_bg_cell(ctx, px_buf, c, r, y_off, ctx->term.cursor_color);
#else   // !SFTE_CURSOR_DYNAMIC
                _sfte_render_bg_cell(ctx, px_buf, c, r, y_off, SFTE_CURSOR_COLOR);
#endif  // !SFTE_CURSOR_DYNAMIC
            } else
                _sfte_render_bg_cell(ctx, px_buf, c, r, y_off, bg);
        }
    }
}

#if SFTE_FONT_LIGATURES
/*
    Computes and returns a 64-bit FNV-1a hash of the row's shaping inputs.
*/
static inline uint64_t _sfte_render_get_row_hash(sfte_ctx *ctx, int32_t logical_row) {
    uint64_t hash = 14695981039346656037ULL;  // FNV-1a 64-bit offset basis
    const uint64_t prime = 1099511628211ULL;

    for (int16_t c = 0; c < ctx->term.cols; ++c) {
        sfte_cell *vcell = _sfte_grid_get_cell(ctx, c, logical_row);
        uint8_t attr = vcell->attr;
        uint32_t fg = _sfte_grid_get_fg(vcell);
        uint32_t bg = _sfte_grid_get_bg(vcell);

        hash ^= ctx->term.render_shaper_ids[c];
        hash *= prime;
        hash ^= ctx->term.render_font_indices[c];
        hash *= prime;
        hash ^= attr;
        hash *= prime;
        hash ^= fg;
        hash *= prime;
        hash ^= bg;
        hash *= prime;
    }

    return hash;
}

/*
    Chunks a row into continuous style/color blocks and passes them to the OpenType shaper.
    Ensures ligatures do not bleed across formatting boundaries.
*/
static inline void _sfte_render_shape_fg_row(sfte_ctx *ctx, int32_t logical_row) {
    int16_t span_start = 0;
    while (span_start < ctx->term.cols) {
        if (!ctx->term.render_shaper_ids[span_start]) {
            span_start++;
            continue;
        }

        sfte_cell *start_cell = _sfte_grid_get_cell(ctx, span_start, logical_row);
        uint32_t active_fg = _sfte_grid_get_fg(start_cell);
        uint32_t active_bg = _sfte_grid_get_bg(start_cell);

        sfte_font_cache *active_cache = ctx->term.render_target_caches[span_start];
        uint8_t active_font_idx = ctx->term.render_font_indices[span_start];

        int16_t span_end = span_start + 1;
        while (span_end < ctx->term.cols && ctx->term.render_shaper_ids[span_end] != 0) {
            sfte_cell *next_cell = _sfte_grid_get_cell(ctx, span_end, logical_row);
            if (_sfte_grid_get_fg(next_cell) != active_fg ||
                _sfte_grid_get_bg(next_cell) != active_bg ||
                ctx->term.render_target_caches[span_end] != active_cache ||
                ctx->term.render_font_indices[span_end] != active_font_idx)
                break;
            span_end++;
        }

        _sfte_shaper_shape_row(&active_cache->shaper[active_font_idx],
                               active_cache->ttf_buf[active_font_idx],
                               &ctx->term.render_shaper_ids[span_start], span_end - span_start);

        span_start = span_end;
    }
}
#endif  // SFTE_FONT_LIGATURES

/*
    Extracts a single terminal row into linear arrays for shaping and rendering.
    Resolves runes into raw glyph IDs and identifies style/color boundaries.
*/
static inline void _sfte_render_extract_fg_row(sfte_ctx *ctx, int32_t logical_row, int16_t row,
                                               int16_t vis_col, int16_t vis_row) {
    (void)row, (void)vis_col, (void)vis_row;
    for (int16_t c = 0; c < ctx->term.cols; ++c) {
        sfte_cell *vcell = _sfte_grid_get_cell(ctx, c, logical_row);
        sfte_rune rune = vcell->rune ? vcell->rune : ' ';
#if defined(SFTE_FONT_BOLD) || defined(SFTE_FONT_ITALIC) || defined(SFTE_FONT_BOLD_ITALIC)
        uint8_t attr = vcell->attr;
#endif  // defined(SFTE_FONT_BOLD) || defined(SFTE_FONT_ITALIC) || defined(SFTE_FONT_BOLD_ITALIC)

        sfte_font_cache *target_cache = &ctx->font.regular;
#ifdef SFTE_FONT_BOLD
        if (attr & _SFTE_ATTR_BOLD) target_cache = &ctx->font.bold;
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
        if (attr & _SFTE_ATTR_ITALIC) target_cache = &ctx->font.italic;
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
        if ((attr & _SFTE_ATTR_BOLD) && (attr & _SFTE_ATTR_ITALIC))
            target_cache = &ctx->font.bold_italic;
#endif  // SFTE_FONT_BOLD_ITALIC
        ctx->term.render_target_caches[c] = target_cache;
        _sfte_font_resolve_rune(target_cache, rune, &ctx->term.render_font_indices[c],
                                &ctx->term.render_ids[c]);

#if SFTE_FONT_LIGATURES
        uint8_t is_cursor = (c == vis_col && row == vis_row && !ctx->term.hide_cursor);
        ctx->term.render_shaper_ids[c] = (rune == ' ' || rune == 0 || is_cursor)
                                             ? 0
                                             : ctx->term.render_ids[c];
#endif  // SFTE_FONT_LIGATURES
    }
}

/*
    Iterates over a shaped row and dispatches foreground/decoration drawing.
*/
static inline void _sfte_render_fg_row(sfte_ctx *ctx, void *px_buf, int16_t row,
                                       int32_t logical_row, int16_t vis_col, int16_t vis_row,
                                       int32_t y_off, sfte_damage_rect *out_dmg) {
    for (int16_t c = 0; c < ctx->term.cols; ++c) {
        sfte_cell *vcell = _sfte_grid_get_cell(ctx, c, logical_row);
        if (!vcell->dirty) continue;

        uint8_t attr = vcell->attr;

#if SFTE_FONT_WIDE_CHARS
        if (attr & _SFTE_ATTR_DUMMY) {
            _sfte_render_damage_add(out_dmg, c * ctx->font.cell_width + SFTE_WINDOW_PAD_X,
                                    row * ctx->font.cell_height + SFTE_WINDOW_PAD_Y,
                                    ctx->font.cell_width, ctx->font.cell_height);
            vcell->dirty = 0;
            continue;
        }
#endif
        sfte_rune rune = vcell->rune ? vcell->rune : ' ';
        uint32_t fg = _sfte_grid_get_fg(vcell);
        uint32_t bg = _sfte_grid_get_bg(vcell);

        if (attr & _SFTE_ATTR_REVERSE
#if SFTE_TERM_FOCUS
            && ctx->term.is_focused
#endif  // SFTE_TERM_FOCUS
        ) {
            uint32_t tmp = fg;
            fg = bg;
            bg = tmp;
        }
#ifdef SFTE_BOLD_WHITE
        if (attr & _SFTE_ATTR_BOLD) fg = SFTE_COLOR_FG;
#endif

        uint8_t is_cursor = (c == vis_col && row == vis_row && !ctx->term.hide_cursor);
#if SFTE_CURSOR_BLINK
        if (!ctx->term.blink_visible) is_cursor = 0;
#endif
        if (is_cursor && _SFTE_CUR_STYLE(ctx) == SFTE_CURSOR_STYLE_BLOCK) fg = bg;

#if SFTE_FONT_LIGATURES
        if (ctx->term.render_font_indices[c] == 0 && ctx->term.render_shaper_ids[c] != 0 &&
            ctx->term.render_shaper_ids[c] != ctx->term.render_ids[c]) {
            ctx->term.render_ids[c] = ctx->term.render_shaper_ids[c];
        }
#endif  // SFTE_TERM_LIGATURES

        _sfte_render_fg_cell(ctx, px_buf, c, row, y_off, rune, ctx->term.render_ids[c],
                             ctx->term.render_font_indices[c], fg,
                             ctx->term.render_target_caches[c]);
        _sfte_render_decorations_cell(ctx, px_buf, c, row, y_off, vcell, is_cursor);

        int32_t dmg_cy = row * ctx->font.cell_height + SFTE_WINDOW_PAD_Y;
        int32_t dmg_ch = ctx->font.cell_height;

        if (row == 0) {
            dmg_cy = 0;
            dmg_ch += SFTE_WINDOW_PAD_Y;
        } else if (row == ctx->term.rows - 1) {
            dmg_ch += ctx->height - (dmg_cy + dmg_ch);
        }
        _sfte_render_damage_add(out_dmg, 0, dmg_cy, ctx->width, dmg_ch);
    }
}

/*
    Foreground rendering pass.
    Renders the whole grid, contrary to `_sfte_render_fg_cell`.
*/
static inline void _sfte_render_fg_grid(sfte_ctx *ctx, void *px_buf, int16_t vis_col,
                                        int16_t vis_row, int32_t y_off, sfte_damage_rect *out_dmg) {
    for (int16_t r = 0; r < ctx->term.rows; ++r) {
        int32_t logical_r = _sfte_grid_vis2log(ctx, r);
        _sfte_render_extract_fg_row(ctx, logical_r, r, vis_col, vis_row);

#if SFTE_FONT_LIGATURES
        int32_t cache_idx = (logical_r >= 0) ? ((logical_r + ctx->term.grid_off) % ctx->term.rows)
                                             : r;
        uint64_t row_hash = _sfte_render_get_row_hash(ctx, logical_r);
        uint16_t *memo_ids = &ctx->term.row_shaper_ids[cache_idx * ctx->term.cols];
        if (ctx->term.row_hashes[cache_idx] == row_hash)
            memcpy(ctx->term.render_shaper_ids, memo_ids, ctx->term.cols * sizeof(uint16_t));
        else {
            _sfte_render_shape_fg_row(ctx, logical_r);
            ctx->term.row_hashes[cache_idx] = row_hash;
            memcpy(memo_ids, ctx->term.render_shaper_ids, ctx->term.cols * sizeof(uint16_t));
        }
#endif  // SFTE_FONT_LIGATURES
        _sfte_render_fg_row(ctx, px_buf, r, logical_r, vis_col, vis_row, y_off, out_dmg);
    }
}

// =================================================================================================
// >>wayland
// =================================================================================================
#if SFTE_WAYLAND
/*
    Callback triggered by the emulator core to send bytes back to the shell (PTY).
*/
static inline void _sfte_wayland_write_cb(void *user_data, const char *data, size_t len) {
    sfte_wayland_app *app = (sfte_wayland_app *)user_data;
    if (app->pty_fd) (void)write(app->pty_fd, data, len);
}

/*
    Callback triggered by the emulator core to change the window title.
*/
static inline void _sfte_wayland_title_cb(void *user_data, const char *title) {
    sfte_wayland_app *app = (sfte_wayland_app *)user_data;
    if (app->xdg_toplevel && title) xdg_toplevel_set_title(app->xdg_toplevel, title);
}

static inline void _sfte_wayland_pty_spawn(sfte_wayland_app *app) {
#ifndef SFTE_NO_POSIX
    app->pty_pid = sfte_posix_pty_spawn(app->ctx, &app->pty_fd, app->width, app->height);
    SFTE_ASSERT(app->pty_pid != -1, "failed to forkpty");
#endif  // !SFTE_NO_POSIX
}

static inline void _sfte_wayland_pty_update(sfte_wayland_app *app) {
#ifndef SFTE_NO_POSIX
    sfte_posix_pty_resize(app->ctx, app->pty_fd, app->width, app->height);
#endif  // !SFTE_NO_POSIX
}

#if SFTE_FONT_ZOOM
static inline void _sfte_wayland_font_resize(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_zoom(ctx, arg->f);
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;
    _sfte_wayland_pty_update(app);
    app->needs_render = 1;
}

static inline void _sfte_wayland_font_reset(sfte_ctx *ctx, const sfte_arg *dummy) {
    (void)dummy;
    const sfte_arg arg = {.f = SFTE_FONT_DEFAULT_SIZE - ctx->font.cur_size};
    _sfte_wayland_font_resize(ctx, &arg);
}
#endif  // SFTE_FONT_ZOOM

#if SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_wayland_view_scroll(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_view_scroll(ctx, arg->i);
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;
    app->needs_render = 1;
}
#endif  // SFTE_TERM_SCROLLBACK_CAP

/*
    Allocates a SHared Memory (SHM) buffer that both the emulator and
    the Wayland compositor can access simultaneously.
*/
static inline void _sfte_wayland_create_buffer(sfte_wayland_app *app) {
    int32_t stride = app->width * 4;  // 4 bytes per pixel (ARGB8888)
    app->shm_size = stride * app->height;

    // memfd_create provides an anonymous file descriptor backed by RAM, not disk
    int32_t fd = memfd_create("sfte-buffer", MFD_CLOEXEC);
    SFTE_ASSERT(fd != -1, "failed to create memfd");
    SFTE_ASSERT(ftruncate(fd, app->shm_size) != -1, "failed to truncate memfd");

    app->shm_data = (uint32_t *)mmap(NULL, app->shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                     0);
    SFTE_ASSERT(app->shm_data != MAP_FAILED, "failed to mmap shm data");

    // NOTE:
    // If enabled, we allocate a secondary heap buffer for the emulator to draw into.
    // Once drawing is complete, we memcpy the damaged regions into the Wayland SHM
    // buffer to prevent the compositor from displaying half-drawn frames.
#if SFTE_TERM_DOUBLE_BUFFER
    if (app->back_buffer) SFTE_FREE(app->back_buffer);
    app->back_buffer = (uint32_t *)SFTE_MALLOC(app->shm_size);
    SFTE_ASSERT(app->back_buffer, "failed to allocate back buffer");
#endif  // SFTE_TERM_DOUBLE_BUFFER

    struct wl_shm_pool *pool = wl_shm_create_pool(app->shm, fd, app->shm_size);
    app->buffer = wl_shm_pool_create_buffer(pool, 0, app->width, app->height, stride,
                                            WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
}

static inline void _sfte_wayland_callback_listener_done(void *data, struct wl_callback *cb,
                                                        uint32_t time) {
    (void)time;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    wl_callback_destroy(cb);
    app->frame_pending = 0;
}

static const struct wl_callback_listener _sfte_wayland_callback_listener = {
    .done = _sfte_wayland_callback_listener_done,
};

#if SFTE_CLIPBOARD
static inline void _sfte_wayland_data_offer_offer(void *data, struct wl_data_offer *offer,
                                                  const char *mime_type) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (strcmp(mime_type, "text/plain;charset=utf-8") == 0 ||
        strcmp(mime_type, "text/plain") == 0) {
        wl_data_offer_accept(offer, app->serial, mime_type);
    }
}

static inline void _sfte_wayland_data_offer_source_actions(void *data, struct wl_data_offer *offer,
                                                           uint32_t actions) {
    (void)data, (void)offer, (void)actions;
}

static inline void _sfte_wayland_data_offer_action(void *data, struct wl_data_offer *offer,
                                                   uint32_t action) {
    (void)data, (void)offer, (void)action;
}

static const struct wl_data_offer_listener _sfte_wayland_data_offer_listener = {
    .offer = _sfte_wayland_data_offer_offer,
    .source_actions = _sfte_wayland_data_offer_source_actions,
    .action = _sfte_wayland_data_offer_action,
};

static inline void _sfte_wayland_data_device_data_offer(void *data, struct wl_data_device *device,
                                                        struct wl_data_offer *offer) {
    (void)device;
    wl_data_offer_add_listener(offer, &_sfte_wayland_data_offer_listener, data);
}

static inline void _sfte_wayland_data_device_enter(void *data, struct wl_data_device *device,
                                                   uint32_t serial, struct wl_surface *surface,
                                                   wl_fixed_t x, wl_fixed_t y,
                                                   struct wl_data_offer *offer) {
    (void)data, (void)device, (void)serial, (void)surface, (void)x, (void)y, (void)offer;
}

static inline void _sfte_wayland_data_device_leave(void *data, struct wl_data_device *device) {
    (void)data, (void)device;
}

static inline void _sfte_wayland_data_device_motion(void *data, struct wl_data_device *device,
                                                    uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void)data, (void)device, (void)time, (void)x, (void)y;
}

static inline void _sfte_wayland_data_device_drop(void *data, struct wl_data_device *device) {
    (void)data, (void)device;
}

static inline void _sfte_wayland_data_device_selection(void *data, struct wl_data_device *device,
                                                       struct wl_data_offer *offer) {
    (void)device;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (app->data_offer && app->data_offer != offer) wl_data_offer_destroy(app->data_offer);
    app->data_offer = offer;
}

static const struct wl_data_device_listener _sfte_wayland_data_device_listener = {
    .data_offer = _sfte_wayland_data_device_data_offer,
    .enter = _sfte_wayland_data_device_enter,
    .leave = _sfte_wayland_data_device_leave,
    .motion = _sfte_wayland_data_device_motion,
    .drop = _sfte_wayland_data_device_drop,
    .selection = _sfte_wayland_data_device_selection,
};

static inline void _sfte_wayland_data_source_target(void *data, struct wl_data_source *src,
                                                    const char *mime_type) {
    (void)data, (void)src, (void)mime_type;
}

static inline void _sfte_wayland_data_source_send(void *data, struct wl_data_source *src,
                                                  const char *mime_type, int32_t fd) {
    (void)src, (void)mime_type;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (app->selection_text) (void)write(fd, app->selection_text, strlen(app->selection_text));

    close(fd);
}

static inline void _sfte_wayland_data_source_cancelled(void *data, struct wl_data_source *src) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    wl_data_source_destroy(src);
    if (app->selection_text) {
        SFTE_FREE(app->selection_text);
        app->selection_text = NULL;
    }
    app->data_source = NULL;
}

static inline void _sfte_wayland_data_source_dnd_drop_performed(void *data,
                                                                struct wl_data_source *src) {
    (void)data, (void)src;
}

static inline void _sfte_wayland_data_source_dnd_finished(void *data, struct wl_data_source *src) {
    (void)data, (void)src;
}

static inline void _sfte_wayland_data_source_action(void *data, struct wl_data_source *src,
                                                    uint32_t action) {
    (void)data, (void)src, (void)action;
}

static const struct wl_data_source_listener _sfte_wayland_data_source_listener = {
    .target = _sfte_wayland_data_source_target,
    .send = _sfte_wayland_data_source_send,
    .cancelled = _sfte_wayland_data_source_cancelled,
    .dnd_drop_performed = _sfte_wayland_data_source_dnd_drop_performed,
    .dnd_finished = _sfte_wayland_data_source_dnd_finished,
    .action = _sfte_wayland_data_source_action,
};
#endif  // SFTE_CLIPBOARD

#if SFTE_INPUT_SELECTION
static inline void _sfte_wayland_pointer_enter(void *data, struct wl_pointer *pointer,
                                               uint32_t serial, struct wl_surface *surface,
                                               wl_fixed_t surface_x, wl_fixed_t surface_y) {
    (void)data, (void)pointer, (void)serial, (void)surface, (void)surface_x, (void)surface_y;
#if SFTE_INPUT_MOUSE
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_mouse_move(app->ctx, wl_fixed_to_int(surface_x), wl_fixed_to_int(surface_y));
    app->needs_render = 1;
#endif  // SFTE_INPUT_MOUSE
}

static inline void _sfte_wayland_pointer_leave(void *data, struct wl_pointer *pointer,
                                               uint32_t serial, struct wl_surface *surface) {
    (void)data, (void)pointer, (void)serial, (void)surface;
}

static inline void _sfte_wayland_pointer_motion(void *data, struct wl_pointer *pointer,
                                                uint32_t time, wl_fixed_t surface_x,
                                                wl_fixed_t surface_y) {
    (void)data, (void)pointer, (void)time, (void)surface_x, (void)surface_y;
#if SFTE_INPUT_MOUSE
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    sfte_mouse_move(app->ctx, wl_fixed_to_int(surface_x), wl_fixed_to_int(surface_y));
    app->needs_render = 1;
#endif  // SFTE_INPUT_MOUSE
}

static inline void _sfte_wayland_pointer_button(void *data, struct wl_pointer *pointer,
                                                uint32_t serial, uint32_t time, uint32_t button,
                                                uint32_t state) {
    (void)data, (void)pointer, (void)serial, (void)time, (void)button, (void)state;
#if SFTE_INPUT_MOUSE
    if (button != 0x110 /* Wayland LMB */) return;

    sfte_wayland_app *app = (sfte_wayland_app *)data;

#if SFTE_CLIPBOARD
    app->serial = serial;
#endif  // SFTE_CLIPBOARD

    sfte_mouse_click(app->ctx, SFTE_MOUSE_BUTTON_LEFT, state == WL_POINTER_BUTTON_STATE_PRESSED,
                     app->ctx->term.mouse_hover_col * app->ctx->font.cell_width + SFTE_WINDOW_PAD_X,
                     app->ctx->term.mouse_hover_row * app->ctx->font.cell_height +
                         SFTE_WINDOW_PAD_Y);
    app->needs_render = 1;

#if SFTE_CLIPBOARD && SFTE_INPUT_SELECTION
    if (state == WL_POINTER_BUTTON_STATE_RELEASED) _sfte_wayland_clipboard_copy(app->ctx, NULL);
#endif  // SFTE_CLIPBOARD && SFTE_INPUT_SELECTION
#endif  // SFTE_INPUT_MOUSE
}

static inline void _sfte_wayland_pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time,
                                              uint32_t axis, wl_fixed_t value) {
    (void)data, (void)pointer, (void)time, (void)axis, (void)value;
#if SFTE_INPUT_MOUSE
    if (axis != WL_POINTER_AXIS_VERTICAL_SCROLL) return;

    sfte_wayland_app *app = (sfte_wayland_app *)data;
    int8_t dir = (wl_fixed_to_double(value) < 0) ? 1 : -1;
    sfte_mouse_scroll(
        app->ctx, dir,
        app->ctx->term.mouse_hover_col * app->ctx->font.cell_width + SFTE_WINDOW_PAD_X,
        app->ctx->term.mouse_hover_row * app->ctx->font.cell_height + SFTE_WINDOW_PAD_Y);
    app->needs_render = 1;
#endif  // SFTE_INPUT_MOUSE
}

static inline void _sfte_wayland_pointer_frame(void *data, struct wl_pointer *pointer) {
    (void)data, (void)pointer;
}

static inline void _sfte_wayland_pointer_axis_source(void *data, struct wl_pointer *pointer,
                                                     uint32_t axis_source) {
    (void)data, (void)pointer, (void)axis_source;
}

static inline void _sfte_wayland_pointer_axis_stop(void *data, struct wl_pointer *pointer,
                                                   uint32_t time, uint32_t axis) {
    (void)data, (void)pointer, (void)time, (void)axis;
}

static inline void _sfte_wayland_pointer_axis_discrete(void *data, struct wl_pointer *pointer,
                                                       uint32_t axis, int32_t discrete) {
    (void)data, (void)pointer, (void)axis, (void)discrete;
}

static const struct wl_pointer_listener _sfte_wayland_pointer_listener = {
    .enter = _sfte_wayland_pointer_enter,
    .leave = _sfte_wayland_pointer_leave,
    .motion = _sfte_wayland_pointer_motion,
    .button = _sfte_wayland_pointer_button,
    .axis = _sfte_wayland_pointer_axis,
    .frame = _sfte_wayland_pointer_frame,
    .axis_source = _sfte_wayland_pointer_axis_source,
    .axis_stop = _sfte_wayland_pointer_axis_stop,
    .axis_discrete = _sfte_wayland_pointer_axis_discrete,
};
#endif  // SFTE_INPUT_SELECTION

#if SFTE_INPUT_HYPERLINKS
static inline void _sfte_wayland_open_link_cb(void *user_data, const char *uri) {
    (void)user_data;
    if (!uri) return;

    pid_t pid = fork();
    if (pid == 0) {
        if (fork() == 0) {
            (void)freopen("/dev/null", "w", stdout);
            (void)freopen("/dev/null", "w", stderr);
            execlp("xdg-open", "xdg-open", uri, NULL);
            exit(1);
        }
        exit(0);
    } else if (pid > 0)
        waitpid(pid, NULL, 0);
}
#endif  // SFTE_INPUT_HYPERLINKS

static inline void _sfte_wayland_keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                                                 uint32_t format, int32_t fd, uint32_t size) {
    (void)keyboard;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    SFTE_ASSERT(format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, "unsupported keymap format");

    char *map_str = (char *)mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    SFTE_ASSERT(map_str != MAP_FAILED, "failed to mmap keyboard");

    if (app->xkb_keymap) xkb_keymap_unref(app->xkb_keymap);
    if (app->xkb_state) xkb_state_unref(app->xkb_state);

    app->xkb_keymap = xkb_keymap_new_from_string(
        app->xkb_context, map_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    app->xkb_state = xkb_state_new(app->xkb_keymap);

    _SFTE_INFO(app->ctx, KEYMAP_LOADED);
    munmap(map_str, size);
    close(fd);  // close the fd to avoid leak
}

static inline void _sfte_wayland_keyboard_enter(void *data, struct wl_keyboard *keyboard,
                                                uint32_t serial, struct wl_surface *surface,
                                                struct wl_array *keys) {
    (void)data, (void)keyboard, (void)serial, (void)surface, (void)keys;
}

static inline void _sfte_wayland_keyboard_leave(void *data, struct wl_keyboard *keyboard,
                                                uint32_t serial, struct wl_surface *surface) {
    (void)data, (void)keyboard, (void)serial, (void)surface;
}

static inline void _sfte_wayland_keyboard_key(void *data, struct wl_keyboard *keyboard,
                                              uint32_t serial, uint32_t time, uint32_t key,
                                              uint32_t state) {
    (void)data, (void)keyboard, (void)time;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
#if SFTE_CLIPBOARD
    app->serial = serial;
#endif  // SFTE_CLIPBOARD

    if (state == WL_KEYBOARD_KEY_STATE_RELEASED && key == app->repeating_key) {
        struct itimerspec its = {0};
        timerfd_settime(app->repeat_timer_fd, 0, &its, NULL);
        app->repeating_key = 0;
        return;
    }

    if (state != WL_KEYBOARD_KEY_STATE_PRESSED || !app->xkb_state) return;

// Clear selection on key press
#if SFTE_INPUT_SELECTION
    if (app->ctx->term.mouse_sel_active) {
        app->ctx->term.mouse_sel_active = 0;
        _sfte_grid_dirty_range(app->ctx, 0, app->ctx->term.cols * app->ctx->term.rows);
        app->needs_render = 1;
    }
#endif  // SFTE_INPUT_SELECTION

    if (app->repeat_rate > 0 && app->repeating_key != key) {
        struct itimerspec its;
        its.it_value.tv_sec = app->repeat_delay / 1000;
        its.it_value.tv_nsec = (app->repeat_delay % 1000) * 1000000;
        its.it_interval.tv_sec = 0;
        if (app->repeat_rate > 0)
            its.it_interval.tv_nsec = 1000000000 / app->repeat_rate;
        else
            its.it_interval.tv_nsec = 0;

        timerfd_settime(app->repeat_timer_fd, 0, &its, NULL);
        app->repeating_key = key;
    }

    xkb_keycode_t keycode = key + 8;  // WARN: evdev codes are offset by 8 from xkb keycodes
    xkb_keysym_t sym = xkb_state_key_get_one_sym(app->xkb_state, keycode);

    uint8_t ctrl = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_CTRL,
                                                XKB_STATE_MODS_EFFECTIVE);
    uint8_t alt = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_ALT,
                                               XKB_STATE_MODS_EFFECTIVE);
    uint8_t shift = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_SHIFT,
                                                 XKB_STATE_MODS_EFFECTIVE);
    uint8_t super = xkb_state_mod_name_is_active(app->xkb_state, XKB_MOD_NAME_LOGO,
                                                 XKB_STATE_MODS_EFFECTIVE);

    uint8_t active_mods = SFTE_MOD_NONE;
    if (ctrl) active_mods |= SFTE_MOD_CTRL;
    if (alt) active_mods |= SFTE_MOD_ALT;
    if (shift) active_mods |= SFTE_MOD_SHIFT;
    if (super) active_mods |= SFTE_MOD_SUPER;

    for (size_t i = 0; i < _SFTE_ARRAY_LEN(_sfte_shortcuts); ++i)
        if ((xkb_keysym_t)_sfte_shortcuts[i].keysym == sym &&
            _sfte_shortcuts[i].mod_mask == active_mods) {
            _sfte_shortcuts[i].func(app->ctx, &_sfte_shortcuts[i].arg);
            return;
        }

    sfte_xkb_process_key(app->ctx, app->xkb_state, keycode);
}

static inline void _sfte_wayland_keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                                                    uint32_t serial, uint32_t mods_depressed,
                                                    uint32_t mods_latched, uint32_t mods_locked,
                                                    uint32_t group) {
    (void)keyboard, (void)serial;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    if (!app->xkb_state) return;

    xkb_state_update_mask(app->xkb_state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static inline void _sfte_wayland_keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
                                                      int32_t rate, int32_t delay) {
    (void)keyboard;
    sfte_wayland_app *app = (sfte_wayland_app *)data;

    app->repeat_rate = rate;
    app->repeat_delay = delay;
}

static const struct wl_keyboard_listener _sfte_wayland_keyboard_listener = {
    .keymap = _sfte_wayland_keyboard_keymap,
    .enter = _sfte_wayland_keyboard_enter,
    .leave = _sfte_wayland_keyboard_leave,
    .key = _sfte_wayland_keyboard_key,
    .modifiers = _sfte_wayland_keyboard_modifiers,
    .repeat_info = _sfte_wayland_keyboard_repeat_info,
};

static inline void _sfte_wayland_seat_capabilities(void *data, struct wl_seat *seat,
                                                   uint32_t capabilities) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    // if seat has a keyboard and we haven't grabbed it yet
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !app->keyboard) {
        app->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(app->keyboard, &_sfte_wayland_keyboard_listener, app);
    }
    // if seat lost keyboard and we still hold the ptr
    else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && app->keyboard) {
        wl_keyboard_release(app->keyboard);
        app->keyboard = NULL;
    }

#if SFTE_INPUT_SELECTION
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !app->pointer) {
        app->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(app->pointer, &_sfte_wayland_pointer_listener, app);

    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && app->pointer) {
        wl_pointer_release(app->pointer);
        app->pointer = NULL;
    }
#endif  // SFTE_INPUT_SELECTION

#if SFTE_CLIPBOARD
    if (app->data_device_manager && !app->data_device) {
        app->data_device = (struct wl_data_device *)wl_data_device_manager_get_data_device(
            app->data_device_manager, seat);
        wl_data_device_add_listener(app->data_device, &_sfte_wayland_data_device_listener, app);
    }
#endif  // SFTE_CLIPBOARD
}

static inline void _sfte_wayland_seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data, (void)seat, (void)name;
}

static const struct wl_seat_listener _sfte_wayland_seat_listener = {
    .capabilities = _sfte_wayland_seat_capabilities,
    .name = _sfte_wayland_seat_name,
};

static inline void _sfte_wayland_xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base,
                                                  uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);  // compositor pinged, pong back with same serial
}

static const struct xdg_wm_base_listener _sfte_wayland_xdg_wm_base_listener = {
    .ping = _sfte_wayland_xdg_wm_base_ping,
};

static inline void _sfte_wayland_registry_global(void *data, struct wl_registry *registry,
                                                 uint32_t name, const char *interface,
                                                 uint32_t version) {
    (void)version;
    sfte_wayland_app *app = (sfte_wayland_app *)data;

    if (strcmp(interface, wl_compositor_interface.name) == 0)
        app->compositor = (struct wl_compositor *)wl_registry_bind(
            registry, name, &wl_compositor_interface, 4 /* wl compositor version */);
    else if (strcmp(interface, wl_shm_interface.name) == 0)
        app->shm = (struct wl_shm *)wl_registry_bind(registry, name, &wl_shm_interface,
                                                     1 /* wl shm version */);
    else if (strcmp(interface, wl_seat_interface.name) == 0) {
        app->seat = (struct wl_seat *)wl_registry_bind(registry, name, &wl_seat_interface,
                                                       7 /* wl seat version */);
        wl_seat_add_listener(app->seat, &_sfte_wayland_seat_listener, app);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        app->xdg_wm_base = (struct xdg_wm_base *)wl_registry_bind(
            registry, name, &xdg_wm_base_interface, 1 /* xdg wm base version */);
        xdg_wm_base_add_listener(app->xdg_wm_base, &_sfte_wayland_xdg_wm_base_listener, app);
    }
#if SFTE_CLIPBOARD
    else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        app->data_device_manager = (struct wl_data_device_manager *)wl_registry_bind(
            registry, name, &wl_data_device_manager_interface, 3 /* data device manager version */);
    }
#endif  // SFTE_CLIPBOARD
}

static inline void _sfte_wayland_registry_global_remove(void *data, struct wl_registry *registry,
                                                        uint32_t name) {
    (void)data, (void)registry, (void)name;
}

static const struct wl_registry_listener _sfte_wayland_registry_listener = {
    .global = _sfte_wayland_registry_global,
    .global_remove = _sfte_wayland_registry_global_remove,
};

static inline void _sfte_wayland_xdg_surface_configure(void *data, struct xdg_surface *xdg_surface,
                                                       uint32_t serial) {
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    xdg_surface_ack_configure(xdg_surface, serial);

    if (app->pending_width > 0 && app->pending_height > 0) {
        app->width = app->pending_width;
        app->height = app->pending_height;
        app->pending_width = 0;
        app->pending_height = 0;

        sfte_resize(app->ctx, app->width, app->height);
        _sfte_wayland_pty_update(app);
    }

    // resize recalc
    size_t needed_size = app->width * app->height * 4;
    if (app->shm_size != needed_size) {
        if (app->buffer) wl_buffer_destroy(app->buffer);
        if (app->shm_data) munmap(app->shm_data, app->shm_size);

        _sfte_wayland_create_buffer(app);
    }

    app->needs_render = 1;
}

static const struct xdg_surface_listener _sfte_wayland_xdg_surface_listener = {
    .configure = _sfte_wayland_xdg_surface_configure,
};

static inline void _sfte_wayland_xdg_toplevel_configure(void *data,
                                                        struct xdg_toplevel *xdg_toplevel,
                                                        int32_t width, int32_t height,
                                                        struct wl_array *states) {
    (void)xdg_toplevel, (void)states;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
#if SFTE_TERM_FOCUS
    uint32_t *state;
    uint8_t focused = 0;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) {
            focused = 1;
            break;
        }
    }

    sfte_set_focus(app->ctx, focused);
    app->needs_render = 1;
#endif  // SFTE_TERM_FOCUS

    if (width <= 0 || height <= 0) return;

    app->pending_width = width;
    app->pending_height = height;
}

static inline void _sfte_wayland_xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    (void)xdg_toplevel;
    sfte_wayland_app *app = (sfte_wayland_app *)data;
    app->running = 0;
}

static const struct xdg_toplevel_listener _sfte_wayland_xdg_toplevel_listener = {
    .configure = _sfte_wayland_xdg_toplevel_configure,
    .close = _sfte_wayland_xdg_toplevel_close,
};

/*
    Initializes the Wayland connection and binds global registry interfaces.
*/
static inline void _sfte_wayland_load(sfte_wayland_app *app) {
    app->xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    SFTE_ASSERT(app->xkb_context, "failed to create xkb context");

    app->display = wl_display_connect(NULL);
    SFTE_ASSERT(app->display, "failed to connect to Wayland display\n");

    app->registry = wl_display_get_registry(app->display);
    wl_registry_add_listener(app->registry, &_sfte_wayland_registry_listener, app);

    // Initial roundtrip to let the registry listener discover compositor/shm/seat
    wl_display_roundtrip(app->display);
    SFTE_ASSERT(app->compositor, "failed to initialize compositor\n");
    SFTE_ASSERT(app->shm, "compositor missing required interfaces\n");
    SFTE_ASSERT(app->xdg_wm_base, "failed to bind xdg_wm_base\n");

    app->surface = wl_compositor_create_surface(app->compositor);
    app->xdg_surface = xdg_wm_base_get_xdg_surface(app->xdg_wm_base, app->surface);
    xdg_surface_add_listener(app->xdg_surface, &_sfte_wayland_xdg_surface_listener, app);

    app->xdg_toplevel = xdg_surface_get_toplevel(app->xdg_surface);
    xdg_toplevel_add_listener(app->xdg_toplevel, &_sfte_wayland_xdg_toplevel_listener, app);
    xdg_toplevel_set_title(app->xdg_toplevel, "sfte");
    xdg_toplevel_set_app_id(app->xdg_toplevel, "sfte");

    wl_surface_commit(app->surface);
    wl_display_roundtrip(app->display);
    _SFTE_INFO(app->ctx, WAYLAND_REGISTRY_BOUND);
}

/*
    Cleans up all Wayland objects and memory mappings.
*/
static inline void _sfte_wayland_unload(sfte_wayland_app *app) {
#if SFTE_TERM_DOUBLE_BUFFER
    SFTE_FREE(app->back_buffer);
#endif  // SFTE_TERM_DOUBLE_BUFFER

    if (app->buffer) wl_buffer_destroy(app->buffer);
    if (app->shm_data) munmap(app->shm_data, app->shm_size);
    if (app->xdg_toplevel) xdg_toplevel_destroy(app->xdg_toplevel);
    if (app->xdg_surface) xdg_surface_destroy(app->xdg_surface);
    if (app->surface) wl_surface_destroy(app->surface);
    if (app->xdg_wm_base) xdg_wm_base_destroy(app->xdg_wm_base);

    if (app->keyboard) wl_keyboard_release(app->keyboard);
#if SFTE_INPUT_SELECTION
    if (app->pointer) wl_pointer_release(app->pointer);
#endif  // SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD
    if (app->data_offer) wl_data_offer_destroy(app->data_offer);
    if (app->data_source) wl_data_source_destroy(app->data_source);
    if (app->data_device) wl_data_device_release(app->data_device);
    if (app->data_device_manager) wl_data_device_manager_destroy(app->data_device_manager);
    if (app->selection_text) SFTE_FREE(app->selection_text);
#endif  // SFTE_CLIPBOARD

    if (app->seat) wl_seat_release(app->seat);

    if (app->shm) wl_shm_destroy(app->shm);
    if (app->compositor) wl_compositor_destroy(app->compositor);

    wl_registry_destroy(app->registry);
    wl_display_disconnect(app->display);

    xkb_state_unref(app->xkb_state);
    xkb_keymap_unref(app->xkb_keymap);
    xkb_context_unref(app->xkb_context);
}

#if SFTE_CLIPBOARD
#if SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD_OSC52
static inline void _sfte_wayland_osc52_clipboard_cb(void *user_data, char target,
                                                    const char *data) {
    (void)target;  // TODO: wl primary selection protocol
    if (target != 'c') return;
    sfte_wayland_app *app = (sfte_wayland_app *)user_data;

    if (app->data_source) {
        wl_data_source_destroy(app->data_source);
        app->data_source = NULL;
    }
    if (app->selection_text) {
        SFTE_FREE(app->selection_text);
        app->selection_text = NULL;
    }

    if (!data || !app->data_device_manager || !app->data_device) return;

    size_t len = strlen(data);
    app->selection_text = (char *)SFTE_MALLOC(len + 1);
    memcpy(app->selection_text, data, len + 1);

    app->data_source = wl_data_device_manager_create_data_source(app->data_device_manager);
    wl_data_source_add_listener(app->data_source, &_sfte_wayland_data_source_listener, app);
    wl_data_source_offer(app->data_source, "text/plain;charset=utf-8");
    wl_data_source_offer(app->data_source, "text/plain");

    wl_data_device_set_selection(app->data_device, app->data_source, app->serial);
}
#endif  // SFTE_CLIPBOARD_OSC52

static inline void _sfte_wayland_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;

    if (app->data_source) {
        wl_data_source_destroy(app->data_source);
        app->data_source = NULL;
    }
    if (app->selection_text) {
        SFTE_FREE(app->selection_text);
        app->selection_text = NULL;
    }

    if (!app->ctx->term.mouse_sel_active || !app->data_device_manager || !app->data_device) return;

    size_t needed_bytes = sfte_get_selection(app->ctx, NULL, 0);
    if (needed_bytes == 0) {
        _SFTE_INFO(ctx, CLIPBOARD_EMPTY);
        return;
    }

    app->selection_text = (char *)SFTE_MALLOC(needed_bytes);
    sfte_get_selection(app->ctx, app->selection_text, needed_bytes);
    if (!app->selection_text) return;

    app->data_source = wl_data_device_manager_create_data_source(app->data_device_manager);
    wl_data_source_add_listener(app->data_source, &_sfte_wayland_data_source_listener, app);
    wl_data_source_offer(app->data_source, "text/plain;charset=utf-8");
    wl_data_source_offer(app->data_source, "text/plain");
    wl_data_device_set_selection(app->data_device, app->data_source, app->serial);
}
#endif  // SFTE_INPUT_SELECTION

static inline void _sfte_wayland_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    sfte_wayland_app *app = (sfte_wayland_app *)ctx->user_data;

    if (!app->data_offer) return;

    int fds[2];
    if (pipe(fds) == 0) {
        wl_data_offer_receive(app->data_offer, "text/plain;charset=utf-8", fds[1]);
        close(fds[1]);

        wl_display_roundtrip(app->display);

        sfte_input_paste_begin(app->ctx);

        char buf[SFTE_CLIPBOARD_BUF_SIZE];
        ssize_t n;

        while ((n = read(fds[0], buf, sizeof(buf))) > 0) sfte_input_text(app->ctx, buf, n);

        sfte_input_paste_end(app->ctx);

        close(fds[0]);
    }
}
#endif  // SFTE_CLIPBOARD

/*
    The primary event loop for the terminal emulator using Wayland.
    Uses `poll` to simultaneously wait for Wayland compositor events,
    shell output events (PTY data), and timer expirations (on cursor blink, key repeat, trail).
*/
static inline void _sfte_wayland_loop(sfte_wayland_app *app) {
    signal(SIGPIPE, SIG_IGN);
    setlocale(LC_ALL, "");

    app->repeat_timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    int wl_fd = wl_display_get_fd(app->display);

    while (app->running) {
        // Dispatch pending Wayland events before polling to prevent deadlock
        wl_display_dispatch_pending(app->display);
        wl_display_flush(app->display);
        struct pollfd fds[] = {{.fd = wl_fd, .events = POLLIN},
                               {.fd = app->pty_fd, .events = POLLIN},
                               {.fd = app->repeat_timer_fd, .events = POLLIN}};

        int32_t timeout = sfte_get_timeout_ms(app->ctx);
        if (app->needs_render) timeout = 0;  // Don't sleep if we already know we need to draw

        if (poll(fds, _SFTE_ARRAY_LEN(fds), timeout) == -1) break;

        if (sfte_tick(app->ctx)) app->needs_render = 1;

        // Handle incoming Wayland events (keys, resizes)
        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP))
            if (wl_display_dispatch(app->display) == -1) app->running = 0;

        // Handle incoming text from the shell
        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP)) {
            uint8_t buf[SFTE_TERM_PTY_BUF_SIZE];
            uint8_t did_read = 0;

            while (1) {
                ssize_t n = read(app->pty_fd, buf, SFTE_TERM_PTY_BUF_SIZE);

                if (n > 0) {
                    sfte_parse(app->ctx, buf, n);
                    did_read = 1;
                } else if (n == 0) {
                    app->running = 0;  // Shell exited
                    break;
                } else {  // n < 0
                    // EAGAIN/EWOULDBLOCK means buffer is empty
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                    // EINTR means its interrupted by signal
                    else if (errno == EINTR)
                        continue;
                    else {
                        app->running = 0;
                        break;
                    }
                }
            }

            if (did_read) app->needs_render = 1;
        }

        // Handle key repeat timer
        if (fds[2].revents & POLLIN) {
            uint64_t expirations;
            if (read(app->repeat_timer_fd, &expirations, sizeof(expirations)) == 0 ||
                app->repeating_key == 0)
                continue;
            // Simulate a key press to autorepeat
            _sfte_wayland_keyboard_key(app, app->keyboard, 0, 0, app->repeating_key,
                                       WL_KEYBOARD_KEY_STATE_PRESSED);
        }

        // Dispatch render pass
        if (app->needs_render && !app->frame_pending) {
            sfte_damage_rect dmg = {0};

            uint32_t *target_pxs = app->shm_data;
#if SFTE_TERM_DOUBLE_BUFFER
            target_pxs = app->back_buffer;
#endif  // SFTE_TERM_DOUBLE_BUFFER

            sfte_render(app->ctx, target_pxs, app->width, app->height, &dmg);

            if (dmg.w > 0 && dmg.h > 0) {
#if SFTE_TERM_DOUBLE_BUFFER
                if (dmg.w == app->width)
                    memcpy(&app->shm_data[dmg.y * app->width],
                           &app->back_buffer[dmg.y * app->width], dmg.w * dmg.h * sizeof(uint32_t));
                else
                    for (int32_t y = dmg.y; y < dmg.y + dmg.h; ++y)
                        memcpy(&app->shm_data[y * app->width + dmg.x],
                               &app->back_buffer[y * app->width + dmg.x], dmg.w * sizeof(uint32_t));
#endif  // SFTE_TERM_DOUBLE_BUFFER

                wl_surface_damage_buffer(app->surface, dmg.x, dmg.y, dmg.w, dmg.h);
                wl_surface_attach(app->surface, app->buffer, 0, 0);
                struct wl_callback *cb = wl_surface_frame(app->surface);
                wl_callback_add_listener(cb, &_sfte_wayland_callback_listener, app);
                app->frame_pending = 1;
                wl_surface_commit(app->surface);
            }

            app->needs_render = 0;
        }
    }
}
#endif  // SFTE_WAYLAND

// =================================================================================================
// >>win32
// =================================================================================================
#if SFTE_WIN32
/*
    Converts a null-terminated (or explicitly sized) UTF-16 string into a heap UTF-8 string.
    The caller owns the returned buffer and must free it with SFTE_FREE.
*/
static inline char *_sfte_win32_wide_to_utf8(const WCHAR *wide, int wide_len) {
    if (!wide) return NULL;
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, wide_len, NULL, 0, NULL, NULL);
    if (len <= 0) return NULL;
    char *out = (char *)SFTE_MALLOC((size_t)len + 1);
    if (!out) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, wide, wide_len, out, len, NULL, NULL);
    out[len] = '\0';
    return out;
}

/*
    Converts a null-terminated UTF-8 string into a heap UTF-16 string.
    The caller owns the returned buffer and must free it with SFTE_FREE.
*/
static inline WCHAR *_sfte_win32_utf8_to_wide(const char *utf8) {
    if (!utf8) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (len <= 0) return NULL;
    WCHAR *out = (WCHAR *)SFTE_MALLOC((size_t)len * sizeof(WCHAR));
    if (!out) return NULL;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, len);
    return out;
}

static inline char *_sfte_win32_dup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)SFTE_MALLOC(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

/*
    Resolves the shell command line to launch, in priority order:
        SFTE_SHELL -> ash.exe -> powershell.exe -> COMSPEC -> cmd.exe
*/
static inline char *_sfte_win32_resolve_shell(void) {
    const char *env = getenv("SFTE_SHELL");
    if (env && *env) return _sfte_win32_dup(env);

    static const char *candidates[] = {"ash.exe", "powershell.exe"};
    for (size_t i = 0; i < _SFTE_ARRAY_LEN(candidates); ++i) {
        char found[MAX_PATH];
        DWORD n = SearchPathA(NULL, candidates[i], NULL, MAX_PATH, found, NULL);
        if (n > 0 && n < MAX_PATH) return _sfte_win32_dup(found);
    }

    env = getenv("COMSPEC");
    if (env && *env) return _sfte_win32_dup(env);

    return _sfte_win32_dup("cmd.exe");
}

static inline void _sfte_win32_write_cb(void *user_data, const char *data, size_t len) {
    sfte_win32_app *app = (sfte_win32_app *)user_data;
    if (!app->pty_in_write || !data || !len) return;
    DWORD written = 0;
    (void)WriteFile(app->pty_in_write, data, (DWORD)len, &written, NULL);
}

static inline void _sfte_win32_title_cb(void *user_data, const char *title) {
    sfte_win32_app *app = (sfte_win32_app *)user_data;
    if (!app->hwnd || !title) return;
    WCHAR *wide = _sfte_win32_utf8_to_wide(title);
    if (wide) {
        SetWindowTextW(app->hwnd, wide);
        SFTE_FREE(wide);
    }
}

/*
    Creates the ConPTY pseudo console and spawns the shell process attached to it.
*/
static inline void _sfte_win32_pty_spawn(sfte_win32_app *app) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE in_read = NULL, in_write = NULL, out_read = NULL, out_write = NULL;

    if (!CreatePipe(&in_read, &in_write, &sa, 0) ||
        !CreatePipe(&out_read, &out_write, &sa, 0)) {
        _SFTE_ERROR(app->ctx, PTY_FORK_FAIL, (int)GetLastError());
        return;
    }

    COORD size = {(SHORT)app->ctx->term.cols, (SHORT)app->ctx->term.rows};
    HRESULT hr = CreatePseudoConsole(size, in_read, out_write, 0, &app->hpc);
    if (FAILED(hr)) {
        _SFTE_ERROR(app->ctx, PTY_FORK_FAIL, (int)GetLastError());
        CloseHandle(in_read);
        CloseHandle(in_write);
        CloseHandle(out_read);
        CloseHandle(out_write);
        return;
    }
    CloseHandle(in_read);
    CloseHandle(out_write);

    SIZE_T attr_size = 0;
    (void)InitializeProcThreadAttributeList(NULL, 1, 0, &attr_size);
    LPPROC_THREAD_ATTRIBUTE_LIST attrs = (LPPROC_THREAD_ATTRIBUTE_LIST)SFTE_MALLOC(attr_size);
    if (!attrs || !InitializeProcThreadAttributeList(attrs, 1, 0, &attr_size) ||
        !UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, app->hpc,
                                   sizeof(HPCON), NULL, NULL)) {
        _SFTE_ERROR(app->ctx, PTY_FORK_FAIL, (int)GetLastError());
        if (attrs) SFTE_FREE(attrs);
        CloseHandle(in_write);
        CloseHandle(out_read);
        ClosePseudoConsole(app->hpc);
        app->hpc = NULL;
        return;
    }

    STARTUPINFOEXW si = {0};
    si.StartupInfo.cb = sizeof(si);
    si.lpAttributeList = attrs;
    // Without this, the child inherits our (possibly redirected) std handles instead of the
    // pseudoconsole. The system reconnects the child's std handles to the pseudoconsole.
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = NULL;
    si.StartupInfo.hStdOutput = NULL;
    si.StartupInfo.hStdError = NULL;

    SetEnvironmentVariableA("TERM", SFTE_TERM_ENV);

    WCHAR *cmd = _sfte_win32_utf8_to_wide(app->shell_cmdline ? app->shell_cmdline : "cmd.exe");
    BOOL ok = FALSE;
    if (cmd) {
        ok = CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL,
                            &si.StartupInfo, &app->proc);
        SFTE_FREE(cmd);
    }
    DeleteProcThreadAttributeList(attrs);
    SFTE_FREE(attrs);

    if (!ok) {
        _SFTE_ERROR(app->ctx, PTY_FORK_FAIL, (int)GetLastError());
        CloseHandle(in_write);
        CloseHandle(out_read);
        return;
    }

    CloseHandle(app->proc.hThread);
    app->pty_in_write = in_write;
    app->pty_out_read = out_read;
    app->pty_proc = app->proc.hProcess;
    app->pty_pid = app->proc.dwProcessId;

    _SFTE_INFO(app->ctx, PTY_SPAWN);
}

static inline void _sfte_win32_pty_update(sfte_win32_app *app) {
    if (!app->hpc) return;
    COORD size = {(SHORT)app->ctx->term.cols, (SHORT)app->ctx->term.rows};
    ResizePseudoConsole(app->hpc, size);
}

static inline void _sfte_win32_destroy_backbuffer(sfte_win32_app *app) {
    if (app->mem_dc && app->old_dib) SelectObject(app->mem_dc, app->old_dib);
    if (app->dib) DeleteObject(app->dib);
    if (app->mem_dc) DeleteDC(app->mem_dc);
    app->dib = NULL;
    app->old_dib = NULL;
    app->mem_dc = NULL;
    app->pixels = NULL;
}

/*
    Allocates a top-down 32bpp DIB section. Its BGRA layout matches the emulator's ARGB buffer.
*/
static inline void _sfte_win32_create_backbuffer(sfte_win32_app *app) {
    _sfte_win32_destroy_backbuffer(app);
    if (app->width <= 0 || app->height <= 0) return;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = app->width;
    bmi.bmiHeader.biHeight = -app->height;  // negative -> top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screen = GetDC(NULL);
    app->mem_dc = CreateCompatibleDC(screen);
    app->dib = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, (void **)&app->pixels, NULL, 0);
    ReleaseDC(NULL, screen);

    if (app->mem_dc && app->dib) app->old_dib = (HBITMAP)SelectObject(app->mem_dc, app->dib);
    else _sfte_win32_destroy_backbuffer(app);
}

static inline void _sfte_win32_setup_dpi(sfte_win32_app *app) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HDC screen = GetDC(NULL);
    app->dpi = (UINT)GetDeviceCaps(screen, LOGPIXELSY);
    ReleaseDC(NULL, screen);
    if (app->dpi == 0) app->dpi = 96;
    app->font_scale = (float)app->dpi / 96.0f;
}

static inline void _sfte_win32_apply_dpi(sfte_win32_app *app, UINT new_dpi) {
    if (new_dpi == 0) new_dpi = 96;
    app->dpi = new_dpi;
    app->font_scale = (float)new_dpi / 96.0f;
#if SFTE_FONT_ZOOM
    float target = SFTE_FONT_DEFAULT_SIZE * app->font_scale;
    float delta = target - app->ctx->font.cur_size;
    if (delta != 0.0f) sfte_zoom(app->ctx, delta);
#endif  // SFTE_FONT_ZOOM
    app->needs_render = 1;
}

/*
    Renders the grid into the DIB and blits only the damaged region to the window.
*/
static inline void _sfte_win32_render(sfte_win32_app *app) {
    if (!app->pixels || app->width <= 0 || app->height <= 0) return;

    sfte_damage_rect dmg = {0};
    sfte_render(app->ctx, app->pixels, app->width, app->height, &dmg);

    if (dmg.w > 0 && dmg.h > 0) {
        HDC dc = GetDC(app->hwnd);
        BitBlt(dc, dmg.x, dmg.y, dmg.w, dmg.h, app->mem_dc, dmg.x, dmg.y, SRCCOPY);
        ReleaseDC(app->hwnd, dc);
    }
    app->needs_render = 0;
}

static inline uint32_t _sfte_win32_mods(void) {
    uint32_t mods = SFTE_MOD_NONE;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= SFTE_MOD_CTRL;
    if (GetKeyState(VK_MENU) & 0x8000) mods |= SFTE_MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= SFTE_MOD_SHIFT;
    if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000))
        mods |= SFTE_MOD_SUPER;
    return mods;
}

static inline sfte_key _sfte_win32_key_from_vk(WPARAM vk) {
    switch (vk) {
    case VK_TAB: return SFTE_KEY_TAB;
    case VK_RETURN: return SFTE_KEY_ENTER;
    case VK_ESCAPE: return SFTE_KEY_ESCAPE;
    case VK_BACK: return SFTE_KEY_BACKSPACE;
    case VK_UP: return SFTE_KEY_UP;
    case VK_DOWN: return SFTE_KEY_DOWN;
    case VK_LEFT: return SFTE_KEY_LEFT;
    case VK_RIGHT: return SFTE_KEY_RIGHT;
    case VK_HOME: return SFTE_KEY_HOME;
    case VK_END: return SFTE_KEY_END;
    case VK_PRIOR: return SFTE_KEY_PAGE_UP;
    case VK_NEXT: return SFTE_KEY_PAGE_DOWN;
    case VK_INSERT: return SFTE_KEY_INSERT;
    case VK_DELETE: return SFTE_KEY_DELETE;
    case VK_F1: return SFTE_KEY_F1;
    case VK_F2: return SFTE_KEY_F2;
    case VK_F3: return SFTE_KEY_F3;
    case VK_F4: return SFTE_KEY_F4;
    case VK_F5: return SFTE_KEY_F5;
    case VK_F6: return SFTE_KEY_F6;
    case VK_F7: return SFTE_KEY_F7;
    case VK_F8: return SFTE_KEY_F8;
    case VK_F9: return SFTE_KEY_F9;
    case VK_F10: return SFTE_KEY_F10;
    case VK_F11: return SFTE_KEY_F11;
    case VK_F12: return SFTE_KEY_F12;
    default: return SFTE_KEY_NONE;
    }
}

static inline void _sfte_win32_handle_keydown(sfte_win32_app *app, WPARAM vk, LPARAM lparam) {
    uint32_t mods = _sfte_win32_mods();

    size_t shortcut_count = _SFTE_ARRAY_LEN(_sfte_shortcuts);
    for (size_t i = 0; i != shortcut_count; ++i)
        if (_sfte_shortcuts[i].keysym == (uint32_t)vk &&
            _sfte_shortcuts[i].mod_mask == mods) {
            _sfte_shortcuts[i].func(app->ctx, &_sfte_shortcuts[i].arg);
            return;
        }

    sfte_key key = _sfte_win32_key_from_vk(vk);
    if (key != SFTE_KEY_NONE) {
        sfte_input_key(app->ctx, key, mods);
        return;
    }

    // For modified printable keys, WM_CHAR only reports the resulting control character.
    // Resolve the base character with Ctrl/Alt/Win cleared and route it through the core so
    // Ctrl+<key>, Alt+<key> and the kitty keyboard protocol behave like the Wayland backend.
    uint8_t altgr = (GetKeyState(VK_RMENU) & 0x8000) != 0;
    if (!altgr && (mods & (SFTE_MOD_CTRL | SFTE_MOD_ALT | SFTE_MOD_SUPER))) {
        BYTE keystate[256];
        GetKeyboardState(keystate);
        keystate[VK_CONTROL] = 0;
        keystate[VK_MENU] = 0;
        keystate[VK_LWIN] = 0;
        keystate[VK_RWIN] = 0;

        WCHAR buf[8];
        int n = ToUnicodeEx((UINT)vk, (UINT)((lparam >> 16) & 0xFF), keystate, buf, 8, 0,
                            GetKeyboardLayout(0));
        if (n == 1 && buf[0] >= 32 && buf[0] < 127)
            sfte_input_key(app->ctx, (sfte_key)buf[0], mods);
    }
}

static inline void _sfte_win32_handle_char(sfte_win32_app *app, WPARAM ch) {
    if (ch < 32 || ch == 127) return;  // control keys are handled via WM_KEYDOWN

    uint32_t mods = _sfte_win32_mods();
    uint8_t altgr = (GetKeyState(VK_RMENU) & 0x8000) != 0;
    if (!altgr && (mods & (SFTE_MOD_CTRL | SFTE_MOD_ALT | SFTE_MOD_SUPER))) return;

    WCHAR pair[3];
    int pair_len = 1;
    pair[0] = (WCHAR)ch;

    if (pair[0] >= 0xD800 && pair[0] <= 0xDBFF) {
        app->surrogate_high = pair[0];
        return;
    }
    if (pair[0] >= 0xDC00 && pair[0] <= 0xDFFF) {
        if (!app->surrogate_high) return;
        pair[0] = app->surrogate_high;
        pair[1] = (WCHAR)ch;
        pair_len = 2;
    }
    app->surrogate_high = 0;

    char utf8[8];
    int n = WideCharToMultiByte(CP_UTF8, 0, pair, pair_len, utf8, sizeof(utf8), NULL, NULL);
    if (n > 0) sfte_input_text(app->ctx, utf8, (size_t)n);
}

static inline void _sfte_win32_apply_dark_title(HWND hwnd) {
    BOOL dark = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        (void)DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
}

static inline LRESULT CALLBACK _sfte_win32_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                                    LPARAM lparam) {
    sfte_win32_app *app = (sfte_win32_app *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    if (msg == WM_NCCREATE) {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lparam;
        app = (sfte_win32_app *)cs->lpCreateParams;
        app->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)app);
    }
    if (!app) return DefWindowProcW(hwnd, msg, wparam, lparam);

    switch (msg) {
    case WM_ERASEBKGND: return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (app->mem_dc && app->pixels) {
            // While a live resize is in progress the event loop is blocked, so the backbuffer
            // can be smaller than the client area. Fill the exposed strips with the terminal
            // background instead of leaving them unpainted.
            RECT client;
            GetClientRect(hwnd, &client);
            if (app->width < client.right) {
                RECT strip = {app->width, 0, client.right, client.bottom};
                FillRect(dc, &strip, app->bg_brush);
            }
            if (app->height < client.bottom) {
                RECT strip = {0, app->height, client.right, client.bottom};
                FillRect(dc, &strip, app->bg_brush);
            }
            BitBlt(dc, 0, 0, app->width, app->height, app->mem_dc, 0, 0, SRCCOPY);
        } else {
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(dc, &rc, app->bg_brush);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        if (wparam == SIZE_MINIMIZED) return 0;
        app->pending_width = (int32_t)LOWORD(lparam);
        app->pending_height = (int32_t)HIWORD(lparam);
        app->pending_resize = 1;
        return 0;
    }

    case WM_DPICHANGED: {
        RECT *suggested = (RECT *)lparam;
        SetWindowPos(hwnd, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        _sfte_win32_apply_dpi(app, (UINT)HIWORD(wparam));
        return 0;
    }

#if SFTE_TERM_FOCUS
    case WM_SETFOCUS:
        sfte_set_focus(app->ctx, 1);
        app->needs_render = 1;
        return 0;
    case WM_KILLFOCUS:
        sfte_set_focus(app->ctx, 0);
        app->needs_render = 1;
        return 0;
#endif  // SFTE_TERM_FOCUS

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        _sfte_win32_handle_keydown(app, wparam, lparam);
        return 0;

    case WM_CHAR:
    case WM_SYSCHAR:
        _sfte_win32_handle_char(app, wparam);
        return 0;

#if SFTE_INPUT_MOUSE
    case WM_MOUSEMOVE:
        sfte_mouse_move(app->ctx, (int32_t)(int16_t)LOWORD(lparam), (int32_t)(int16_t)HIWORD(lparam));
        app->needs_render = 1;
        return 0;

    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP: {
        int32_t x = (int32_t)(int16_t)LOWORD(lparam);
        int32_t y = (int32_t)(int16_t)HIWORD(lparam);
        uint8_t pressed = (msg == WM_LBUTTONDOWN);
        sfte_mouse_click(app->ctx, SFTE_MOUSE_BUTTON_LEFT, pressed, x, y);
#if SFTE_CLIPBOARD && SFTE_INPUT_SELECTION
        if (!pressed) _sfte_win32_clipboard_copy(app->ctx, NULL);
#endif  // SFTE_CLIPBOARD && SFTE_INPUT_SELECTION
        app->needs_render = 1;
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int32_t delta = GET_WHEEL_DELTA_WPARAM(wparam);
        POINT pt = {(LONG)(int16_t)LOWORD(lparam), (LONG)(int16_t)HIWORD(lparam)};
        ScreenToClient(hwnd, &pt);
        sfte_mouse_scroll(app->ctx, delta > 0 ? 1 : -1, pt.x, pt.y);
        app->needs_render = 1;
        return 0;
    }
#endif  // SFTE_INPUT_MOUSE

    case WM_CLOSE:
        app->running = 0;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        app->hwnd = NULL;
        PostQuitMessage(0);
        return 0;

    default: break;
    }

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static inline void _sfte_win32_register_class(sfte_win32_app *app) {
    uint32_t bg = SFTE_COLOR_BG;
    app->bg_brush = CreateSolidBrush(RGB((bg >> 16) & 0xFF, (bg >> 8) & 0xFF, bg & 0xFF));

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = _sfte_win32_wnd_proc;
    wc.hInstance = app->hinstance;
    wc.hCursor = LoadCursor(NULL, IDC_IBEAM);
    wc.hbrBackground = app->bg_brush;
    wc.lpszClassName = L"sfte_win32";
    RegisterClassExW(&wc);
}

#if SFTE_CLIPBOARD
static inline void _sfte_win32_clipboard_set(const char *utf8) {
    if (!utf8) return;
    WCHAR *wide = _sfte_win32_utf8_to_wide(utf8);
    if (!wide) return;

    size_t bytes = (wcslen(wide) + 1) * sizeof(WCHAR);
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (mem) {
            void *dst = GlobalLock(mem);
            if (dst) {
                memcpy(dst, wide, bytes);
                GlobalUnlock(mem);
                if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
            } else {
                GlobalFree(mem);
            }
        }
        CloseClipboard();
    }
    SFTE_FREE(wide);
}

static inline char *_sfte_win32_clipboard_get(void) {
    char *out = NULL;
    if (OpenClipboard(NULL)) {
        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        if (handle) {
            const WCHAR *wide = (const WCHAR *)GlobalLock(handle);
            if (wide) {
                out = _sfte_win32_wide_to_utf8(wide, -1);
                GlobalUnlock(handle);
            }
        }
        CloseClipboard();
    }
    return out;
}

#if SFTE_INPUT_SELECTION
#if SFTE_CLIPBOARD_OSC52
static inline void _sfte_win32_osc52_clipboard_cb(void *user_data, char target, const char *data) {
    (void)user_data;
    if (target != 'c' || !data) return;
    _sfte_win32_clipboard_set(data);
}
#endif  // SFTE_CLIPBOARD_OSC52

static inline void _sfte_win32_clipboard_copy(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    if (!ctx->term.mouse_sel_active) return;

    size_t needed = sfte_get_selection(ctx, NULL, 0);
    if (needed == 0) {
        _SFTE_INFO(ctx, CLIPBOARD_EMPTY);
        return;
    }

    char *buf = (char *)SFTE_MALLOC(needed);
    if (!buf) return;
    sfte_get_selection(ctx, buf, needed);
    _sfte_win32_clipboard_set(buf);
    SFTE_FREE(buf);
}
#endif  // SFTE_INPUT_SELECTION

static inline void _sfte_win32_clipboard_paste(sfte_ctx *ctx, const sfte_arg *arg) {
    (void)arg;
    char *text = _sfte_win32_clipboard_get();
    if (!text) return;
    sfte_input_paste_begin(ctx);
    sfte_input_text(ctx, text, strlen(text));
    sfte_input_paste_end(ctx);
    SFTE_FREE(text);
}
#endif  // SFTE_CLIPBOARD

#if SFTE_INPUT_HYPERLINKS
static inline void _sfte_win32_open_link_cb(void *user_data, const char *uri) {
    (void)user_data;
    if (!uri) return;
    WCHAR *wide = _sfte_win32_utf8_to_wide(uri);
    if (!wide) return;
    (void)ShellExecuteW(NULL, L"open", wide, NULL, NULL, SW_SHOWNORMAL);
    SFTE_FREE(wide);
}
#endif  // SFTE_INPUT_HYPERLINKS

#if SFTE_FONT_ZOOM
static inline void _sfte_win32_font_resize(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_win32_app *app = (sfte_win32_app *)ctx->user_data;
    sfte_zoom(ctx, arg->f);
    _sfte_win32_pty_update(app);
    app->needs_render = 1;
}

static inline void _sfte_win32_font_reset(sfte_ctx *ctx, const sfte_arg *dummy) {
    (void)dummy;
    sfte_win32_app *app = (sfte_win32_app *)ctx->user_data;
    const sfte_arg arg = {.f = (SFTE_FONT_DEFAULT_SIZE * app->font_scale) - ctx->font.cur_size};
    _sfte_win32_font_resize(ctx, &arg);
}
#endif  // SFTE_FONT_ZOOM

#if SFTE_TERM_SCROLLBACK_CAP
static inline void _sfte_win32_view_scroll(sfte_ctx *ctx, const sfte_arg *arg) {
    sfte_view_scroll(ctx, arg->i);
    sfte_win32_app *app = (sfte_win32_app *)ctx->user_data;
    app->needs_render = 1;
}
#endif  // SFTE_TERM_SCROLLBACK_CAP

/*
    The primary Win32 event loop. Waits simultaneously for window messages, shell output on the
    ConPTY pipe, shell process termination and the core's blink/animation timeout.
*/
static inline void _sfte_win32_loop(sfte_win32_app *app) {
    while (app->running) {
        MSG msg;
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                app->running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!app->running) break;

        if (app->pending_resize) {
            app->pending_resize = 0;
            if (app->pending_width > 0 && app->pending_height > 0 &&
                (app->pending_width != app->width || app->pending_height != app->height)) {
                app->width = app->pending_width;
                app->height = app->pending_height;
                sfte_resize(app->ctx, app->width, app->height);
                _sfte_win32_create_backbuffer(app);
                _sfte_win32_pty_update(app);
                app->needs_render = 1;
            }
        }

        if (sfte_tick(app->ctx)) app->needs_render = 1;

        HANDLE handles[2];
        DWORD count = 0;
        if (app->pty_out_read) handles[count++] = app->pty_out_read;
        if (app->pty_proc) handles[count++] = app->pty_proc;

        int32_t timeout = sfte_get_timeout_ms(app->ctx);
        DWORD wait_ms = (timeout < 0) ? INFINITE : (DWORD)timeout;
        if (app->needs_render) wait_ms = 0;

        DWORD wait = MsgWaitForMultipleObjects(count, count ? handles : NULL, FALSE, wait_ms,
                                               QS_ALLINPUT);
        if (wait == WAIT_FAILED) {
            app->running = 0;
            break;
        }
        if (count > 0 && wait == WAIT_OBJECT_0) {
            uint8_t buf[SFTE_TERM_PTY_BUF_SIZE];
            for (;;) {
                DWORD avail = 0;
                if (!PeekNamedPipe(app->pty_out_read, NULL, 0, NULL, &avail, NULL)) {
                    app->running = 0;  // pipe broken
                    break;
                }
                if (avail == 0) break;

                DWORD read = 0;
                if (!ReadFile(app->pty_out_read, buf, sizeof(buf), &read, NULL) || read == 0) {
                    app->running = 0;
                    break;
                }
                sfte_parse(app->ctx, buf, read);
                app->needs_render = 1;
            }
        } else if (count > 1 && wait == WAIT_OBJECT_0 + 1) {
            app->running = 0;
        }

        // The pipe can remain signaled with no data at EOF; rely on the process handle to
        // detect that the shell has exited.
        if (app->running && app->pty_proc &&
            WaitForSingleObject(app->pty_proc, 0) == WAIT_OBJECT_0)
            app->running = 0;

        if (app->needs_render) _sfte_win32_render(app);
    }
}

static inline void _sfte_win32_unload(sfte_win32_app *app) {
    _sfte_win32_destroy_backbuffer(app);
    if (app->bg_brush) DeleteObject(app->bg_brush);
    if (app->pty_in_write) CloseHandle(app->pty_in_write);
    if (app->pty_out_read) CloseHandle(app->pty_out_read);
    if (app->hpc) ClosePseudoConsole(app->hpc);
    if (app->pty_proc) CloseHandle(app->pty_proc);
    if (app->shell_cmdline) SFTE_FREE(app->shell_cmdline);
    if (app->hwnd) DestroyWindow(app->hwnd);
    app->pty_in_write = NULL;
    app->pty_out_read = NULL;
    app->hpc = NULL;
    app->pty_proc = NULL;
}
#endif  // SFTE_WIN32

// #################################################################################################
// >>>PUBLIC IMPLEMENTATION
// #################################################################################################

// =================================================================================================
// >>core initialization & lifecycle
// =================================================================================================

sfte_ctx *sfte_init(sfte_write_cb write_fn, void *user_data) {
    sfte_ctx *ctx = (sfte_ctx *)SFTE_CALLOC(1, sizeof(sfte_ctx));
    SFTE_ASSERT(ctx, "failed to allocate core context");

    void *stack_back_buf = SFTE_MALLOC(SFTE_MEM_STACK_SIZE);
    _sfte_mem_stack_init(&ctx->stack, stack_back_buf, SFTE_MEM_STACK_SIZE);

    ctx->write_cb = write_fn;
    ctx->user_data = user_data;

#ifndef SFTE_NO_LOGGING
    ctx->logger.func = SFTE_LOG_FUNC;
#endif  // !SFTE_NO_LOGGING

#if SFTE_IMG_SIXEL
    memcpy(ctx->sixel.palette, _sfte_palette_256, 256 * sizeof(uint32_t));
#endif  // SFTE_IMG_SIXEL

    ctx->term.cols = SFTE_TERM_INIT_COLS;
    ctx->term.rows = SFTE_TERM_INIT_ROWS;
#if SFTE_FONT_LIGATURES
    ctx->term.render_shaper_ids = (uint16_t *)SFTE_CALLOC(SFTE_TERM_INIT_COLS, sizeof(uint16_t));
    ctx->term.row_hashes = (uint64_t *)SFTE_CALLOC(SFTE_TERM_INIT_ROWS, sizeof(uint64_t));
    ctx->term.row_shaper_ids = (uint16_t *)SFTE_CALLOC(SFTE_TERM_INIT_COLS * SFTE_TERM_INIT_ROWS,
                                                       sizeof(uint16_t));
#endif  // SFTE_FONT_LIGATURES
    ctx->term.render_ids = (uint16_t *)SFTE_CALLOC(SFTE_TERM_INIT_COLS, sizeof(uint16_t));
    ctx->term.render_font_indices = (uint8_t *)SFTE_CALLOC(SFTE_TERM_INIT_COLS, sizeof(uint8_t));
    ctx->term.render_target_caches = (sfte_font_cache **)SFTE_CALLOC(SFTE_TERM_INIT_COLS,
                                                                     sizeof(sfte_font_cache *));
    ctx->term.auto_wrap = 1;
    ctx->term.origin_mode = 0;

#if SFTE_TERM_SCROLLBACK_CAP
    ctx->term.sb_cap = SFTE_TERM_SCROLLBACK_CAP;
    ctx->term.scrollback = (sfte_cell *)SFTE_CALLOC(ctx->term.sb_cap * ctx->term.cols,
                                                    sizeof(sfte_cell));
#endif  // SFTE_TERM_SCROLLBACK_CAP

    _sfte_grid_resize_tabs(ctx, 0, ctx->term.cols);

#if SFTE_INPUT_MOUSE
    ctx->term.mouse_btn_state = 3;
#endif  // SFTE_INPUT_MOUSE
#if SFTE_INPUT_KITTY
    ctx->term.kitty_kb_stack_idx[0] = 0;
    ctx->term.kitty_kb_stack_idx[1] = 0;
    ctx->term.kitty_kb_stack[0][0] = 0;
    ctx->term.kitty_kb_stack[1][0] = 0;
#endif  // SFTE_INPUT_KITTY

#if SFTE_CURSOR_BLINK
    ctx->term.blink_enabled = 1;
    ctx->term.blink_visible = 1;
    ctx->term.next_blink_ms = SFTE_TIME_MS() + SFTE_CURSOR_BLINK_RATE_MS;
#endif  // SFTE_CURSOR_BLINK
#if SFTE_CURSOR_TRAIL
    ctx->term.tail_rx = 0.0f;
    ctx->term.tail_ry = 0.0f;
    ctx->term.trail_dmg.x = 0.0f;
    ctx->term.trail_dmg.y = 0.0f;
    ctx->term.trail_dmg.w = 0.0f;
    ctx->term.trail_dmg.h = 0.0f;
    ctx->term.last_grid_col = 0;
    ctx->term.last_grid_row = 0;
    ctx->term.last_move_ms = 0;
    ctx->term.is_trailing = 0;
#endif  // SFTE_CURSOR_TRAIL
#if SFTE_CURSOR_DYNAMIC
    ctx->term.cursor_color = SFTE_CURSOR_COLOR;
    ctx->term.cursor_style = SFTE_CURSOR_STYLE;
#endif  // SFTE_CURSOR_DYNAMIC
#if SFTE_UNDERLINE_COLORED
    ctx->term.cur_ul_color = _SFTE_COLOR_FG_DEFAULT;
#endif  // SFTE_UNDERLINE_COLORED
#if SFTE_UNDERLINE_EXTENDED
    ctx->term.cur_ul_style = 0;
#endif  // SFTE_UNDERLINE_EXTENDED
    ctx->term.scroll_top = 0;
    ctx->term.scroll_bot = ctx->term.rows - 1;

    ctx->term.osc_cap = SFTE_OSC_INIT_CAP;
    ctx->term.osc_payload = (char *)SFTE_MALLOC(ctx->term.osc_cap);
    ctx->term.osc_len = 0;

#if SFTE_INPUT_HYPERLINKS
    ctx->term.link_pool_cap = SFTE_INPUT_HYPERLINKS_INIT_CAP;
    ctx->term.link_pool = (char **)SFTE_CALLOC(ctx->term.link_pool_cap, sizeof(char *));
    ctx->term.link_pool_len = 1;  // idx 0 is reserved for no link
    ctx->term.cur_link_idx = 0;
#endif  // SFTE_INPUT_HYPERLINKS
#if SFTE_TERM_FOCUS
    ctx->term.is_focused = 1;
#endif  // SFTE_TERM_FOCUS

    ctx->term.cells = (sfte_cell *)SFTE_MALLOC(ctx->term.cols * ctx->term.rows * sizeof(sfte_cell));
    SFTE_ASSERT(ctx->term.cells, "failed to allocate term grid");
    memset(ctx->term.cells, 0, ctx->term.cols * ctx->term.rows * sizeof(sfte_cell));

    return ctx;
}

void sfte_free(sfte_ctx *ctx) {
    if (!ctx) return;

    SFTE_FREE(ctx->stack.buf);
    SFTE_FREE(ctx->term.tab_stops);

#define FREE_CACHE(type)                                                                           \
    do {                                                                                           \
        for (int i = 0; i < type.num_fonts; ++i)                                                   \
            if (type.owns_ttf_buf[i]) SFTE_FREE(type.ttf_buf[i]);                                  \
        SFTE_FREE(type.atlas_pxs);                                                                 \
        SFTE_FREE(type.glyphs);                                                                    \
    } while (0)

    FREE_CACHE(ctx->font.regular);
#ifdef SFTE_FONT_BOLD
    FREE_CACHE(ctx->font.bold);
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    FREE_CACHE(ctx->font.italic);
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    FREE_CACHE(ctx->font.bold_italic);
#endif  // SFTE_FONT_BOLD_ITALIC

#undef FREE_CACHE

    SFTE_FREE(ctx->term.osc_payload);
    SFTE_FREE(ctx->term.cells);
#if SFTE_TERM_ALT_SCREEN
    SFTE_FREE(ctx->term.alt_cells);
#endif  // SFTE_TERM_ALT_SCREEN
#if SFTE_TERM_SCROLLBACK_CAP
    SFTE_FREE(ctx->term.scrollback);
#endif  // SFTE_TERM_SCROLLBACK_CAP
#if SFTE_INPUT_HYPERLINKS
    if (ctx->term.link_pool) {
        for (uint16_t i = 0; i < ctx->term.link_pool_len; ++i) SFTE_FREE(ctx->term.link_pool[i]);
        SFTE_FREE(ctx->term.link_pool);
    }
#endif  // SFTE_INPUT_HYPERLINKS

#if SFTE_IMG_SIXEL
    _sfte_sixel_deinit(ctx);
#endif  // SFTE_IMG_SIXEL
#if SFTE_IMG_KITTY
    _sfte_kitty_deinit(ctx);
#endif  // SFTE_IMG_KITTY

    SFTE_FREE(ctx);
}

void sfte_font_load_mem(sfte_ctx *ctx, sfte_font_style style, const uint8_t *ttf_data) {
    sfte_font_cache *cache = _sfte_font_get_cache(ctx, style);
    if (!cache || !ttf_data || cache->num_fonts >= SFTE_FONT_MAX_COUNT) return;

    uint8_t idx = cache->num_fonts++;
    cache->ttf_buf[idx] = (uint8_t *)ttf_data;
    cache->owns_ttf_buf[idx] = 0;

    if (idx == 0) {
        if (style == SFTE_FONT_STYLE_REGULAR && idx == 0)
            ctx->font.cur_size = SFTE_FONT_DEFAULT_SIZE;

        if (!cache->atlas_pxs) {
            cache->atlas_pxs = (uint8_t *)SFTE_MALLOC(SFTE_FONT_ATLAS_SIZE * SFTE_FONT_ATLAS_SIZE);
            SFTE_ASSERT(cache->atlas_pxs, "failed to allocate font atlas");
        }

        if (!cache->glyphs) {
            cache->glyphs = (sfte_glyph *)SFTE_CALLOC(SFTE_FONT_GLYPH_CAP, sizeof(sfte_glyph));
            SFTE_ASSERT(cache->glyphs, "failed to allocate glyphs storage");
        }
    }

    SFTE_FONT_INIT(&cache->info[idx], cache->ttf_buf[idx]);

#if SFTE_FONT_LIGATURES
    _sfte_shaper_init(&cache->shaper[idx], cache->ttf_buf[idx]);
#endif  // SFTE_FONT_LIGATURES

    if (style == SFTE_FONT_STYLE_REGULAR && idx == 0)
        _sfte_font_reset_cache(ctx);
    else {
        float tweak = _sfte_font_scales[idx];
        if (tweak <= 0.0f) tweak = 1.0f;
        cache->scales[idx] = SFTE_FONT_GET_SCALE(&cache->info[idx], ctx->font.cur_size * tweak);
    }

    _SFTE_INFO(ctx, FONT_LOADED);
}

void sfte_font_load_file(sfte_ctx *ctx, sfte_font_style style, const char *path) {
    sfte_font_cache *cache = _sfte_font_get_cache(ctx, style);
    if (!cache) return;

    FILE *f = fopen(path, "rb");
    if (!f) {
        _SFTE_ERROR(ctx, FONT_LOAD_FAIL, path);
        return;
    }

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = (uint8_t *)SFTE_MALLOC(size);
    SFTE_ASSERT(fread(buf, 1, size, f) == size, "failed to read font file");
    fclose(f);

    sfte_font_load_mem(ctx, style, buf);
    cache->owns_ttf_buf[cache->num_fonts - 1] = 1;
}

#ifndef SFTE_NO_POSIX
pid_t sfte_posix_pty_spawn(sfte_ctx *ctx, int32_t *out_fd, uint16_t px_w, uint16_t px_h) {
    struct winsize ws = {
        .ws_row = (unsigned short)ctx->term.rows,
        .ws_col = (unsigned short)ctx->term.cols,
        .ws_xpixel = (unsigned short)px_w,
        .ws_ypixel = (unsigned short)px_h,
    };

    pid_t pid = forkpty(out_fd, NULL, NULL, &ws);
    if (pid == -1) {
        _SFTE_ERROR(ctx, PTY_FORK_FAIL, errno);
        return -1;
    }

    if (pid == 0) {
        setenv("TERM", SFTE_TERM_ENV, 1);
        char *shell = getenv("SHELL");
        if (!shell) {
            shell = (char *)"/bin/sh";
            _SFTE_WARN(ctx, SHELL_FALLBACK);
        }
        execlp(shell, shell, NULL);
        abort();  // if execlp returns, it failed to exec the shell
    }

    int flags = fcntl(*out_fd, F_GETFL, 0);
    fcntl(*out_fd, F_SETFL, flags | O_NONBLOCK);

    _SFTE_INFO(ctx, PTY_SPAWN);
    return pid;
}

void sfte_posix_pty_resize(sfte_ctx *ctx, int32_t pty_fd, uint16_t px_w, uint16_t px_h) {
    if (pty_fd <= 0) return;
    struct winsize ws = {
        .ws_row = (unsigned short)ctx->term.rows,
        .ws_col = (unsigned short)ctx->term.cols,
        .ws_xpixel = (unsigned short)px_w,
        .ws_ypixel = (unsigned short)px_h,
    };
    ioctl(pty_fd, TIOCSWINSZ, &ws);
}
#endif  // !SFTE_NO_POSIX

int32_t sfte_get_timeout_ms(sfte_ctx *ctx) {
    if (!ctx) return -1;

    int32_t frame_ms = 1000 / SFTE_TERM_REFRESH_RATE;
    if (frame_ms < 1) frame_ms = 1;

#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
    if (ctx->term.is_animating) return frame_ms;
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
#if SFTE_TERM_SCROLL_SMOOTH
    if (ctx->term.is_scrolling) return frame_ms;
#endif  // SFTE_TERM_SCROLL_SMOOTH
#if SFTE_CURSOR_TRAIL
    if (ctx->term.is_trailing) return frame_ms;
#endif  // SFTE_CURSOR_TRAIL

    int32_t timeout = -1;

#if SFTE_CURSOR_BLINK
    uint8_t can_blink = ctx->term.blink_enabled && !ctx->term.hide_cursor;
#if SFTE_TERM_FOCUS
    can_blink &= ctx->term.is_focused;
#endif  // SFTE_TERM_FOCUS

    if (can_blink) {
        uint64_t now = SFTE_TIME_MS();
        int32_t time_to_next = (int32_t)(ctx->term.next_blink_ms - now);
        if (time_to_next < 0) time_to_next = 0;
        timeout = time_to_next;
    }
#endif  // SFTE_CURSOR_BLINK

    return timeout;
}

uint8_t sfte_tick(sfte_ctx *ctx) {
    if (!ctx) return 0;
    uint8_t needs_render = 0;

#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
    if (ctx->term.is_animating) needs_render = 1;
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN

#if SFTE_TERM_SCROLL_SMOOTH
    if (ctx->term.is_scrolling) needs_render = 1;
#endif  // SFTE_TERM_SCROLL_SMOOTH

#if SFTE_CURSOR_BLINK
    uint8_t can_blink = ctx->term.blink_enabled && !ctx->term.hide_cursor;
#if SFTE_TERM_FOCUS
    can_blink &= ctx->term.is_focused;
#endif  // SFTE_TERM_FOCUS

    if (can_blink) {
        uint64_t now = SFTE_TIME_MS();
        if (now >= ctx->term.next_blink_ms) {
            ctx->term.blink_visible = !ctx->term.blink_visible;
            ctx->term.next_blink_ms = now + SFTE_CURSOR_BLINK_RATE_MS;

            int16_t vis_col = ctx->term.cursor_col >= ctx->term.cols ? ctx->term.cols - 1
                                                                     : ctx->term.cursor_col;
            ctx->term.cells[_sfte_grid_get_idx(ctx, vis_col, ctx->term.cursor_row)].dirty = 1;
            needs_render = 1;
        }
    }
#endif  // SFTE_CURSOR_BLINK

#if SFTE_CURSOR_TRAIL
    if (ctx->term.is_trailing) {
        int16_t vis_col = ctx->term.cursor_col >= ctx->term.cols ? ctx->term.cols - 1
                                                                 : ctx->term.cursor_col;
        float target_rx = vis_col * ctx->font.cell_width;
        float target_ry = ctx->term.cursor_row * ctx->font.cell_height;

        uint64_t now = SFTE_TIME_MS();
        if (ctx->term.last_trail_update_ms == 0) ctx->term.last_trail_update_ms = now;
        float dt_ms = (float)(now - ctx->term.last_trail_update_ms);
        ctx->term.last_trail_update_ms = now;

        float tx = target_rx - ctx->term.tail_rx;
        float ty = target_ry - ctx->term.tail_ry;

        // Snap to target if we're close enough to stop animating
        if (tx * tx + ty * ty <= 0.5f) {
            ctx->term.is_trailing = 0;
            ctx->term.tail_rx = target_rx;
            ctx->term.tail_ry = target_ry;
            ctx->term.last_trail_update_ms = 0;
        } else {
            float decay = dt_ms * SFTE_CURSOR_TRAIL_DECAY;
            if (decay > 1.0f) decay = 1.0f;
            ctx->term.tail_rx += tx * decay;
            ctx->term.tail_ry += ty * decay;
        }
        needs_render = 1;
    }
#endif  // SFTE_CURSOR_TRAIL

    return needs_render;
}

// =================================================================================================
// >>rendering & parsing
// =================================================================================================

void sfte_get_ideal_size(sfte_ctx *ctx, int16_t cols, int16_t rows, int32_t *out_w,
                         int32_t *out_h) {
    if (out_w) *out_w = cols * ctx->font.cell_width + (2 * SFTE_WINDOW_PAD_X);
    if (out_h) *out_h = rows * ctx->font.cell_height + (2 * SFTE_WINDOW_PAD_Y);
}

void sfte_parse(sfte_ctx *ctx, const uint8_t *data, size_t len) {
    if (len == 0 || !data) return;

#if SFTE_TERM_SCROLLBACK_CAP
    // snap view to bottom if new output arrives
    if (ctx->term.sb_offset > 0) {
        ctx->term.sb_offset = 0;
        _sfte_grid_dirty_range(ctx, 0, ctx->term.cols * ctx->term.rows);
    }
#endif  // SFTE_TERM_SCROLLBACK_CAP

#if SFTE_CURSOR_BLINK
    // reset blink timer when typing/outputting
    ctx->term.blink_visible = 1;
    ctx->term.next_blink_ms = SFTE_TIME_MS() + SFTE_CURSOR_BLINK_RATE_MS;
#endif  // SFTE_CURSOR_BLINK

    // parse incoming stream
    for (size_t i = 0; i < len; ++i) _sfte_parser_feed_byte(ctx, data[i]);

#if SFTE_CURSOR_TRAIL
    int16_t vis_col = ctx->term.cursor_col >= ctx->term.cols ? ctx->term.cols - 1
                                                             : ctx->term.cursor_col;
    float target_rx = vis_col * ctx->font.cell_width;
    float target_ry = ctx->term.cursor_row * ctx->font.cell_height;

    if (vis_col != ctx->term.last_grid_col || ctx->term.cursor_row != ctx->term.last_grid_row) {
        uint64_t now = SFTE_TIME_MS();

        if (ctx->term.last_move_ms != 0 && (now - ctx->term.last_move_ms >= SFTE_CURSOR_TRAIL))
            ctx->term.is_trailing = 1;
        else if (!ctx->term.is_trailing) {
            ctx->term.tail_rx = target_rx;
            ctx->term.tail_ry = target_ry;
        }

        ctx->term.last_grid_col = vis_col;
        ctx->term.last_grid_row = ctx->term.cursor_row;
        ctx->term.last_move_ms = now;
    }
#endif  // SFTE_CURSOR_TRAIL
}

void sfte_render(sfte_ctx *ctx, void *px_buf, int32_t w, int32_t h, sfte_damage_rect *out_dmg) {
    ctx->width = w;
    ctx->height = h;

    _sfte_pass_info passes[2];
    uint8_t num_passes = _sfte_render_prepare_passes(ctx, px_buf, passes, out_dmg);

    int16_t new_cols = (w - (2 * SFTE_WINDOW_PAD_X)) / ctx->font.cell_width;
    int16_t new_rows = (h - (2 * SFTE_WINDOW_PAD_Y)) / ctx->font.cell_height;
    if (new_cols != ctx->term.cols || new_rows != ctx->term.rows)
        _sfte_grid_resize(ctx, new_cols, new_rows);

    int16_t vis_col = _SFTE_CLAMP(ctx->term.cursor_col, 0, ctx->term.cols - 1);
    int32_t vis_row = _sfte_grid_log2vis(ctx, ctx->term.cursor_row);
#if SFTE_FONT_WIDE_CHARS
    uint8_t cursor_is_visible = (vis_row >= 0 && vis_row < ctx->term.rows);
    if (cursor_is_visible && vis_col > 0 &&
        (_sfte_grid_get_cell(ctx, vis_col, vis_row)->attr & _SFTE_ATTR_DUMMY))
        vis_col--;
#endif  // SFTE_FONT_WIDE_CHARS

    _sfte_render_propagate_damage(ctx, vis_col, vis_row);

#if SFTE_CURSOR_TRAIL
    _sfte_grid_dirty_trail(ctx);
#endif  // SFTE_CURSOR_TRAIL

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    _sfte_render_sort_images(ctx);
    int32_t base_y_off = _sfte_grid_log2vis(ctx, 0) * ctx->font.cell_height;
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

#if SFTE_TERM_ALT_SCREEN
    uint8_t orig_alt_active = ctx->term.alt_active;
#endif  // SFTE_TERM_ALT_SCREEN

    for (uint8_t p = 0; p < num_passes; ++p) {
        ctx->term.cells = passes[p].grid;
        ctx->term.hide_cursor = passes[p].hide_cursor;

#if SFTE_TERM_ALT_SCREEN
        if (num_passes == 2 && p == 0)
            ctx->term.alt_active = !orig_alt_active;
        else
            ctx->term.alt_active = orig_alt_active;
#endif  // SFTE_TERM_ALT_SCREEN

        // Rendering order:
        // BG grid -> BG images -> FG grid -> FG images
        _sfte_render_bg_grid(ctx, px_buf, vis_col, vis_row, passes[p].y_off);

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
        _sfte_render_images(ctx, px_buf, 1, base_y_off + passes[p].y_off, ctx->padding_dirty);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY

        _sfte_render_fg_grid(ctx, px_buf, vis_col, vis_row, passes[p].y_off, out_dmg);

#if SFTE_IMG_SIXEL || SFTE_IMG_KITTY
        _sfte_render_images(ctx, px_buf, 0, base_y_off + passes[p].y_off, ctx->padding_dirty);
#endif  // SFTE_IMG_SIXEL || SFTE_IMG_KITTY
    }

#if SFTE_TERM_ALT_SCREEN
    ctx->term.alt_active = orig_alt_active;
#endif  // SFTE_TERM_ALT_SCREEN

#if SFTE_CURSOR_TRAIL
#if SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
    if (!ctx->term.is_animating)
#endif  // SFTE_TERM_ANIMATE_SCREEN && SFTE_TERM_ALT_SCREEN
        _sfte_render_trail(ctx, px_buf, out_dmg);
#endif  // SFTE_CURSOR_TRAIL

    if (ctx->padding_dirty) {
        _sfte_view_clear_padding_rects(ctx, px_buf);
        ctx->padding_dirty--;
        if (num_passes == 1) _sfte_render_damage_add(out_dmg, 0, 0, w, h);
    }

    if (out_dmg->w > 0 && out_dmg->h > 0) {
        out_dmg->x = _SFTE_CLAMP(out_dmg->x, 0, w);
        out_dmg->y = _SFTE_CLAMP(out_dmg->y, 0, h);
        out_dmg->w = _SFTE_CLAMP(out_dmg->w, 0, w);
        out_dmg->h = _SFTE_CLAMP(out_dmg->h, 0, h);
        for (int32_t r = 0; r < ctx->term.rows; ++r) {
            int32_t logical_r = _sfte_grid_vis2log(ctx, r);
            if (logical_r >= 0 && logical_r < ctx->term.rows)
                for (int16_t c = 0; c < ctx->term.cols; ++c)
                    _sfte_grid_get_cell(ctx, c, logical_r)->dirty = 0;
        }
    } else
        out_dmg->w = 0, out_dmg->h = 0;
}

void sfte_resize(sfte_ctx *ctx, int32_t w, int32_t h) {
    if (w <= 0 || h <= 0) return;

    ctx->width = w;
    ctx->height = h;
    ctx->padding_dirty = 1;

    int16_t new_cols = (w - (2 * SFTE_WINDOW_PAD_X)) / (int32_t)ctx->font.cell_width;
    if (new_cols < 1) new_cols = 1;
    int16_t new_rows = (h - (2 * SFTE_WINDOW_PAD_Y)) / (int32_t)ctx->font.cell_height;
    if (new_rows < 1) new_rows = 1;

    if (new_cols != ctx->term.cols || new_rows != ctx->term.rows) {
        _sfte_grid_resize(ctx, new_cols, new_rows);

#if SFTE_CURSOR_TRAIL
        ctx->term.warp_tail = 1;
#endif  // SFTE_CURSOR_TRAIL
    }
}

#if SFTE_FONT_ZOOM
void sfte_zoom(sfte_ctx *ctx, float delta) {
    if (delta == 0) return;
    ctx->padding_dirty = 1;
    float new_size = ctx->font.cur_size + delta;
    if (new_size < SFTE_FONT_MIN_SIZE || new_size > SFTE_FONT_MAX_SIZE) return;
    ctx->font.cur_size = new_size;
    _sfte_font_reset_cache(ctx);
    sfte_resize(ctx, ctx->width, ctx->height);
}
#endif  // SFTE_FONT_ZOOM

// =================================================================================================
// >>input & interaction
// =================================================================================================

void sfte_input_text(sfte_ctx *ctx, const char *text, size_t len) {
    if (!ctx || !text || !len) return;

#if SFTE_TERM_SCROLLBACK_CAP
    if (ctx->term.sb_offset > 0) {
        ctx->term.sb_offset = 0;
        _sfte_grid_dirty_range(ctx, 0, ctx->term.cols * ctx->term.rows);
    }
#endif  // SFTE_TERM_SCROLLBACK_CAP
    if (ctx->write_cb) ctx->write_cb(ctx->user_data, text, len);
}

void sfte_input_paste_begin(sfte_ctx *ctx) {
    if (!ctx || !ctx->write_cb || !ctx->term.bracketed_paste) return;
    ctx->write_cb(ctx->user_data, "\033[200~", 6);
}

void sfte_input_paste_end(sfte_ctx *ctx) {
    if (!ctx || !ctx->write_cb || !ctx->term.bracketed_paste) return;
    ctx->write_cb(ctx->user_data, "\033[201~", 6);
}

void sfte_input_key(sfte_ctx *ctx, sfte_key key, uint32_t mod_mask) {
    if (!ctx) return;

    char buf[128];
    size_t size = 0;

#if SFTE_INPUT_KITTY
    uint32_t codepoint = 0;
    size = _sfte_kitty_kb_encode(ctx, key, codepoint, mod_mask, buf, sizeof(buf));
    if (size > 0) {
        sfte_input_text(ctx, buf, size);
        return;
    }
#endif  // SFTE_INPUT_KITTY

    uint8_t csi_mod = 1;
    if (mod_mask & SFTE_MOD_SHIFT) csi_mod += 1;
    if (mod_mask & SFTE_MOD_ALT) csi_mod += 2;
    if (mod_mask & SFTE_MOD_CTRL) csi_mod += 4;
    // SFTE_MOD_SUPER not handled on purpose

    switch (key) {
    case SFTE_KEY_UP:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dA", csi_mod);
        else {
            memcpy(buf, "\033[A", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_DOWN:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dB", csi_mod);
        else {
            memcpy(buf, "\033[B", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_RIGHT:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dC", csi_mod);
        else {
            memcpy(buf, "\033[C", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_LEFT:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dD", csi_mod);
        else {
            memcpy(buf, "\033[D", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_HOME:
        memcpy(buf, "\033[H", 3);
        size = 3;
        break;
    case SFTE_KEY_END:
        memcpy(buf, "\033[F", 3);
        size = 3;
        break;
    case SFTE_KEY_BACKSPACE:
        buf[0] = (mod_mask & SFTE_MOD_ALT) ? '\033' : '\x7f';
        buf[1] = (mod_mask & SFTE_MOD_ALT) ? '\x7f' : '\0';
        size = (mod_mask & SFTE_MOD_ALT) ? 2 : 1;
        break;
    case SFTE_KEY_ENTER:
        buf[0] = '\r';
        size = 1;
        break;
    case SFTE_KEY_TAB:
        buf[0] = '\t';
        size = 1;
        break;
    case SFTE_KEY_ESCAPE:
        buf[0] = '\033';
        size = 1;
        break;
    case SFTE_KEY_PAGE_UP:
        memcpy(buf, "\033[5~", 4);
        size = 4;
        break;
    case SFTE_KEY_PAGE_DOWN:
        memcpy(buf, "\033[6~", 4);
        size = 4;
        break;
    case SFTE_KEY_F1:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dP", csi_mod);
        else {
            memcpy(buf, "\033OP", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_F2:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dQ", csi_mod);
        else {
            memcpy(buf, "\033OQ", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_F3:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dR", csi_mod);
        else {
            memcpy(buf, "\033OR", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_F4:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[1;%dS", csi_mod);
        else {
            memcpy(buf, "\033OS", 3);
            size = 3;
        }
        break;
    case SFTE_KEY_F5:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[15;%d~", csi_mod);
        else {
            memcpy(buf, "\033[15~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F6:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[17;%d~", csi_mod);
        else {
            memcpy(buf, "\033[17~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F7:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[18;%d~", csi_mod);
        else {
            memcpy(buf, "\033[18~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F8:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[19;%d~", csi_mod);
        else {
            memcpy(buf, "\033[19~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F9:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[20;%d~", csi_mod);
        else {
            memcpy(buf, "\033[20~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F10:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[21;%d~", csi_mod);
        else {
            memcpy(buf, "\033[21~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F11:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[23;%d~", csi_mod);
        else {
            memcpy(buf, "\033[23~", 5);
            size = 5;
        }
        break;
    case SFTE_KEY_F12:
        if (csi_mod > 1)
            size = (size_t)snprintf(buf, sizeof(buf), "\033[24;%d~", csi_mod);
        else {
            memcpy(buf, "\033[24~", 5);
            size = 5;
        }
        break;
    default: break;
    }

    if (size == 0 && key >= 32 && key < 127) {
        if (mod_mask & SFTE_MOD_CTRL) {
            if (key >= 'a' && key <= 'z') {
                buf[0] = key - 'a' + 1;
                size = 1;
            } else if (key >= 'A' && key <= 'Z') {
                buf[0] = key - 'A' + 1;
                size = 1;
            } else if (key == ' ') {
                buf[0] = '\0';
                size = 1;
            }
        } else if ((mod_mask & SFTE_MOD_ALT) && !(mod_mask & SFTE_MOD_SHIFT)) {
            buf[0] = (char)key;
            size = 1;
        }
    }

    // alt sends escape
    if (size > 0 && (mod_mask & SFTE_MOD_ALT) && buf[0] != '\033') {
        memmove(buf + 1, buf, size);
        buf[0] = '\033';
        size++;
    }

    sfte_input_text(ctx, buf, size);
}

#if SFTE_INPUT_MOUSE
void sfte_mouse_move(sfte_ctx *ctx, int32_t px_x, int32_t px_y) {
    int16_t c, screen_r;
    int32_t logical_r;
    _sfte_grid_from_px(ctx, px_x, px_y, &c, &logical_r, &screen_r);

    if (ctx->term.mouse_hover_col == c && ctx->term.mouse_hover_row == screen_r) return;

    ctx->term.mouse_hover_col = c;
    ctx->term.mouse_hover_row = screen_r;

    if (ctx->term.mouse_mode) {
        _sfte_input_send_mouse_event(ctx, ctx->term.mouse_btn_state, 0, c, screen_r, 1);
        return;
    }

#if SFTE_INPUT_SELECTION
    if (!ctx->term.mouse_sel_dragging) return;
    sfte_term *term = &ctx->term;

    if (term->mouse_sel_end_col == term->mouse_hover_col && term->mouse_sel_end_row == logical_r)
        return;

    _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_row, term->mouse_sel_end_row);
    term->mouse_sel_end_col = term->mouse_hover_col;
    term->mouse_sel_end_row = logical_r;
    _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_row, term->mouse_sel_end_row);
#endif  // SFTE_INPUT_SELECTION
}

void sfte_mouse_click(sfte_ctx *ctx, sfte_mouse_button btn, uint8_t pressed, int32_t px_x,
                      int32_t px_y) {
    int16_t c, screen_r;
    int32_t logical_r;
    _sfte_grid_from_px(ctx, px_x, px_y, &c, &logical_r, &screen_r);
    sfte_term *term = &ctx->term;

#if SFTE_INPUT_HYPERLINKS
    if (pressed && btn == SFTE_MOUSE_BUTTON_LEFT && ctx->open_link_cb) {
        const char *uri = sfte_get_link_at(ctx, c, logical_r);
        if (uri) {
            ctx->open_link_cb(ctx->user_data, uri);
            return;
        }
    }
#endif  // SFTE_INPUT_HYPERLINKS

    if (term->mouse_mode) {
        if (pressed)
            term->mouse_btn_state = btn;
        else
            term->mouse_btn_state = SFTE_INPUT_MOUSE_BTN_RELEASE;

        _sfte_input_send_mouse_event(ctx, btn, !pressed, c, screen_r, 0);
        return;
    }

#if SFTE_INPUT_SELECTION
    if (pressed && btn == SFTE_MOUSE_BUTTON_LEFT) {
        if (term->mouse_sel_active)
            _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_row, term->mouse_sel_end_row);

        term->mouse_hover_col = c;
        term->mouse_hover_row = screen_r;

        term->mouse_sel_start_col = c;
        term->mouse_sel_start_row = logical_r;
        term->mouse_sel_end_col = c;
        term->mouse_sel_end_row = logical_r;
        term->mouse_sel_active = 1;
        term->mouse_sel_dragging = 1;

        _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_row, term->mouse_sel_end_row);
    } else {
        term->mouse_sel_dragging = 0;
        if (term->mouse_sel_start_col == term->mouse_sel_end_col &&
            term->mouse_sel_start_row == term->mouse_sel_end_row) {
            term->mouse_sel_active = 0;
            _sfte_grid_dirty_rows(ctx, term->mouse_sel_start_row, term->mouse_sel_end_row);
        }
    }
#endif  // SFTE_INPUT_SELECTION
}

void sfte_mouse_scroll(sfte_ctx *ctx, int8_t dir, int32_t px_x, int32_t px_y) {
    int16_t c, screen_r;
    _sfte_grid_from_px(ctx, px_x, px_y, &c, NULL, &screen_r);

    if (ctx->term.mouse_mode) {
        uint8_t btn = (dir > 0) ? 64 : 65;
        _sfte_input_send_mouse_event(ctx, btn, 0, c, screen_r, 0);
    }

#if SFTE_TERM_SCROLLBACK_CAP
    sfte_view_scroll(ctx, (dir > 0) ? SFTE_TERM_SCROLL_STEP : -SFTE_TERM_SCROLL_STEP);
#endif  // SFTE_TERM_SCROLLBACK_CAP
}
#endif  // SFTE_INPUT_MOUSE

#if SFTE_INPUT_HYPERLINKS
const char *sfte_get_link_at(sfte_ctx *ctx, int16_t col, int16_t row) {
    if (col < 0 || col >= ctx->term.cols || row < 0 || row >= ctx->term.rows) return NULL;

    sfte_cell *c = &ctx->term.cells[row * ctx->term.cols + col];
    if (!c || c->link_idx == 0 || c->link_idx >= ctx->term.link_pool_len) return NULL;

    return ctx->term.link_pool[c->link_idx];
}
#endif  // SFTE_INPUT_HYPERLINKS

#if SFTE_INPUT_SELECTION
size_t sfte_get_selection(sfte_ctx *ctx, char *out_buf, size_t max_bytes) {
    if (!ctx->term.mouse_sel_active) return 0;

    int16_t sc = ctx->term.mouse_sel_start_col, sr = ctx->term.mouse_sel_start_row;
    int16_t ec = ctx->term.mouse_sel_end_col, er = ctx->term.mouse_sel_end_row;

    // Normalize if dragging backwards
    if (sr > er || (sr == er && sc > ec)) {
        int tmp = sr;
        sr = er, er = tmp;
        tmp = sc, sc = ec;
        ec = tmp;
    }

    size_t pos = 0;

#define _SFTE_WRITE_CHAR(c)                                                                        \
    do {                                                                                           \
        if (out_buf && pos < max_bytes - 1) out_buf[pos] = (c);                                    \
        pos++;                                                                                     \
    } while (0)

    for (int16_t r = sr; r <= er; ++r) {
        int16_t row_start = (r == sr) ? sc : 0;
        int16_t row_end = (r == er) ? ec : ctx->term.cols - 1;
        int32_t logical_r = _sfte_grid_vis2log(ctx, r);

        // Trim trailing spaces if the user selected past the end of text
        int16_t actual_end = row_end;
        if (actual_end == ctx->term.cols - 1) {
            while (actual_end >= row_start) {
                sfte_cell *vcell = _sfte_grid_get_cell(ctx, actual_end, logical_r);
                if (vcell->rune != ' ' && vcell->rune != 0) break;
                actual_end--;
            }
        }

        for (int16_t c = row_start; c <= actual_end; ++c) {
            sfte_cell *vcell = _sfte_grid_get_cell(ctx, c, logical_r);
            sfte_rune rune = (vcell->rune && vcell->rune != ' ') ? vcell->rune : ' ';

// UTF-8 encoding
#if SFTE_TERM_ASCII_CHARSET
            _SFTE_WRITE_CHAR(rune);
#else   // !SFTE_TERM_ASCII_CHARSET
            if (rune < 0x80) {
                _SFTE_WRITE_CHAR(rune);
            } else if (rune < 0x800) {
                _SFTE_WRITE_CHAR(0xC0 | (rune >> 6));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            } else if (rune < 0x10000) {
                _SFTE_WRITE_CHAR(0xE0 | (rune >> 12));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 6) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            } else {
                _SFTE_WRITE_CHAR(0xF0 | (rune >> 18));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 12) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | ((rune >> 6) & 0x3F));
                _SFTE_WRITE_CHAR(0x80 | (rune & 0x3F));
            }
#endif  // !SFTE_TERM_ASCII_CHARSET
        }

        // Inject newlines for multi-line selections, unless the line soft-wrapped
        if (r < er) {
#if SFTE_TERM_REFLOW
            if (!_sfte_grid_get_cell(ctx, ctx->term.cols - 1, logical_r)->wrapped)
#endif
                _SFTE_WRITE_CHAR('\n');
        }
    }
#undef _SFTE_WRITE_CHAR

    if (out_buf && max_bytes > 0) out_buf[pos < max_bytes ? pos : max_bytes - 1] = '\0';
    return pos + 1;
}
#endif  // SFTE_INPUT_SELECTION

#if SFTE_TERM_SCROLLBACK_CAP
void sfte_view_scroll(sfte_ctx *ctx, int32_t delta) {
#if SFTE_TERM_ALT_SCREEN
    if (ctx->term.alt_active) return;
#endif  // SFTE_TERM_ALT_SCREEN
    int32_t new_off = ctx->term.sb_offset + delta;
    if (new_off < 0) new_off = 0;
    int32_t max_scroll = ctx->term.sb_len < ctx->term.sb_cap ? ctx->term.sb_len : ctx->term.sb_cap;
    if (new_off > max_scroll) new_off = max_scroll;

    if (new_off != ctx->term.sb_offset) {
        ctx->term.sb_offset = new_off;
        int32_t top_logical_r = -ctx->term.sb_offset;
        int32_t bot_logical_r = top_logical_r + ctx->term.rows - 1;
        _sfte_grid_dirty_rows(ctx, top_logical_r, bot_logical_r);
    }
}
#endif  // SFTE_TERM_SCROLLBACK_CAP

#ifdef SFTE_XKB_COMMON
void sfte_xkb_process_key(sfte_ctx *ctx, struct xkb_state *state, uint32_t keycode) {
    if (!ctx || !state) return;

    xkb_keysym_t sym = xkb_state_key_get_one_sym(state, keycode);

    // Ignore standalone modifier keys
    if (sym >= XKB_KEY_Shift_L && sym <= XKB_KEY_Hyper_R) return;

    uint8_t ctrl = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE);
    uint8_t alt = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE);
    uint8_t shift = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT,
                                                 XKB_STATE_MODS_EFFECTIVE);
    uint8_t super = xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO,
                                                 XKB_STATE_MODS_EFFECTIVE);

    uint8_t active_mods = SFTE_MOD_NONE;
    if (ctrl) active_mods |= SFTE_MOD_CTRL;
    if (alt) active_mods |= SFTE_MOD_ALT;
    if (shift) active_mods |= SFTE_MOD_SHIFT;
    if (super) active_mods |= SFTE_MOD_SUPER;

    sfte_key key_id = SFTE_KEY_NONE;

    switch (sym) {
    case XKB_KEY_Tab:
    case XKB_KEY_ISO_Left_Tab: key_id = SFTE_KEY_TAB; break;
    case XKB_KEY_Return:
    case XKB_KEY_Linefeed:
    case XKB_KEY_KP_Enter: key_id = SFTE_KEY_ENTER; break;
    case XKB_KEY_BackSpace: key_id = SFTE_KEY_BACKSPACE; break;
    case XKB_KEY_Escape: key_id = SFTE_KEY_ESCAPE; break;
    case XKB_KEY_Up: key_id = SFTE_KEY_UP; break;
    case XKB_KEY_Down: key_id = SFTE_KEY_DOWN; break;
    case XKB_KEY_Left: key_id = SFTE_KEY_LEFT; break;
    case XKB_KEY_Right: key_id = SFTE_KEY_RIGHT; break;
    case XKB_KEY_Home: key_id = SFTE_KEY_HOME; break;
    case XKB_KEY_End: key_id = SFTE_KEY_END; break;
    case XKB_KEY_Page_Up: key_id = SFTE_KEY_PAGE_UP; break;
    case XKB_KEY_Page_Down: key_id = SFTE_KEY_PAGE_DOWN; break;
    case XKB_KEY_Insert: key_id = SFTE_KEY_INSERT; break;
    case XKB_KEY_Delete: key_id = SFTE_KEY_DELETE; break;
    case XKB_KEY_F1: key_id = SFTE_KEY_F1; break;
    case XKB_KEY_F2: key_id = SFTE_KEY_F2; break;
    case XKB_KEY_F3: key_id = SFTE_KEY_F3; break;
    case XKB_KEY_F4: key_id = SFTE_KEY_F4; break;
    case XKB_KEY_F5: key_id = SFTE_KEY_F5; break;
    case XKB_KEY_F6: key_id = SFTE_KEY_F6; break;
    case XKB_KEY_F7: key_id = SFTE_KEY_F7; break;
    case XKB_KEY_F8: key_id = SFTE_KEY_F8; break;
    case XKB_KEY_F9: key_id = SFTE_KEY_F9; break;
    case XKB_KEY_F10: key_id = SFTE_KEY_F10; break;
    case XKB_KEY_F11: key_id = SFTE_KEY_F11; break;
    case XKB_KEY_F12: key_id = SFTE_KEY_F12; break;
    default: key_id = SFTE_KEY_NONE; break;
    }

    if (key_id != SFTE_KEY_NONE)
        // Mapped control key
        sfte_input_key(ctx, key_id, active_mods);
    else {
        uint32_t cp = xkb_keysym_to_utf32(sym);

        if (cp > 0 && (active_mods & (SFTE_MOD_CTRL | SFTE_MOD_ALT | SFTE_MOD_SUPER)))
            // Modified text
            sfte_input_key(ctx, (sfte_key)cp, active_mods);
        else {
            // Pure typing
            char buf[64];
            int8_t size = (int8_t)xkb_state_key_get_utf8(state, keycode, buf, sizeof(buf));
            if (size > 0) sfte_input_text(ctx, buf, size);
        }
    }
}
#endif  // SFTE_XKB_COMMON

#if SFTE_TERM_FOCUS
void sfte_set_focus(sfte_ctx *ctx, uint8_t focused) {
    if (!ctx || ctx->term.is_focused == focused) return;

    ctx->term.is_focused = focused;

#if SFTE_CURSOR_BLINK
    // Force the cursor to be visible when changing focus states
    ctx->term.blink_visible = 1;
    ctx->term.next_blink_ms = SFTE_TIME_MS() + SFTE_CURSOR_BLINK_RATE_MS;
#endif  // SFTE_CURSOR_BLINK

    int16_t vis_col = ctx->term.cursor_col >= ctx->term.cols ? ctx->term.cols - 1
                                                             : ctx->term.cursor_col;
    ctx->term.cells[_sfte_grid_get_idx(ctx, vis_col, ctx->term.cursor_row)].dirty = 1;

    if (ctx->term.report_focus && ctx->write_cb) {
        if (focused)
            ctx->write_cb(ctx->user_data, "\033[I", 3);
        else
            ctx->write_cb(ctx->user_data, "\033[O", 3);
    }
}
#endif  // SFTE_TERM_FOCUS

// =================================================================================================
// >>wayland backend
// =================================================================================================
#if SFTE_WAYLAND
sfte_wayland_app *sfte_wayland_init(void) {
    sfte_wayland_app *app = (sfte_wayland_app *)SFTE_CALLOC(1, sizeof(sfte_wayland_app));
    app->running = 1;
    app->ctx = sfte_init(_sfte_wayland_write_cb, app);

#if SFTE_CLIPBOARD && SFTE_INPUT_SELECTION && SFTE_CLIPBOARD_OSC52
    app->ctx->osc52_clipboard_cb = _sfte_wayland_osc52_clipboard_cb;
#endif  // SFTE_CLIPBOARD && SFTE_INPUT_SELECTION && SFTE_CLIPBOARD_OSC52
#if SFTE_INPUT_HYPERLINKS
    app->ctx->open_link_cb = _sfte_wayland_open_link_cb;
#endif  // SFTE_INPUT_HYPERLINKS
    app->ctx->title_cb = _sfte_wayland_title_cb;

    _sfte_wayland_pty_spawn(app);
    _sfte_wayland_load(app);

    return app;
}

sfte_ctx *sfte_wayland_get_ctx(sfte_wayland_app *app) {
    return app->ctx;
}

int sfte_wayland_run(sfte_wayland_app *app) {
#ifdef SFTE_FONT_BOLD
    SFTE_ASSERT(app->ctx->font.bold.glyphs && app->ctx->font.bold.atlas_pxs,
                "if SFTE_FONT_BOLD is defined, a bold font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    SFTE_ASSERT(app->ctx->font.italic.glyphs && app->ctx->font.italic.atlas_pxs,
                "if SFTE_FONT_ITALIC is defined, an italic font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    SFTE_ASSERT(app->ctx->font.bold_italic.glyphs && app->ctx->font.bold_italic.atlas_pxs,
                "if SFTE_FONT_BOLD_ITALIC is defined, a bold italic font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_BOLD_ITALIC

    int32_t ideal_w, ideal_h;
    sfte_get_ideal_size(app->ctx, SFTE_TERM_INIT_COLS, SFTE_TERM_INIT_ROWS, &ideal_w, &ideal_h);
    app->width = ideal_w;
    app->height = ideal_h;
    sfte_resize(app->ctx, app->width, app->height);
    if (!app->buffer) _sfte_wayland_create_buffer(app);
    _sfte_wayland_pty_update(app);

    _sfte_wayland_loop(app);

    _sfte_wayland_unload(app);
    sfte_free(app->ctx);
    SFTE_FREE(app);
    return 0;
}
#endif  // SFTE_WAYLAND

// =================================================================================================
// >>win32 backend
// =================================================================================================
#if SFTE_WIN32
sfte_win32_app *sfte_win32_init(void) {
    sfte_win32_app *app = (sfte_win32_app *)SFTE_CALLOC(1, sizeof(sfte_win32_app));
    SFTE_ASSERT(app, "failed to allocate win32 app");

    app->running = 1;
    app->hinstance = GetModuleHandleW(NULL);
    app->ctx = sfte_init(_sfte_win32_write_cb, app);

#if SFTE_CLIPBOARD && SFTE_INPUT_SELECTION && SFTE_CLIPBOARD_OSC52
    app->ctx->osc52_clipboard_cb = _sfte_win32_osc52_clipboard_cb;
#endif  // SFTE_CLIPBOARD && SFTE_INPUT_SELECTION && SFTE_CLIPBOARD_OSC52
#if SFTE_INPUT_HYPERLINKS
    app->ctx->open_link_cb = _sfte_win32_open_link_cb;
#endif  // SFTE_INPUT_HYPERLINKS
    app->ctx->title_cb = _sfte_win32_title_cb;

    app->shell_cmdline = _sfte_win32_resolve_shell();
    _sfte_win32_setup_dpi(app);

    return app;
}

sfte_ctx *sfte_win32_get_ctx(sfte_win32_app *app) {
    return app->ctx;
}

void sfte_win32_set_shell(sfte_win32_app *app, const char *cmdline) {
    if (!app || !cmdline) return;
    if (app->shell_cmdline) SFTE_FREE(app->shell_cmdline);
    app->shell_cmdline = _sfte_win32_dup(cmdline);
}

int sfte_win32_run(sfte_win32_app *app) {
#ifdef SFTE_FONT_BOLD
    SFTE_ASSERT(app->ctx->font.bold.glyphs && app->ctx->font.bold.atlas_pxs,
                "if SFTE_FONT_BOLD is defined, a bold font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_BOLD
#ifdef SFTE_FONT_ITALIC
    SFTE_ASSERT(app->ctx->font.italic.glyphs && app->ctx->font.italic.atlas_pxs,
                "if SFTE_FONT_ITALIC is defined, an italic font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_ITALIC
#ifdef SFTE_FONT_BOLD_ITALIC
    SFTE_ASSERT(app->ctx->font.bold_italic.glyphs && app->ctx->font.bold_italic.atlas_pxs,
                "if SFTE_FONT_BOLD_ITALIC is defined, a bold italic font must be provided using "
                "sfte_font_load_*");
#endif  // SFTE_FONT_BOLD_ITALIC

    // Fonts are loaded by now; scale the default size to the system DPI before sizing the window.
    _sfte_win32_apply_dpi(app, app->dpi);

    int32_t ideal_w, ideal_h;
    sfte_get_ideal_size(app->ctx, SFTE_TERM_INIT_COLS, SFTE_TERM_INIT_ROWS, &ideal_w, &ideal_h);
    app->width = ideal_w;
    app->height = ideal_h;
    sfte_resize(app->ctx, app->width, app->height);

    _sfte_win32_register_class(app);

    RECT rect = {0, 0, app->width, app->height};
    AdjustWindowRectExForDpi(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0, app->dpi);
    HWND hwnd = CreateWindowExW(0, L"sfte_win32", L"sfte", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL,
                                NULL, app->hinstance, app);
    if (!hwnd) {
        _sfte_win32_unload(app);
        sfte_free(app->ctx);
        SFTE_FREE(app);
        return 1;
    }

    _sfte_win32_apply_dark_title(hwnd);
    _sfte_win32_create_backbuffer(app);

    _sfte_win32_pty_spawn(app);
    if (!app->hpc) {
        _sfte_win32_unload(app);
        sfte_free(app->ctx);
        SFTE_FREE(app);
        return 1;
    }
    _sfte_win32_pty_update(app);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    app->needs_render = 1;

    _sfte_win32_loop(app);

    _sfte_win32_unload(app);
    sfte_free(app->ctx);
    SFTE_FREE(app);
    return 0;
}
#endif  // SFTE_WIN32
#endif  // SFTE_IMPL
