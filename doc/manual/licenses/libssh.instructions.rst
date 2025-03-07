.. _appendix.license.libssh.instructions:

Build instructions for libssh
-----------------------------

The OVITO package includes a binary version of the libssh library licensed under the GNU Lesser General Public License (LGPLv2.1).
In accordance with the requirements of this license, this page provides instructions on how to rebuild a compatible version of the library from source code.

Windows
"""""""

OVITO for Windows includes binaries that have been built from the unmodified sources of libssh 0.11.1.
The following commands have been used to generate them::

  # Compiler: Microsoft Visual C++ 2022 (command line tools)
  # OpenSSL version: 3.0.16
  # zlib version: 1.3.1
  cd libssh-0.11.1
  mkdir build
  cd build
  cmake -G Ninja ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DCMAKE_INSTALL_PREFIX=../../libssh ^
    -DZLIB_INCLUDE_DIR=%cd:\=/%/../../zlib/include ^
    -DZLIB_LIBRARY=%cd:\=/%/../../zlib/lib/zlib.lib ^
    -DWITH_SERVER=OFF ^
    -DWITH_GSSAPI=OFF ^
    -DWITH_EXAMPLES=OFF ^
    -DOPENSSL_ROOT_DIR=%cd:\=/%/../../openssl ^
    ..
  cmake --build . --config Release --target install

Linux
"""""

OVITO for Linux includes a shared library that has been built from the unmodified sources of libssh 0.11.1.
The following commands were used to build it::

  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DWITH_SERVER=OFF .
  cmake --build . --parallel
  cmake --install .

macOS
"""""

OVITO for macOS includes a shared library that has been built from the unmodified sources of libssh 0.10.6.
The following commands were used to build it::

  export OPENSSL_ROOT_DIR=...
  cmake -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DWITH_SERVER=OFF \
      -DWITH_EXAMPLES=OFF \
      -DWITH_PKCS11_URI=ON \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      ..
  cmake --build . --parallel && cmake --install .
