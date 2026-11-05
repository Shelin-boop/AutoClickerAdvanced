# Third-party notices

## XClicker (origin of this program)

AutoClicker Advanced is a modified version of XClicker 1.5.1 by robiot and the
XClicker contributors, <https://github.com/robiot/xclicker>, licensed under
the GNU General Public License v3.0 (see `LICENSE`). The changes are listed in
`NOTICE.md`. AutoClicker Advanced is not an official XClicker release and is
not affiliated with or endorsed by the XClicker project.

The icon of AutoClicker Advanced (`assets/icon.svg`) is an original drawing;
it does not use the XClicker icon or the XClicker design files.

## Libraries used by the program

The program is linked dynamically against libraries of the operating system:
GTK 3, GLib, GdkPixbuf, cairo, Xlib, libXi and libXext. They are not part of
this repository or of the AppImage and keep their own licenses.

## AppImage runtime (only in the AppImage file)

The AppImage file starts with the AppImage type 2 runtime, a separate program
that mounts the image and starts AutoClicker Advanced. It is not part of the
program's source code. The runtime used for the release files is the
"continuous" build of <https://github.com/AppImage/type2-runtime>, commit
`fedfad5` (2026-10-19), file `runtime-x86_64`, SHA-256
`617aa77314d57c8b8e4fae9c3f0b35af7b30d5529cb69e98f64f92510ed98a46`.
Its source code and build scripts are available from that repository; the
runtime prints its own license notice with `--appimage-help`.

The runtime is statically linked with the components below. License texts are
in the folder `licenses/` (also inside the AppImage under
`usr/share/doc/autoclicker-advanced/licenses/`).

| Component | License | Source | License text |
|---|---|---|---|
| type2-runtime | MIT | <https://github.com/AppImage/type2-runtime> | `licenses/type2-runtime.txt` |
| musl libc | MIT | <https://musl.libc.org/> | `licenses/musl.txt` |
| libfuse 3.15.0 (patched by the runtime project) | LGPL-2.1 | <https://github.com/libfuse/libfuse/releases/tag/fuse-3.15.0>, patch in the runtime repository under `patches/libfuse` | `licenses/libfuse-LGPL-2.1.txt` |
| squashfuse 0.5.2 | BSD-2-Clause | <https://github.com/vasi/squashfuse/releases/tag/0.5.2> | `licenses/squashfuse.txt` |
| Zstandard (libzstd) | BSD-3-Clause (dual-licensed with GPL-2.0) | <https://github.com/facebook/zstd> | `licenses/zstd.txt` |
| zlib | zlib License | <https://zlib.net/> | `licenses/zlib.txt` |
| mimalloc | MIT | <https://github.com/microsoft/mimalloc> | `licenses/mimalloc.txt` |

The versions of musl, Zstandard, zlib and mimalloc are those of Alpine Linux
at the time the runtime was built; the runtime's build scripts name the
packages. To rebuild the runtime with a modified libfuse, follow `BUILD.md`
in the runtime repository.
