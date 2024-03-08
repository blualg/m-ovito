.. _appendix.license.ffmpeg.instructions:

Build instructions for ffmpeg
-----------------------------

The OVITO package includes binary versions of the ffmpeg libraries licensed under the GNU Lesser General Public License (LGPLv2.1).
In accordance with the requirements of this license, this page provides instructions on how to rebuild compatible versions of these libraries from source code.

Windows
"""""""

OVITO for Windows includes binaries that have been built from the unmodified sources of ffmpeg 6.1.1.
The following commands have been used to generate them::

  # Compiler: Microsoft Visual C++ 2019 (command line tools) + MSYS2 environment
  # zlib version: 1.3.1
  ./configure \
    --toolchain=msvc \
    --target-os=win64 \
    --arch=x86_64 \
    --disable-programs \
    --disable-static \
    --enable-shared \
    --prefix=../../ffmpeg \
    --extra-cflags=-I$PWD/../zlib/include  \
    --extra-ldflags=-LIBPATH:$PWD/../zlib/lib \
    --enable-zlib \
    --disable-doc \
    --disable-network \
    --disable-debug \
    --disable-decoders \
    --disable-indevs \
    --disable-postproc \
    --disable-sdl2 \
    --disable-libxcb \
    --disable-libxcb-shm \
    --disable-libxcb-xfixes \
    --disable-libxcb-shape \
    --disable-iconv
  make install

Linux
"""""

OVITO for Linux includes shared libraries that have been built from the unmodified sources of ffmpeg 6.1.1.
The following commands have been used to generate them::

  # Build platform: CentOS 7
  # Compiler: GCC 10
  ./configure \
    --enable-pic \
    --enable-shared \
    --disable-static \
    --disable-doc \
    --disable-network \
    --disable-programs \
    --disable-debug \
    --disable-decoders \
    --disable-indevs \
    --disable-postproc \
    --disable-sdl2 \
    --prefix=$HOME/ffmpeg
  make install

macOS
"""""

OVITO for macOS includes shared libraries that have been built from the unmodified sources of ffmpeg 6.0.
The following commands have been used to generate them::

  git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg_source
  cd ffmpeg_source
  git checkout n6.0
  ./configure \
    --disable-network \
    --disable-programs \
    --disable-debug \
    --disable-doc \
    --disable-static \
    --disable-decoders \
    --disable-indevs \
    --disable-postproc \
    --disable-sdl2 \
    --disable-libxcb \
    --disable-libxcb-shm \
    --disable-libxcb-xfixes \
    --disable-libxcb-shape \
    --disable-iconv \
    --disable-bzlib \
    --disable-zlib \
    --disable-xlib \
    --enable-shared \
    --prefix=$HOME/progs/ffmpeg
  make install
