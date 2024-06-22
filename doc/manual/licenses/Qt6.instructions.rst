.. _appendix.license.qt6.instructions:

Build instructions for Qt6
--------------------------

*OVITO Basic* and *OVITO Pro* program packages include a binary copy of the Qt framework licensed under the GNU Lesser General Public License (LGPLv3).
In accordance with the requirements of this license, this section gives instructions on how to obtain or rebuild compatible versions of these binaries from source.

Windows
"""""""

OVITO for Windows includes an unmodified copy of the Qt shared libraries (version 6.5.3, MSVC 2019 64-bit) distributed by the Qt Company.

Linux
"""""

OVITO for Linux includes a copy of Qt shared libraries, which have been built from the unmodified Qt sources (version 6.7.2) as follows::

  ./configure \
    -opensource -confirm-license -shared -nomake examples -qt-libpng -qt-libjpeg -qt-pcre -xkbcommon -no-cups -pch -no-eglfs -no-linuxfb -fontconfig -libinput -icu -no-feature-vulkan \
    -skip qtactiveqt -skip qtconnectivity -skip qt3d -skip qtcanvas3d -skip qtdatavis3d -skip qtcharts -skip qtlocation -skip qtsensors -skip qtdeclarative -skip qtdoc \
    -skip qtgraphicaleffects -skip qtmultimedia -skip qtquickcontrols -skip qtquickcontrols2 -skip qtpurchasing -skip qtremoteobjects -skip qtsensors \
    -skip qtserialport -skip qttranslations -skip qtwebchannel -skip qtgamepad -skip qtscript -skip qtserialbus -skip qtvirtualkeyboard \
    -skip qtwebengine -skip qtwebsockets -skip qtwebview -skip qtwebglplugin -skip qtxmlpatterns \
    -skip qt5compat -skip qtlottie -skip qtmqtt -skip qtopcua -skip qtquicktimeline -skip qtquick3d -skip qtquick3dphysics -skip qtscxml \
    -skip qtspeech -skip qtcoap -skip qthttpserver -skip qtpositioning -skip qtquickeffectmaker -skip qtgraphs -skip qtlanguageserver \
    -no-use-gold-linker -prefix /usr/local/lib/qt6
  cmake --build . --parallel
  cmake --install .

macOS
"""""

OVITO for macOS includes an unmodified copy of the Qt shared libraries (version 6.7.2) distributed by the Qt Company.
