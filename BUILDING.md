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
call C:\BuildTools\Common7\Tools\VsDevCmd.bat -arch=x64
"C:\Program Files\CMake\bin\cmake.exe" ^
  -S C:\Users\tsaka\Documents\CODEX\Ovito-current ^
  -B C:\Users\tsaka\Documents\CODEX\Ovito-current\build-modern3 ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.10.2\msvc2022_64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:\Users\tsaka\Documents\CODEX\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DOVITO_BUILD_BASIC=ON ^
  -DOVITO_BUILD_SSH_CLIENT=OFF ^
  -DOVITO_BUILD_PLUGIN_NETCDFPLUGIN=OFF ^
  -DCMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON
```

### Build

```bat
call C:\BuildTools\Common7\Tools\VsDevCmd.bat -arch=x64
"C:\Program Files\CMake\bin\cmake.exe" --build C:\Users\tsaka\Documents\CODEX\Ovito-current\build-modern3 --target Ovito --parallel 8
```

### Quick runtime check

```bat
set PATH=C:\Users\tsaka\Documents\CODEX\Ovito-current\build-modern3;C:\Qt\6.10.2\msvc2022_64\bin;%PATH%
C:\Users\tsaka\Documents\CODEX\Ovito-current\build-modern3\ovito.exe --version
```

Expected result:

- `Ovito 3.15.3`

## Current optional-feature choices

The working baseline above uses these build choices:

- `OVITO_BUILD_SSH_CLIENT=OFF`
- `OVITO_BUILD_PLUGIN_NETCDFPLUGIN=OFF`
- `CMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON`

This keeps the build focused on the core application and the local analysis modifications in this fork.
