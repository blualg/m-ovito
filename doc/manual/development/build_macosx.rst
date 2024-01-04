.. _development.build_macosx:

Building OVITO on macOS
=======================

Installing dependencies
-----------------------

See the :ref:`list of requirements <development.requirements>` and install the required build tools and third-party libraries. OVITO should be compiled with
Apple's clang C++ compiler shipping with Xcode. It's easiest to use `MacPorts <https://www.macports.org/>`_ for installing many of the required dependencies.

After setting up MacPorts, run::

  sudo port install netcdf pzlib libssh boost cmake yasm

from the terminal to install the required dependencies of OVITO.

Next, download and install `Qt 6 for Mac <https://www.qt.io/download/>`_.

Next, download the source code and build the shared version of the `ffmpeg <https://ffmpeg.org/>`_ video encoding library (optional)::

  curl -O https://ffmpeg.org/releases/ffmpeg-4.2.8.tar.gz
  tar xzfv ffmpeg-4.2.8.tar.gz
  cd ffmpeg-4.2.1
  ./configure \
    --disable-network \
    --disable-programs \
    --disable-debug \
    --disable-doc \
    --disable-static \
    --enable-shared \
    --prefix=$HOME/ffmpeg
  make install

Downloading the source code
---------------------------

To download OVITO's source code into a new directory named :file:`ovito/` run::

  git clone --recursive https://gitlab.com/stuko/ovito.git

Compiling OVITO
---------------

Within the source directory, create a build sub-directory and let CMake generate the Makefile::

  cd ovito
  mkdir build
  cd build
  cmake -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=../install \
      -DCMAKE_PREFIX_PATH=`echo $HOME/Qt/6.*.*/clang_64/` \
      -DFFMPEG_INCLUDE_DIR=$HOME/ffmpeg/include \
      -DFFMPEG_LIBRARY_DIR=$HOME/ffmpeg/lib \
      ..

Adjust the paths in the command above as needed.
If this step fails, or if you want to disable individual components of OVITO, you can now run :command:`ccmake .` to
open the CMake configuration program and make changes to the build settings.
Once you are done, build OVITO by running ::

  cmake --build . --parallel

If this step succeeds, you can run :command:`cmake install .` to generate an app bundle in the :file:`ovito/install/` directory.
