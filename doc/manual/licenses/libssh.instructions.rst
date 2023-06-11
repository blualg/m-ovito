.. _appendix.license.libssh.instructions:

Build instructions for libssh
-----------------------------

The OVITO package includes a binary version of the libssh library licensed under the GNU Lesser General Public License (LGPLv2.1).
In accordance with the requirements of this license, this page provides instructions on how to rebuild a compatible version of the library from source code.

Windows
"""""""

OVITO for Windows includes binaries that have been built from the unmodified sources of libssh 0.10.4.
The following commands have been used to generate them::

  # Compiler: Microsoft Visual C++ 2019 (command line tools)
  # OpenSSL version: 1.1.1t
  # Zlib version: 1.2.13
  cd libssh-0.10.4
  mkdir build
  cd build
  cmake -G "NMake Makefiles" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=../../libssh ^
    -DZLIB_INCLUDE_DIR=%cd:\=/%/../../zlib/include ^
    -DZLIB_LIBRARY=%cd:\=/%/../../zlib/lib/zlib.lib ^
    -DWITH_SERVER=OFF ^
    -DWITH_GSSAPI=OFF ^
    -DWITH_EXAMPLES=OFF ^
    -DOPENSSL_ROOT_DIR=%cd:\=/%/../../openssl ^
    ..
  nmake install

Linux
"""""

OVITO for Linux includes a shared library that has been built from the unmodified sources of libssh 0.10.5.
The following commands were used to build it::

  # Build platform: CentOS 7
  # Compiler: GCC 10
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DWITH_SERVER=OFF .
  cmake --build . --parallel
  cmake --install .

macOS
"""""

OVITO for amcOS includes a shared library that has been built from the unmodified sources of libssh 0.10.4.
The following commands were used to build it::

  export OPENSSL_ROOT_DIR=...
  cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DWITH_SERVER=OFF \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
      ..
  cmake --build . --parallel && cmake --install .
