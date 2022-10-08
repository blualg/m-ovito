.. _development.build_windows:

Building OVITO on Windows
===============================

The required steps are quite involved, particularly those for building/installing the prerequisites, 
and the available instructions may be insufficient. Please get in touch with the developers if you want to 
build OVITO on the Windows platform.

You are going to need at least the following prerequisites:

* Microsoft Visual Studio 2019 or newer (64-bit command line tools for C++ development)
* Qt 6.x (install the `msvc2019 64-bit` component)
* CMake (v3.12 or newer)
* git
* `zlib <http://www.zlib.net/>`_ 
* `Boost <https://www.boost.org>`_ (header-only component)
* HDF5 (optional, requires zlib)
* NetCDF (optional, requires HDF5 and zlib)
* ffmpeg (optional, version 4.2.1, requires MSYS2)

Downloading the source code
---------------------------

To download OVITO's source code into a new subdirectory named :file:`ovito/`, install `Git <https://git-scm.com>`_ and run::

  git clone https://gitlab.com/stuko/ovito.git

from the command line or download the source tree package from https://gitlab.com/stuko/ovito.