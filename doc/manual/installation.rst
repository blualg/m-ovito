.. _installation:

============
Installation
============

Binary program packages of OVITO for Linux, Windows, and macOS can be downloaded from `www.ovito.org <https://www.ovito.org/>`_.

.. _installation.requirements:

System Requirements
===================

OVITO requires a 64-bit operating system and runs on processors with x86-64 or arm64 architecture.
The graphical user interface of OVITO requires 3D graphics hardware with support for the `OpenGL <https://en.wikipedia.org/wiki/OpenGL>`_ programming interface (OpenGL 2.1 or newer). 
In general, it is recommended that you install the latest graphics driver provided by your hardware vendor before running OVITO as some older drivers may not fully support modern OpenGL specifications, which can lead to compatibility problems.

Operating system compatibility:

  - Windows 10 (21H2 or later), Windows 11 (21H2 or later) - x86_64 processor architecture
  - Linux: CentOS Linux 8.4 or later, openSUSE 15.3 or later, Ubuntu 20.04 or later, SUSE Linux Enterprise Server 15 SP3 or later - x86_64 processor architecture
  - macOS 10.14, 10.15, 11, 12 - x86_64 and arm64 processor architectures

.. _installation.instructions:

Installation instructions
=========================

*Linux*:
    Extract the downloaded :file:`.tar.xz` archive file using the `tar <https://www.computerhope.com/unix/utar.htm>`_ command: :command:`tar xJfv ovito-{{OVITO_VERSION_STRING}}-x86_64.tar.xz`.
    This will create a new sub-directory containing the program files.
    Change into that directory and start OVITO by running the executable :command:`./bin/ovito`.

*Windows*:
    Run the installer program :file:`ovito-{{OVITO_VERSION_STRING}}-win64.exe` to install OVITO in a directory of your choice.
    Note that Windows might ask whether you really want to launch the installer because it was downloaded from the web and is not digitally signed.

*macOS*:
    Double-click the downloaded :file:`.dmg` disk image file to open it, agree to the program license, and drag the :program:`Ovito` application bundle into your :file:`Applications` folder.
    Then start OVITO by double-clicking the application bundle.

.. _installation.remote:

Running on remote machines
==========================
    
Note that the OVITO desktop application cannot be run through an SSH connection using X11 forwarding mode, because the software requires direct 
access to the graphics hardware (OpenGL direct rendering mode). If you simply run :command:`ovito` in an SSH terminal, you will likely get failure messages 
during program startup or just a black application window. 
  
It is possible to run OVITO on a remote machine through an SSH connection using a VirtualGL + VNC setup.
For further information, please see the `www.virtualgl.org <https://www.virtualgl.org/>`_ website.
In this mode, OVITO will make use of the graphics hardware of the remote machine, which must be set up to allow running
applications in a desktop environment. Please contact your local computing center staff to find out whether 
this kind of remote visualization mode is supported by the HPC cluster(s) you work on. 

Python module installation
==========================

The **OVITO Pro** program packages ship with an :ref:`integrated Python interpreter <ovitos_interpreter>` (:command:`ovitos`) that gets installed alongside the desktop application,
allowing you to execute Python scripts written for OVITO. 
Optionally, you can install the ``ovito`` Python module into an external Python interpreter on your system  (e.g. :program:`Anaconda` or the standard :program:`CPython` interpreter) in case you would like to make use of 
OVITO's functionality in script-based workflows. Please refer to :ref:`this section <pydoc:use_ovito_with_system_interpreter>` for further setup instructions.

.. _installation.troubleshooting:

Troubleshooting
===============

If you run into any problems during the installation of OVITO, you can contact the developers through our `online support forum <https://www.ovito.org/forum/>`_. 
The OVITO team will be happy to help you. The most commonly encountered installation issues on different platforms are addressed here: 

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
    Windows 7 has reached its end of life and is no longer supported. Please upgrade your Windows operating system. With some luck, you may be able to run the Anaconda versions of 
    `OVITO Basic <https://anaconda.org/conda-forge/ovito>`__ or `OVITO Pro <https://www.ovito.org/python-downloads/>`__ on a Windows 7 computer, 
    because these are still built against the old Qt5 framework (as of April 2022).

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
    preinstalled on fresh Linux systems. 

  .. admonition:: Solution

    Install the required libraries using the system's package manager:

    .. code-block:: shell

      # On Ubuntu/Debian systems:
      sudo apt-get install libxcb1 libx11-xcb1 libxcb-glx0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 \
                          libxcb-randr0 libxcb-render-util0 libxcb-render0 libxcb-shape0 libxcb-shm0 \
                          libxcb-sync1 libxcb-xfixes0 libxcb-xinerama0 libxcb-xinput0 libxcb-xkb1
                    
      # On CentOS/RHEL systems:
      sudo yum install libxcb xcb-util-image xcb-util-keysyms xcb-util-renderutil xcb-util-wm

    Debian users should also pay attention to `this thread in the OVITO support forum <https://www.ovito.org/forum/topic/installation-problem/#postid-2272>`__.

.. _installation.troubleshooting.macos:

macOS
-----

OVITO Pro license activation fails
  .. error::

    The activation step could not be completed due to an issue with the local license information store. File path: :file:`$HOME/.config/Ovito/LicenseStore.ini`.
    Please check if file access permissions are correctly set. OVITO Pro requires read/write access to this filesystem path. 

  .. admonition:: Solution
    
    OVITO Pro needs to save its licensing information under the path :file:`$HOME/.config/Ovito/`, which is the `canonical storage location <https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html>`__ 
    for configuration data of Linux/Unix applications.
    On some macOS computers, the :file:`$HOME/.config/` directory may have been marked as write-protected by the system administrator, 
    which lets the license activation process fail. Please ask your system administrator to make the :file:`$HOME/.config/Ovito/` subdirectory 
    writable by applications running under your user account.  

    If this is not possible for some reason, you can set the standard environment variable ``XDG_CONFIG_HOME`` to point to some directory other than :file:`$HOME/.config/`.
    `This will redirect OVITO Pro <https://specifications.freedesktop.org/basedir-spec/latest/ar01s03.html>`__ to store its licensing information in a different location.
