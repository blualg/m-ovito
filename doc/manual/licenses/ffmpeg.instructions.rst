.. _appendix.license.ffmpeg.instructions:

Build instructions for ffmpeg
-----------------------------

The OVITO package includes binary versions of the ffmpeg libraries licensed under the GNU Lesser General Public License (LGPLv2.1).
In accordance with the requirements of this license, this page provides instructions on how to rebuild compatible versions of these libraries from source code.

Windows
"""""""

OVITO for Windows includes binaries that have been built from the unmodified sources of ffmpeg 4.2.1.
The following commands have been used to generate them::

  # Compiler: Microsoft Visual C++ 2019 (command line tools) + MSYS2 environment
  # Zlib version: 1.2.11
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
    --disable-debug
  make install

Linux
"""""

OVITO for Linux includes shared libraries that have been built from the unmodified sources of ffmpeg 4.2.1.
The following commands have been used to generate them::

  # Build platform: CentOS 6.9
  # Compiler: GCC 7.1 (CentOS devtoolset-7)
  ./configure \
    --enable-pic \
    --enable-shared \
    --disable-static \
    --disable-doc \
    --disable-network \
    --disable-programs \
    --disable-debug \
    --prefix=$HOME/progs/ffmpeg
  make install

macOS
"""""

OVITO for macOS includes shared libraries that have been built from the unmodified sources of ffmpeg 4.2.8.
The following commands have been used to generate them::

  git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg_source
  cd ffmpeg_source
  git checkout n4.2.8
  export MACOSX_DEPLOYMENT_TARGET=10.14
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
    --enable-shared \
    --prefix=$HOME/progs/ffmpeg
  make install
