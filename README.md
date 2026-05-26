# m-ovito

`m-ovito` is a modified source version of [OVITO](https://www.ovito.org/), the Open Visualization Tool for atomistic, molecular, and particle-based simulation data.

This repository is intended as a reproducible source-code record of the customized OVITO build used for transport, interfacial, and trajectory-analysis workflows in our research.

Public repository:

- [blualg/m-ovito](https://github.com/blualg/m-ovito)

## Upstream base

This repository is derived from the public OVITO source code:

- Upstream project: [https://gitlab.com/stuko/ovito.git](https://gitlab.com/stuko/ovito.git)
- Upstream baseline commit used locally: `6088e4c233aede802cff4d06bdd8787df22848b3`

## What is included here

This source tree contains OVITO-derived code plus local modifications, including custom analysis modifiers, workflow changes, and supporting scripts under [tools](./tools).

Examples of customized areas in this tree include:

- transport analysis tooling and UI changes
- trajectory-wide modifiers such as time series and time averaging
- interface / film-analysis related additions
- helper scripts for post-processing simulation outputs

## Licensing

The OVITO implementation source files in this repository are available under the terms of either:

- GNU GPL v3
- MIT

See:

- [LICENSE.txt](./LICENSE.txt)
- [LICENSE.GPL.txt](./LICENSE.GPL.txt)
- [LICENSE.MIT.txt](./LICENSE.MIT.txt)

This repository also contains bundled third-party components in [src/3rdparty](./src/3rdparty), which remain under their own licenses. A convenience summary is provided in [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md). The original license files shipped with those components remain the authoritative source.

## Citation

This codebase is associated with both the original OVITO software paper and this modified source repository.

1. The original OVITO software paper:

   A. Stukowski, *Visualization and analysis of atomistic simulation data with OVITO - the Open Visualization Tool*, Modelling Simul. Mater. Sci. Eng. 18 (2010), 015012.

2. This modified source repository:

   - repository name: `m-ovito`

GitHub citation metadata for this repository is provided in [CITATION.cff](./CITATION.cff).

## Building

OVITO build documentation:

- [https://www.ovito.org/docs/current/development.php](https://www.ovito.org/docs/current/development.php)

This local source tree also contains Windows-specific build notes in [UPSTREAM_BASELINE.md](./UPSTREAM_BASELINE.md).

## Attribution

This repository is an unofficial modified source tree derived from OVITO. It is not an official release of OVITO GmbH.
