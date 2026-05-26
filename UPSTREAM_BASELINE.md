# OVITO Upstream Baseline

## Chosen source base

Use the newer upstream OVITO source tree in the current repository checkout.

The repository was cloned from:

- `https://gitlab.com/stuko/ovito.git`

The local working branch is:

- `modern-base-3.15.3`

This branch tracks:

- `origin/dev3.15.3`

That makes it a much better foundation than the old 2015 codebase in a separate local checkout, because it already contains the modern architecture, UI workflow, file readers, rendering updates, and modifier system we want to build on.

## License status

The current upstream source is explicitly modifiable.

Repository-level license summary:

- `LICENSE.txt` states that every source file in the implementation of OVITO may be redistributed and/or modified under either:
  - GNU GPL v3
  - MIT

Important note:

- Third-party libraries under `src/3rdparty/` keep their own licenses.

Conclusion:

- The current upstream source is suitable as the new implementation base.

## Installed toolchain on this PC

Installed during setup:

- Visual Studio Build Tools 2022 with MSVC C++ toolchain
- CMake 4.3.1
- Ninja 1.13.2
- Python 3.11.9
- Qt 6.10.2 (`C:\Qt\6.10.2\msvc2022_64`)
- `aqtinstall`
- `vcpkg`
- Boost 1.90.0 through `vcpkg`
- zlib 1.3.1 through `vcpkg`

Useful local paths:

- MSVC environment script:
  - `<VSDEVCMD_PATH>`
- CMake:
  - `<CMAKE_EXE>`
- Qt:
  - `<QT_ROOT>`
- vcpkg toolchain file:
  - `<VCPKG_TOOLCHAIN_FILE>`

## Proven build status

The newer source has already been configured and built successfully on this machine.

Working configure command:

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

Working build command:

```bat
call <VSDEVCMD_PATH> -arch=x64
"<CMAKE_EXE>" --build <BUILD_DIR> --target Ovito --parallel 8
```

Smoke test that succeeded:

```bat
set PATH=<BUILD_DIR>;<QT_ROOT>\bin;%PATH%
<BUILD_DIR>\ovito.exe --version
```

Observed result:

- `Ovito 3.15.3`

## Optional dependencies still missing or intentionally disabled

Current baseline build choices:

- `OVITO_BUILD_SSH_CLIENT=OFF`
- `OVITO_BUILD_PLUGIN_NETCDFPLUGIN=OFF`
- `CMAKE_DISABLE_FIND_PACKAGE_ZLIB=ON`

Not yet installed:

- ffmpeg development libraries
- libssh
- NetCDF

Why they are deferred:

- They are not required to get a modern working application baseline.
- We can add them after the core modernization work is underway.

## Feature target taken from the current release line

The modernization target should follow the current OVITO 3.15 release line, not the old 2.4.x application behavior.

### Open-source or core product direction to match

- Recent Files menu
- Pipeline visibility toggles in the pipelines list
- Keyboard search in the redesigned modifier selector
- New workflow for editing simulation cell and type definitions through modifiers
- `Edit simulation cell` modifier
- `Edit types` modifier
- `Types` tab in the data inspector
- Modifier snippets import/export workflow
- Better molecular workflow, including covalent-radius bond creation
- `Select overlapping particles` modifier
- Split bond-analysis workflow into bond-angle and bond-length distribution tools
- Molecule mode for `Expand selection`
- Object-specific operation scope for several modifiers
- External FFmpeg encoding support
- Import/export of chemical element color/radius themes as JSON
- MOL/SDF reader and expanded modern chemistry/molecular file support
- Improved PDB/CIF/mmCIF handling
- Smaller session state files for long trajectories
- Interactive viewport error reporting
- Simulation-cell data inspector improvements
- Data inspector filtering and better pipeline-state visibility
- Timeline display of simulation timesteps
- Shared visual elements across multiple objects

### Pro-only items to keep separate

Some features in the official changelog are marked as Pro-only and should be treated as a separate track:

- Bond order modifier
- VisRTX renderer enhancements
- Find rings modifier
- Reduce property modifier
- Difference between frames modifier
- Remote rendering / remote access workflows

This distinction matters because the public upstream repository corresponds roughly to OVITO Basic.

## Recommendation

Do not continue from the old 2015 source as the primary implementation base.

Recommended strategy:

1. Use `Ovito-current` as the active source base.
2. Treat `Ovito` (the old repository) as reference material only.
3. Re-enable optional dependencies one by one:
   - zlib
   - ffmpeg
   - NetCDF
   - libssh
4. Add new features on top of the newer architecture instead of backporting the entire modern product to the old tree.

## Immediate next milestone

The next practical milestone should be:

- clean up the Windows build recipe,
- re-enable zlib without the current DLL naming mismatch workaround,
- decide which 3.15 features we want to prioritize first,
- and then begin implementing net-new functionality in `Ovito-current`.
