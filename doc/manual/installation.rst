.. _installation:

============
Installation
============

.. _installation.requirements:

System requirements
===================

The *OVITO Basic* and *OVITO Pro* desktop applications require a system supporting the `OpenGL 3D graphics interface <https://en.wikipedia.org/wiki/OpenGL>`__ (version 2.1 or newer).
For optimal performance, it is recommended to install the latest graphics driver from your hardware vendor, as older drivers may cause compatibility or stability issues.

Operating System Compatibility:

Windows:
  64-bit Windows 10 (21H2 or later), Windows 11 (21H2 or later) on x86_64 processor architecture.
Linux:
  Ubuntu 21.04+, ALT Linux 10+, RHEL 9+, Debian 11+, Fedora 34+, or compatible distributions with glibc >= 2.28, running on x86_64 processors.
macOS:
  macOS 11.0 or newer, supporting both Apple Silicon (arm64) and Intel architectures.

.. _installation.instructions:

Installation instructions
=========================

Download a binary program package for *OVITO Basic* or *OVITO Pro* from `www.ovito.org <https://www.ovito.org/#download>`__.

*Linux*:
    Extract the downloaded `.tar.xz` archive using the tar utility: :command:`tar xJfv ovito-{{OVITO_VERSION_STRING}}-x86_64.tar.xz`.
    This creates a subdirectory containing the program files. Change to that directory and run the application: :command:`./bin/ovito`.

*Windows*:
    Run the installer program :file:`ovito-{{OVITO_VERSION_STRING}}-win64.exe` to install OVITO in a directory of your choice.
    Follow the on-screen instructions to install OVITO. Note: Windows may prompt you to confirm before running an installer downloaded from a website.

*macOS*:
    Double-click the downloaded :file:`.dmg` disk image file to open it, agree to the license terms, and drag the :program:`Ovito` bundle into your :file:`Applications` folder.
    You can then launch OVITO by double-clicking the application bundle.

**Anaconda installation:** You can install the software also via the Anaconda package manager from
the `conda-forge channel <https://anaconda.org/conda-forge/ovito>`__ (*OVITO Basic*) or from :ref:`our own conda channel <pydoc:installation.anaconda>` (*OVITO Pro*).

**Unattended installation:** The Windows installers support `unattended installation via command-line parameters <https://nsis.sourceforge.io/Docs/Chapter3.html#installerusage>`__
:command:`/S` and :command:`/D` from an administrator command prompt. Furthermore, Windows versions of OVITO are available as .zip archives
`here <https://www.ovito.org/download_history/>`__ and can simply be extracted to a directory of your choice
(`Microsoft Visual C++ Redistributable <https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist>`__ may need to be installed separately).

.. _installation.python:

Installing the OVITO Python module
==================================

To use OVITO Pro's functionality in standalone Python scripts, install the *OVITO Python module* in your Python environment.
It can be used for free and doesn't require a license key. Refer to :ref:`this section <pydoc:installation>` for detailed instructions.

.. _installation.remote:

Running OVITO remotely
======================

You may want to visualize simulation data stored on a high-performance computing (HPC) cluster.
For remote use, consider the following options:

1. Linux remote desktop with VirtualGL + VNC:

  Use a VirtualGL + VNC setup to enable OVITO to leverage the remote machine's graphics hardware.
  Consult the `VirtualGL <https://www.virtualgl.org/>`__ website for setup details.
  Ensure the remote machine supports running desktop applications and ask your HPC cluster administrator about compatibility.

2. Local installation with remote file access:

  Install OVITO on your local computer and use its :ref:`integrated SSH file transfer feature <usage.import.remote>` to
  open files stored on the remote machine.

.. note::

  The OVITO desktop application cannot function via SSH connections using X11 forwarding, as it requires direct
  access to graphics hardware (OpenGL direct rendering mode).
  Starting :command:`ovito` from an SSH terminal without further measures will likely result in errors or a blank application window.

.. _installation.troubleshooting:

Troubleshooting
===============

If you encounter issues during installation, assistance is available in the `OVITO user forum <https://matsci.org/c/ovito/>`__ on matsci.org.
*OVITO Pro* users can also contact `customer support <https://www.ovito.org/contact/>`__ directly. The OVITO team will be happy to help you.

Below are some common installation issues and solutions:

  - :ref:`Installation troubleshooting tips for Linux <installation.troubleshooting.linux>`
  - :ref:`Installation troubleshooting tips for Windows <installation.troubleshooting.windows>`
  - :ref:`Installation troubleshooting tips for macOS <installation.troubleshooting.macos>`

.. _installation.troubleshooting.linux:

Linux
-----

Missing shared object files or broken links
  .. error::

    Starting the desktop application :command:`ovito` or the script interpreter :command:`ovitos` may fail with the following error::

      ./ovito: error while loading shared libraries: libQt5DBus.so.5:
              cannot open shared object file: No such file or directory

    This error is typically caused by broken symbolic links in the :file:`lib/ovito/` sub-directory of the OVITO installation after
    extracting the installation package for Linux on a Windows computer.

  .. admonition:: Solution

    Reinstall OVITO by extracting the `.tar.xz` archive on the target machine.
    Do *not* transfer the directory tree between different computers after it has been extracted,
    because this can easily break symbolic links between files.

Missing XCB system libraries
  .. error::

    You may see the following error when running :command:`ovito` on a Linux machine::

      qt.qpa.plugin: Could not load the Qt platform plugin "xcb" in "" even though it was found.
      This application failed to start because no Qt platform plugin could be initialized.
      Reinstalling the application may fix this problem.
      Available platform plugins are: minimal, offscreen, vnc, xcb.

    In this case OVITO cannot find the required :file:`libxcb-*.so` set of system libraries, which might not be
    preinstalled on new Linux systems.

  .. admonition:: Solution

    Install the required system libraries using your system's package manager:

    .. code-block:: shell

      # On Ubuntu/Debian systems:
      sudo apt install libxcb1 libx11-xcb1 libxcb-glx0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
               libxcb-randr0 libxcb-render-util0 libxcb-render0 libxcb-shape0 libxcb-shm0 \
               libxcb-sync1 libxcb-xfixes0 libxcb-xinerama0 libxcb-xinput0 libxcb-xkb1 libxcb-cursor0 \
               libfontconfig1 libfreetype6 libopengl0 libglx0 libx11-6

      # On CentOS/RHEL systems:
      sudo yum install libxcb xcb-util-cursor xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm

    Debian users should also pay attention to `this thread in the OVITO support forum <https://www.ovito.org/forum/topic/installation-problem/#postid-2272>`__.

Missing OpenGL system libraries
  .. error::

    You may see the following errors when running :command:`ovito` or importing the OVITO Python module on a Linux machine::

      ./ovito: error while loading shared libraries: libOpenGL.so.0: cannot open shared object file: No such file or directory

      libEGL.so.1: cannot open shared object file: No such file or directory

  .. admonition:: Solution

    Install the required system libraries using your system's package manager:

    .. code-block:: shell

      # On Ubuntu/Debian systems:
      sudo apt install libopengl0 libgl1-mesa-glx libegl1
      # On CentOS/RHEL systems:
      sudo yum install libglvnd-opengl libglvnd-glx

    If an installation of the required system libraries is not possible due to restrictions on the target machine, you can
    set up a local :ref:`conda environment and install the OVITO Pro package <installation.anaconda>`. Next, install
    the missing system libraries in the conda environment and make them available:

    .. code-block:: shell

      conda install -c conda-forge libglvnd-opengl-cos7-x86_64 libglvnd-cos7-x86_64 libglvnd-glx-cos7-x86_64 libglvnd-egl-cos7-x86_64
      export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${CONDA_PREFIX}/x86_64-conda-linux-gnu/sysroot/usr/lib

    This should allow you to at least run Python scripts that import the OVITO Python module. The OVITO desktop application
    will still not work, however, because your system is lacking true OpenGL graphics support.

.. _installation.troubleshooting.windows:

Windows
-------

Windows 7 no longer supported
  .. error::

    If you try to run OVITO 3.7 or later on a Windows 7 computer, it will fail with the error "*The procedure entry point CreateDXGIFactory2 could not be
    located in the dynamic link library dxgi.dll*".

  .. admonition:: Solution

    Modern versions of OVITO are based on the Qt6 cross-platform framework, which `requires Windows 10 or later to run <https://doc.qt.io/qt-6/supported-platforms.html>`__.
    Windows 7 has reached its end of life and is no longer supported. Please upgrade your Windows operating system.

.. _installation.troubleshooting.macos:

macOS
-----

OVITO Pro license activation fails
  .. error::

    The activation step could not be completed due to an issue with the local license information store. File path: :file:`$HOME/.config/Ovito/LicenseStore.ini`.
    Please check if file access permissions are correctly set. OVITO Pro requires read/write access to this filesystem path.

  .. admonition:: Solution

    During license activation, *OVITO Pro* needs to create the directory :file:`$HOME/.config/Ovito/` to store the downloaded licensing information.
    The error occurs if creating this directory or storing files in this directory is prevented by insufficient file system permissions.

    In many cases, the parent directory, :file:`$HOME/.config/`, is the actual reason for this problem, because it is owned by the wrong macOS user account.
    :file:`$HOME/.config/` is the `canonical storage location <https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html>`__  for application configuration
    data on Linux/Unix systems. On macOS, this directory is not created by the system by default, but rather it is created by individual applications
    such as OVITO when they run for the first time. As a result, the ownership and permissions of this directory can vary depending on how it was created and
    which system user ran the first process creating it.

    It can happen that :file:`$HOME/.config/` is owned by the system administrator ("root" user), because it was the root user who first ran an application
    creating the :file:`.config` directory. As a result, your personal user account, which you are using to install *OVITO Pro*, can't make further modifications to the directory, which
    lets the license activation fail. To resolve the problem, ask your system administrator to create the :file:`$HOME/.config/Ovito/` subdirectory for you
    and make it writable by your personal user account -- or follow `these instructions <https://apple.stackexchange.com/a/320686>`__ to correct the ownership
    of the :file:`$HOME/.config/` parent directory yourself.

    If changing the ownership is not possible for some reason, as a last resort, you can set the standard environment variable ``XDG_CONFIG_HOME`` to point to some existing directory
    other than :file:`$HOME/.config/`. This will `redirect OVITO Pro <https://specifications.freedesktop.org/basedir-spec/latest/ar01s03.html>`__ to store its
    licensing information in a different place, i.e., in a writable filesystem path of your choice.
