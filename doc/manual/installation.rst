.. _installation:

============
Installation
============

Binary program packages of *OVITO Basic* and *OVITO Pro* for Linux, Windows, and macOS can be downloaded from `www.ovito.org <https://www.ovito.org/>`_.

.. _installation.requirements:

System requirements
===================

OVITO runs on processors with x86-64 or arm64 architecture. The desktop application requires an environment that has support for the `OpenGL <https://en.wikipedia.org/wiki/OpenGL>`_ 3d graphics interface (OpenGL 2.1 or newer).
In general, it is recommended that you install the latest graphics driver provided by your hardware vendor, as some older drivers may not fully support modern OpenGL specifications.

Operating system compatibility:

  - 64-bit Windows 10 (21H2 or later), Windows 11 (21H2 or later) -- x86_64 processor architecture
  - Linux: CentOS Linux 8.4+, openSUSE 15.3+, Ubuntu 20.04+, SUSE Linux Enterprise Server 15+, or any compatible distributions running on x86_64 processors
  - macOS 10.15+ -- arm64 (Apple Silicon) and Intel processor architectures

.. _installation.instructions:

Installation instructions
=========================

*Linux*:
    Extract the downloaded :file:`.tar.xz` archive file using the tar utility: :command:`tar xJfv ovito-{{OVITO_VERSION_STRING}}-x86_64.tar.xz`.
    This will create a new sub-directory containing the program files.
    Change into that directory and start OVITO by running the executable :command:`./bin/ovito`.

*Windows*:
    Run the installer program :file:`ovito-{{OVITO_VERSION_STRING}}-win64.exe` to install OVITO in a directory of your choice.
    Note that Windows might ask whether you really want to launch the installer since it was downloaded from the web.

*macOS*:
    Double-click the downloaded :file:`.dmg` disk image file to open it, agree to the program license, and drag the :program:`Ovito` application bundle into your :file:`Applications` folder.
    Then start OVITO by double-clicking the application bundle.

.. _installation.remote:

Running on remote machines
==========================

Note that the OVITO desktop application cannot be run through an SSH connection using X11 forwarding mode, because the software requires direct
access to the graphics hardware (OpenGL direct rendering mode). If you simply run :command:`ovito` in an SSH terminal, you will likely get failure messages
during program startup or just a black application window.

It is possible to run OVITO on a remote machine through an SSH connection using a VirtualGL + VNC remote desktop setup.
For further information, please see the `www.virtualgl.org <https://www.virtualgl.org/>`_ website.
In this mode, OVITO will make use of the graphics hardware of the remote machine, which must be set up to allow running
applications in a desktop environment. Please contact your local computing center staff to find out whether
this kind of remote visualization mode is supported by the HPC cluster you work on.

.. _installation.python:

Python module installation
==========================

The *OVITO Pro* program package includes an :ref:`integrated Python interpreter <ovitos_interpreter>` (:command:`ovitos`) that gets installed alongside with the application,
allowing you to run Python code written for OVITO and install third-party modules from the `OVITO Extensions Directory <https://www.ovito.org/extensions/>`__ for example.
You can also install the standalone ``ovito`` Python module into an external Python interpreter on your system  (e.g. :program:`Anaconda` or the standard :program:`CPython` interpreter) in case you
would like to make use of OVITO's functionality in script-based workflows. Please refer to :ref:`this section <pydoc:installation>` for further setup instructions.

.. _installation.troubleshooting:

Troubleshooting
===============

If you run into any problems during the installation of OVITO, you can contact the developers through the `online user forum <https://matsci.org/c/ovito/>`__.
The OVITO team will be happy to help you. The most commonly encountered installation issues on different platforms are addressed in the following:

  - :ref:`installation.troubleshooting.windows`
  - :ref:`installation.troubleshooting.linux`
  - :ref:`installation.troubleshooting.macos`

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

    Install the required system libraries using your Linux package manager:

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

    Install the required system libraries using your package manager:

    .. code-block:: shell

      # On Ubuntu/Debian systems:
      sudo apt install libopengl0 libgl1-mesa-glx libegl1

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
