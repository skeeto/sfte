<img width="384" height="128" alt="sfte gif logo" src="https://github.com/user-attachments/assets/e148269e-1301-4d18-9d14-ede0a413e4ef" />

---

<div align="center"><img src="showcase.gif" width="800"></div>

---

<div align="center">single-file terminal emulator (library) written in C11.</div>

## Features

- **Lightweight** - as low as 60KB binary size with <10MB RAM usage
- **Customizable** - over 80 configurable macros that actually affect performance
- **One-size-fits-all** - can be used on everything from microcontrollers to daily-driven machines
- ***(Almost)* dependency-free** - dependencies are minimized with crucial ones being inlined.
- **Capable** - supports Kitty graphics, Sixel images, True Color, font fallbacks, styled and colored underlines.
- **Smooth** - supports multiple 'eye-candy' features: buffer transitions, cursor trail, etc.

## Platforms

**sfte** ships with two native backends:

- **Wayland** (Linux),
- **Win32** (Windows, using ConPTY).

That being said, custom backends are supported, allowing it to be ported to anything as easily as possible.

Planned supported platforms are:

- macOS,
- X11 for legacy support,
- [sokol](https://github.com/floooh/sokol) for easy game engine embedding.

## Installing

### Linux (Wayland)

Prerequisites:

- Any C compiler supporting C11+ standard
- Development headers for `wayland-client` and `xkbcommon`

Getting started:

1. Clone the repository.

```sh
git clone https://github.com/nihiL7331/sfte.git --depth=1
cd sfte
```

2. Compile the build binary (nob).

```sh
cc nob.c -o nob
```

3. Run the build binary with the `install` argument.

```sh
./nob install
```

### Windows (Win32)

Prerequisites:

- A GCC/MinGW-w64 toolchain supporting C11+ (e.g. [w64devkit](https://github.com/skeeto/w64devkit)) on `PATH`
- CMake 3.20+ (with Ninja or MinGW Makefiles)

Getting started:

1. Clone the repository and configure the build. On the first configure, `config.def.win32.c` is copied to `config.c`.

```sh
git clone https://github.com/nihiL7331/sfte.git --depth=1
cd sfte
cmake -S . -B build -G Ninja
cmake --build build
```

2. Run `build/sfte.exe`.

The shell is resolved in this order: `SFTE_SHELL`, `ash.exe`, `powershell.exe`, `COMSPEC`, `cmd.exe`.
Override it from `config.c` with `sfte_win32_set_shell(app, "...")` or via the `SFTE_SHELL` environment variable.

### Windows font rendering (DirectWrite)

The default Windows config rasterizes glyphs with DirectWrite, so text honors the system
ClearType configuration (antialiasing mode, gamma, enhanced contrast and hinting).

- `SFTE_FONT_SUBPIXEL` (`1` by default) selects ClearType subpixel coverage; set it to `0` for grayscale.
- `SFTE_DWRITE_ANTIALIAS`: `0` follows the system ClearType setting, `1` forces ClearType, `2` forces grayscale.
- `SFTE_DWRITE_RENDERING_MODE`: `0` GDI classic (default), `1` GDI natural, `2` natural, `3` natural symmetric.
- `SFTE_DWRITE_SWAP_RB`: `1` swaps the red/blue coverage channels.

Remove the `SFTE_FONT_CUSTOM_BACKEND` block from `config.c` to fall back to the built-in `stb_truetype` rasterizer.

## Customization

**sfte** relies heavily on customization.
By default, it comes with minimal, sensible configuration.

It is configured by defining macros inside `config.c`, taking inspiration from `st`.
When you build the project for the first time, a `config.c` is generated.
A complete description of all override macros can be found directly inside `sfte.h`.

When any changes are done, **sfte** needs to be recompiled for it to apply.

### Why handle the customization this way?

It's extremely simple. No parsing is needed, and everything is handled during the compilation time.
It allows the compiler to strip away any disabled portions of code, maximizing the performance.
The terminal only does what it has to do, nothing more.

It also fits the single-file premise of this project, to compile and run **sfte** all that is needed
are the prerequisites, `nob` binary, `sfte.h` and `config.c`. There's no massive 100MB artifact,
so everything e.g. can be stored in a dotfiles repository.

## Dependencies

**sfte** tries to use as few dependencies as possible.
There are still a couple of mandatory ones, which were hard to get rid of, hence they got inlined.

- [`stb_truetype.h`](https://github.com/nothings/stb/blob/master/stb_truetype.h)
- [`stb_image.h`](https://github.com/nothings/stb/blob/master/stb_image.h)

## License

zlib license.
