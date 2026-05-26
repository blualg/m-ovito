# Building m-ovito

This repository is derived from the OVITO source tree and includes local modifications. The original external build link referenced by earlier OVITO documentation is no longer used here; this file serves as the repo-local build entry point.

## Platform-specific references in this source tree

The upstream-style platform notes that ship with the source tree are still present:

- [Windows](./doc/manual/development/build_windows.rst)
- [Linux](./doc/manual/development/build_linux.rst)
- [macOS](./doc/manual/development/build_macosx.rst)

Those files are useful background references. The concrete build recipe below is the known working configuration used for this fork on Windows.

## Known working Windows build

### Toolchain used

The following toolchain was used successfully for this repository:

- Visual Studio Build Tools 2022 / MSVC x64
- CMake
- Ninja
- Qt 6.x for MSVC
- Python 3.11
- `vcpkg`

### Configure

Run the following from a Visual Studio developer command prompt:

```bat
call <VSDEVCMD_PATH> -arch=x64
"<CMAKE_EXE>" ^
  -S <REPO_ROOT> ^
  -B <BUILD_DIR> ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_PREFIX_PATH=<QT_ROOT> ^
  -DCMAKE_TOOLCHAIN_FILE=<VCPKG_TOOLCHAIN_FILE> ^
  -DOVITO_BUILD_BASIC=ON ^
  -DOVITO_BUILD_SSH_CLIENT=OFF ^
  -DOVITO_BUILD_PLUGIN_NETCDFPLUGIN=OFF ^
  -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON
```

### Build

```bat
call <VSDEVCMD_PATH> -arch=x64
"<CMAKE_EXE>" --build <BUILD_DIR> --target Ovito --parallel 8
```

### Quick runtime check

```bat
set PATH=<BUILD_DIR>;<QT_ROOT>\bin;%PATH%
<BUILD_DIR>\ovito.exe --version
```

Expected result:

- `Ovito 3.15.3`

Typical placeholder meanings:

- `<REPO_ROOT>`: checkout directory of this repository
- `<BUILD_DIR>`: out-of-source build directory such as `<REPO_ROOT>\build-modern3`
- `<QT_ROOT>`: Qt installation prefix for the MSVC toolchain
- `<VCPKG_TOOLCHAIN_FILE>`: `vcpkg\scripts\buildsystems\vcpkg.cmake`
- `<CMAKE_EXE>`: `cmake.exe`
- `<VSDEVCMD_PATH>`: Visual Studio developer environment batch file

## Current optional-feature choices

The working baseline above uses these build choices:

- `OVITO_BUILD_SSH_CLIENT=OFF`
- `OVITO_BUILD_PLUGIN_NETCDFPLUGIN=OFF`
- `CMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON`

This keeps the build focused on the core application and the local analysis modifications in this fork.
