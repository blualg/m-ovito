# Third-Party License Summary

This file is a convenience summary of bundled third-party components found under [src/3rdparty](./src/3rdparty).

The original license files shipped with each component remain the authoritative source.

## Bundled components

| Component | License summary | Primary local reference |
| --- | --- | --- |
| `function2` | Boost Software License 1.0 | [src/3rdparty/function2/LICENSE.txt](./src/3rdparty/function2/LICENSE.txt) |
| `gemmi` | MPL-2.0; README also notes MPLv2 or LGPLv3 at your option | [src/3rdparty/gemmi/LICENSE.txt](./src/3rdparty/gemmi/LICENSE.txt) |
| `geogram` | BSD-3-Clause | [src/3rdparty/geogram/LICENSE.txt](./src/3rdparty/geogram/LICENSE.txt) |
| `gsd` | BSD-2-Clause | [src/3rdparty/gsd/gsd.h](./src/3rdparty/gsd/gsd.h) |
| `kissfft` | BSD-3-Clause; includes `LICENSES/Unlicense` in tree | [src/3rdparty/kissfft/COPYING](./src/3rdparty/kissfft/COPYING) |
| `muparser` | MIT | [src/3rdparty/muparser/License.txt](./src/3rdparty/muparser/License.txt) |
| `mwm_csp` | MIT | [src/3rdparty/mwm_csp/mwm_csp.cpp](./src/3rdparty/mwm_csp/mwm_csp.cpp) |
| `netcdf_integration` | OVITO-owned integration wrapper, dual GPLv3/MIT like the main OVITO source files checked here | [src/3rdparty/netcdf_integration/NetCDFIntegration.cpp](./src/3rdparty/netcdf_integration/NetCDFIntegration.cpp) |
| `ptm` | MIT | [src/3rdparty/ptm/LICENSE](./src/3rdparty/ptm/LICENSE) |
| `pybind11` | BSD-3-Clause | [src/3rdparty/pybind11/LICENSE](./src/3rdparty/pybind11/LICENSE) |
| `qwt` | LGPL-2.1-based Qwt license with stated exceptions | [src/3rdparty/qwt/COPYING](./src/3rdparty/qwt/COPYING) |
| `rapidyaml` | MIT | [src/3rdparty/rapidyaml/LICENSE.txt](./src/3rdparty/rapidyaml/LICENSE.txt) |
| `tachyon` | BSD-3-Clause, Copyright (c) 1994-2013 John E. Stone | [src/3rdparty/tachyon/LICENSE](./src/3rdparty/tachyon/LICENSE) |
| `voro++` | BSD-style permissive license | [src/3rdparty/voro++/LICENSE](./src/3rdparty/voro++/LICENSE) |
| `xdrfile` | BSD-2-Clause-style permissive license | [src/3rdparty/xdrfile/xdrfile.h](./src/3rdparty/xdrfile/xdrfile.h) |
| `zstd` | dual BSD or GPLv2 | [src/3rdparty/zstd/LICENSE](./src/3rdparty/zstd/LICENSE) |

## Notes

- This repository snapshot is source-only; binary distribution has not been reviewed in this summary.
- This summary is for convenience only and does not replace the full text of the component licenses.

## External extension references

| Component | License summary | Notes |
| --- | --- | --- |
| `ovito-org/ScoreBasedDenoising` | MIT, Copyright (c) 2026 Daniel Utt | The native m-ovito score-based denoising modifier adapts the upstream extension's Python inference flow. The PyTorch, torch-geometric, and graphite runtime stack is not bundled and must be supplied by the user's Python environment. |
| `LLNL/graphite` denoiser model code | MIT, Copyright (c) 2022 Tim Hsu | Referenced through the external `graphite` Python package and pretrained denoiser models used by `ScoreBasedDenoising`; models are not bundled in this source tree. |
| `ovito-org/RingFinder` | GPL-3.0-only, Copyright (c) 2025 Daniel Utt and Alexander Stukowski | The native m-ovito Ring Finder modifier adapts the upstream extension's shortest-ring search and output model. The upstream triangulation helper is not bundled. |
