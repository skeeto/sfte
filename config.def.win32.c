/*
    config.def.win32.c - default Windows configuration template for sfte

    NOTE:
    This file is automatically copied to config.c on the first CMake configure.
    Edit config.c to customize your terminal settings and font paths.
    To apply a change, just rebuild the project.
*/

// #define SFTE_COLOR_BG_OPACITY 0xEE
// #define SFTE_CURSOR_TRAIL 10
#define SFTE_FONT_BOLD
#define SFTE_FONT_ITALIC
#define SFTE_FONT_BOLD_ITALIC
// ... other options

#define SFTE_IMPL
#ifdef SFTE_DEV_ENV
#include "sfte_dev.h"
#else  // !SFTE_DEV_ENV
#include "sfte.h"
#endif  // !SFTE_DEV_ENV

int main(void) {
    sfte_win32_app *app = sfte_win32_init();
    sfte_ctx *ctx = sfte_win32_get_ctx(app);

    // The shell is resolved as: SFTE_SHELL -> ash.exe -> powershell.exe -> COMSPEC -> cmd.exe.
    // Override it explicitly with: sfte_win32_set_shell(app, "C:/Windows/System32/cmd.exe");

    sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "C:/Windows/Fonts/consola.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD, "C:/Windows/Fonts/consolab.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_ITALIC, "C:/Windows/Fonts/consolai.ttf");
    sfte_font_load_file(ctx, SFTE_FONT_STYLE_BOLD_ITALIC, "C:/Windows/Fonts/consolaz.ttf");

    // Optional fallback for nerd symbols:
    // sfte_font_load_file(ctx, SFTE_FONT_STYLE_REGULAR, "C:/path/to/SymbolsNerdFontMono-Regular.ttf");

    return sfte_win32_run(app);
}