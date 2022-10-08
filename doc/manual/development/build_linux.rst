.. _development.build_linux:

Building OVITO on Linux
=============================

The following instructions have been written for Ubuntu Linux 20.04 and compatible Debian-based distributions.
Other Linux distributions may require slightly different build steps.

Installing dependencies
-----------------------

First, install the required :ref:`build tools and third-party libraries <development.requirements>`:

.. list-table::
   :width: 100%
   :widths: auto
   :header-rows: 1

   * - Linux distribution
     - Commands
   * - Ubuntu / Debian
     - .. code::

          sudo apt-get install build-essential git cmake-curses-gui qt5-default libqt5svg5-dev \
                libboost-dev libavcodec-dev libavdevice-dev libavfilter-dev libavformat-dev \
                libavresample-dev libavutil-dev libswscale-dev libnetcdf-dev libhdf5-dev \
                libhdf5-serial-dev libssh-dev python3-sphinx python3-sphinx-rtd-theme

   * - openSUSE
     - .. code::
          
          sudo zypper install git cmake gcc-c++ libQt5Concurrent-devel libQt5Core-devel libQt5Gui-devel \
                 libQt5Network-devel libQt5DBus-devel libQt5OpenGL-devel libQt5PrintSupport-devel \
                 libQt5Widgets-devel libQt5Xml-devel libQt5Svg-devel libavutil-devel libavresample-devel \
                 libavfilter-devel libavcodec-devel libavdevice-devel netcdf-devel libssh-devel \
                 boost-devel hdf5-devel libswscale-devel

   * - Fedora
     - .. code::
          
          # Activate the RPMfusion repository providing the ffmpeg package (optional):
          sudo dnf install \
           https://download1.rpmfusion.org/free/fedora/rpmfusion-free-release-$(rpm -E %fedora).noarch.rpm \
           https://download1.rpmfusion.org/nonfree/fedora/rpmfusion-nonfree-release-$(rpm -E %fedora).noarch.rpm
          
          sudo dnf install git cmake g++ qt5-qtbase-devel qt5-qtsvg-devel boost-devel zlib-devel \
                           ffmpeg-devel netcdf-devel libssh-devel python3-sphinx python3-sphinx_rtd_theme

   * - CentOS / RHEL
     - .. code::
       
          sudo yum install epel-release
          sudo yum install git gcc gcc-c++ cmake qt5-qtbase-devel qt5-qtsvg-devel qt5-qttools-devel \
                               boost-devel netcdf-devel hdf5-devel libssh-devel

       These packages allow building only a basic version of OVITO without video encoding support and documentation.
       In order to build a complete version, other :ref:`dependencies <development.requirements>` must be installed manually.

Getting the source code
-----------------------

Download the source repository of OVITO into a new subdirectory named :file:`ovito/`::

  git clone https://gitlab.com/stuko/ovito.git

Compiling OVITO
---------------

Create a build directory and let `CMake <https://www.cmake.org/>`_ generate a Makefile::

  cd ovito
  mkdir build
  cd build
  cmake -DCMAKE_BUILD_TYPE=Release ..

If this step fails, you can now run :command:`ccmake .` to start up the
`CMake <https://www.cmake.org/>`_ configuration program and adjust the build options as needed.

To build OVITO run::

  make -j4

If this step is successful, the :program:`ovito` executable can be found in the directory :file:`ovito/build/bin/`.
The command :command:`make documentation` builds the HTML pages of the user manual (requires Sphinx Python package and Sphinx RTD theme).