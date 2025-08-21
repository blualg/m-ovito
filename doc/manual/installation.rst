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
  macOS 14.0+, Apple Silicon (arm64).

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

If you experience installation issues, visit the `OVITO user forum <https://matsci.org/c/ovito/>`__ for support.
*OVITO Pro* users can also contact `customer support <https://www.ovito.org/contact/>`__ directly. The OVITO team will be happy to help you.

Common installation issues and solutions:

  - :ref:`Linux troubleshooting <installation.troubleshooting.linux>`
  - :ref:`Windows troubleshooting <installation.troubleshooting.windows>`
  - :ref:`macOS troubleshooting <installation.troubleshooting.macos>`

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
      sudo yum install libglvnd-opengl libglvnd-glx libglvnd-egl

    If an installation of the required system libraries is not possible due to restrictions on the target machine, you can
    set up a local :ref:`conda environment and install the OVITO Pro package <installation.anaconda>`. Next, install
    the missing system libraries in the conda environment and make them available:

    .. code-block:: shell

      conda install -c conda-forge libglvnd-opengl-cos7-x86_64 libglvnd-cos7-x86_64 libglvnd-glx-cos7-x86_64 libglvnd-egl-cos7-x86_64
      export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${CONDA_PREFIX}/x86_64-conda-linux-gnu/sysroot/usr/lib

    This should allow you to at least run Python scripts that import the OVITO Python module. The OVITO desktop application
    will still not work, however, because your system is lacking true OpenGL graphics support.

OpenGL initialization fails
  .. error::

    You may see the following messages when running :command:`ovito` in the terminal:

    .. code-block:: shell

      $ bin/ovito
      QXcbIntegration: Cannot create platform OpenGL context, neither GLX nor EGL are enabled
      QRhiGles2: Failed to create temporary context
      QXcbIntegration: Cannot create platform offscreen surface, neither GLX nor EGL are enabled
      QXcbIntegration: Cannot create platform OpenGL context, neither GLX nor EGL are enabled
      QRhiGles2: Failed to create context
      Failed to create QRhi for QBackingStoreRhiSupport

  .. admonition:: Solution

    Run :command:`ovito` with the environment variable ``QT_XCB_GL_INTEGRATION=xcb_egl``:

    .. code-block:: shell

      $ QT_XCB_GL_INTEGRATION=xcb_egl bin/ovito

LD_LIBRARY_PATH overrides OVITO's bundled Qt libraries
  .. error::

    Launching :command:`ovito` may fail with an error such as::

      ./bin/ovito: /usr/lib/x86_64-linux-gnu/libQt6Core.so.6: version `Qt_6.8' not found (required by ovito)

    or a similar message pointing to an incompatible copy of a shared library needed by the program.

  .. admonition:: Solution

    OVITO ships its own Qt libraries in :file:`lib/ovito/`.
    If the environment variable ``LD_LIBRARY_PATH`` lists a system directory
    like :file:`/usr/lib/`, the dynamic linker loads the wrong *libQt6Core.so.6*
    before OVITO's own version, causing the mismatch above.
    Either start OVITO with a clean library path

    .. code-block:: shell

      env -u LD_LIBRARY_PATH ./bin/ovito          # temporarily ignore LD_LIBRARY_PATH

    or remove the offending directories from ``LD_LIBRARY_PATH`` in your shell configuration.

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

    License activation fails due to an issue with the local license information store (file path :file:`$HOME/.config/Ovito/LicenseStore.ini`).
    Please check if file access permissions are correctly set. OVITO Pro requires read/write access to this filesystem path.

  .. admonition:: Solution

    OVITO Pro requires read/write access to :file:`$HOME/.config/Ovito/` for storing the license activation.

    - If this directory does not exist, OVITO Pro will attempt to create it. If permission issues arise, check ownership of :file:`$HOME/.config/`.
    - This directory may be owned by the system administrator (`root`), preventing modifications by applications running under your personal user account.
      In such cases, a system administrator must grant write access.

    In many cases, the issue stems from the parent directory, :file:`$HOME/.config/`, being owned by the wrong macOS user account.
    This directory serves as the `canonical storage location <https://specifications.freedesktop.org/basedir-spec/latest/>`__ for
    application configuration data on Linux/Unix systems. Unlike Linux, macOS does not create :file:`$HOME/.config/` by default;
    instead, individual applications such as OVITO create it when first run. As a result, its ownership and permissions may vary
    depending on which user account initiated its creation.

    If :file:`$HOME/.config/` was originally created by the system administrator (`root`), your personal user account may lack
    write access, preventing OVITO Pro from modifying it. This causes license activation to fail. To fix this issue:

    - Ask your system administrator to create :file:`$HOME/.config/Ovito/` and grant write access to your user account.
    - Alternatively, follow `this guide <https://apple.stackexchange.com/a/320686>`__ to correct ownership of :file:`$HOME/.config/` yourself.

    If changing ownership is not possible, you can set the environment variable ``XDG_CONFIG_HOME`` (`info <https://specifications.freedesktop.org/basedir-spec/latest/#variables>`__)
    to point to a different writable directory:

    .. code-block:: shell

      export XDG_CONFIG_HOME=/path/to/writable/directory

    This redirects OVITO Pro to store its licensing information in a user-specified location.
