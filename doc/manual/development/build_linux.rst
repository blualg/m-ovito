.. _development.build_linux:

Building OVITO on Linux
=======================

The following instructions have been written for Ubuntu Linux 22.04 and compatible distributions.

Installing dependencies
-----------------------

First, install the required :ref:`build tools and third-party libraries <development.requirements>`
as follows:

.. note::

  OVITO requires the Qt cross-platform framework (version 6.2 or higher). We recommend using the newest release of the Qt
  framework, which is available as a download from https://www.qt.io/download. Alternatively,
  you can use the Qt6 development files provided by the package manager of your Linux distro.

Ubuntu / Debian:
  .. code-block:: shell

          sudo apt-get install build-essential git cmake-curses-gui qt6-base-dev libqt6svg6-dev \
                libboost-dev libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev \
                libavutil-dev libswscale-dev libnetcdf-dev libhdf5-dev libhdf5-serial-dev \
                libglu1-mesa-dev ninja-build \
                libssh-dev python3-sphinx python3-sphinx-rtd-theme

openSUSE:
  .. code-block:: shell

          sudo zypper install git cmake gcc-c++ qt6-concurrent-devel qt6-core-devel qt6-gui-devel \
                 qt6-network-devel qt6-dbus-devel qt6-opengl-devel qt6-printsupport-devel \
                 qt6-widgets-devel qt6-xml-devel qt6-svg-devel libavutil-devel libavresample-devel \
                 libavfilter-devel libavcodec-devel libavdevice-devel netcdf-devel libssh-devel \
                 boost-devel hdf5-devel libswscale-devel ninja

Fedora:
  .. code-block:: shell

          # Activate the RPMfusion repository providing the ffmpeg package (optional):
          sudo dnf install \
           https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
           https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm

          sudo dnf install git cmake g++ qt6-qtbase-devel qt6-qtsvg-devel boost-devel zlib-devel ninja-build \
                           ffmpeg-devel netcdf-devel libssh-devel python3-sphinx python3-sphinx_rtd_theme

RHEL / CentOS:
  .. code-block:: shell

          sudo yum install epel-release
          sudo yum install git gcc gcc-c++ cmake qt6-qtbase-devel qt6-qtsvg-devel qt6-qttools-devel \
                               boost-devel netcdf-devel hdf5-devel libssh-devel

  These RHEL/CentOS packages allow building only a basic version of OVITO without video encoding support and documentation.
  In order to build a complete version, other :ref:`dependencies <development.requirements>` must be installed manually.

Getting the source code
-----------------------

Download the source repository of OVITO into a new subdirectory named :file:`ovito/`::

  git clone --recursive https://gitlab.com/stuko/ovito.git

Compiling OVITO
---------------

Create a build directory and let `CMake <https://www.cmake.org/>`_ generate a Makefile::

  cd ovito
  mkdir build
  cd build
  cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..

If this step fails, you can now run :command:`ccmake .` to start up the
`CMake <https://www.cmake.org/>`_ configuration program and adjust the build options as needed.

To build OVITO run::

  cmake --build . --parallel

If this step is successful, the :program:`ovito` executable can be found in the directory :file:`ovito/build/bin/`.
The command :command:`cmake --build . --target documentation` builds the HTML pages of the user manual (requires Sphinx Python package and Sphinx RTD theme).