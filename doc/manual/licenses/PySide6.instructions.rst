.. _appendix.license.pyside6.instructions:

Build instructions for PySide6
------------------------------

*OVITO Pro* and the *OVITO Python package* use a distribution of the PySide6 module and Shiboken6 module licensed under the GNU Lesser General Public License (LGPLv3).
In accordance with the requirements of this license, this page provides instructions on how to obtain or rebuild compatible versions of these binary modules from source.

Windows
"""""""

*OVITO Pro* for Windows includes a copy of the PySide6-Essentials module (version 6.7.3) from
the official `PyPI repository <https://pypi.org/project/PySide6/>`__.

Linux
"""""

*OVITO Pro* for Linux ships with a copy of the PySide6 module that has been built from the original sources provided by
the Qt Company, following the standard procedure described `here <https://doc.qt.io/qtforpython-6/gettingstarted/linux.html>`__.
PySide6 v6.7.3 has been compiled against Qt 6.7.3 (see :ref:`here <appendix.license.qt6.instructions>`) and a custom build of the `CPython <https://www.python.org>`__ 3.12 interpreter::

  git clone --recursive https://code.qt.io/pyside/pyside-setup
  cd pyside-setup
  git checkout v6.7.3
  python3 setup.py install \
    --qtpaths=/usr/local/lib/qt6/bin/qtpaths \
    --ignore-git \
    --parallel=8 \
    --module-subset=Core,Gui,Widgets,Xml,Network,Svg,OpenGL,OpenGLWidgets \
    --verbose-build \
    --no-qt-tools

macOS
"""""

OVITO Pro for macOS ships with a copy of the PySide6 module that has been built from the original sources provided by
the Qt Company, following the standard procedure described `here <https://doc.qt.io/qtforpython-6/gettingstarted/macOS.html>`__.
PySide6 v6.7.3 has been compiled against Qt 6.7.3 (macOS) and a standard installation of the `CPython <https://www.python.org>`__ 3.12 interpreter for macOS (universal2 binary)::

  git clone --recursive https://code.qt.io/pyside/pyside-setup
  cd pyside-setup
  git checkout v6.7.3

  sudo CLANG_INSTALL_DIR=$HOME/progs/libclang \
    python3.12 setup.py install \
    --qtpaths=`echo $HOME/Qt/6.7.*/macos/bin/qtpaths` \
    --ignore-git \
    --module-subset=Core,Gui,Widgets,Xml,Network,Svg,OpenGL,OpenGLWidgets \
    --no-qt-tools \
    --macos-deployment-target=11.0 \
    --macos-arch='x86_64;arm64'

  cd /Library/Frameworks/Python.framework/Versions/3.12/lib/python3.12/site-packages/PySide6/
  sudo rm -r Qt
