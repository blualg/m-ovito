# m-ovito additional modifications manual

This page documents the additions and behavior changes in the public `m-ovito` source tree. For the standard OVITO user manual, see [docs.ovito.org](https://docs.ovito.org/).

Repository: [blualg/m-ovito](https://github.com/blualg/m-ovito)

## General behavior

- The bottom data-inspector help button opens this public manual page when a local generated OVITO manual is not available.
- Custom trajectory-wide analysis modifiers use an explicit `Run` or `Start` button when they need to traverse many frames.
- Long trajectory-wide calculations report progress through OVITO's normal progress/status system instead of opening a separate blocking progress dialog.
- Result tables can be opened from modifier panels with `Show in data inspector` when the modifier produces a data table.

## Tachyon renderer

`Tachyon` is an output renderer for generating images and movies from the render settings panel. It is not a live viewport backend.

Main controls:

- Antialiasing samples. The default is moderate for speed; increase it for final figures.
- Max ray recursion.
- Max transparent surfaces.
- Direct light and shadows.
- Ambient occlusion and ambient-occlusion samples. Ambient occlusion is off by default because it is expensive for large transparent scenes.
- Default material ambient, diffuse, and highlight weights. Mirror reflections are disabled; the highlight control uses non-recursive Phong highlights.
- Maximum Tachyon worker threads, where `0` lets Tachyon choose.

Supported first-pass scene elements:

- Spherical particles.
- Cylinders and bond-like geometry.
- Triangle meshes, including optional wireframe edges.
- Basic line primitives.

Known limitations:

- Special particle glyphs, arrowheads, and volume primitives are not implemented in this first native Tachyon renderer.
- Unsupported scene elements are skipped with renderer issue messages instead of crashing the render.

License note:

- The bundled Tachyon ray tracing library is BSD-3-Clause licensed, Copyright (c) 1994-2013 John E. Stone.

## OSPRay renderer

`OSPRay` is an output renderer for generating images and movies with Intel OSPRay when the OSPRay runtime is available on the system. It is not a live viewport backend.

Main controls:

- Renderer type. `scivis` is the default first-pass renderer subtype.
- Samples per pixel.
- OSPRay library path. Leave it empty to load `ospray.dll` from `PATH` or the application directory.
- Direct light, ambient light, shadows, and ambient-occlusion samples.

Supported first-pass scene elements:

- Spherical particles.
- Cylinders and bond-like geometry.
- Triangle meshes, including optional wireframe edges.
- Basic line primitives.

Known limitations:

- Volume primitives, special particle glyphs, mesh pseudo-color mappings, and instanced meshes are not fully implemented in this first native OSPRay renderer.
- Unsupported scene elements are skipped with renderer issue messages instead of crashing the render.

License note:

- The OSPRay runtime is not bundled in this source tree. Install or provide a compatible OSPRay runtime separately if you want to render with this backend.

## VisRTX renderer

`VisRTX` is an output renderer for generating images and movies through the Khronos ANARI runtime using NVIDIA VisRTX. It is not a live viewport backend.

Main controls:

- ANARI runtime path. Leave it empty to load `anari.dll` from `PATH` or the application directory.
- ANARI library name. Use `visrtx` for NVIDIA VisRTX.
- Device subtype and renderer subtype. `default` is usually appropriate.
- Samples per pixel.
- Direct light irradiance.
- Ambient radiance and ambient samples.
- Denoise.
- Max ray depth.

Supported first-pass scene elements:

- Spherical particles.
- Cylinders and bond-like geometry.
- Triangle meshes, including optional wireframe edges.
- Basic line primitives.

Known limitations:

- VisRTX requires a compatible NVIDIA GPU and driver plus the ANARI/VisRTX runtime libraries.
- Volume primitives, special particle glyphs, mesh pseudo-color mappings, and instanced meshes are not fully implemented in this first native VisRTX renderer.
- Unsupported scene elements are skipped with renderer issue messages instead of crashing the render.

License note:

- The ANARI SDK and NVIDIA VisRTX runtime are not bundled in this source tree. Install or provide compatible runtime libraries separately if you want to render with this backend.

## Load topology

`Load topology` is a standalone modifier for loading bond/topology information from an external topology-style file. It was separated from `Create bonds` so topology import can be added as its own pipeline operation.

Use this when the trajectory already has meaningful atom ordering or molecule identity information and the bond graph should come from a reference topology instead of distance-based bond creation.

## Time series

The modified `Time series` modifier has a dedicated start control. It does not automatically traverse the whole trajectory as soon as the modifier is added or edited.

Typical outputs:

- Global attribute versus frame or time.
- Data-table quantity versus frame or time.
- Element/property quantity versus frame or time.

## Time averaging

The modified `Time averaging` modifier has a dedicated start control. This avoids immediately recomputing the average whenever the modifier UI is opened or an upstream modifier changes.

Supported average targets include:

- Global attributes.
- Data tables.
- Element properties for stable element counts or stable element identifiers.

If `Overwrite original quantity` is enabled, the averaged result replaces the selected output quantity. If it is disabled, the modifier keeps the original quantity and writes a separate averaged result where supported.

## Willard-Chandler interface

The custom Willard-Chandler interface modifier builds a density field from selected particles, extracts an interface mesh, and can classify particles relative to that interface.

Main controls:

- Gaussian width: smoothing width used when constructing the density field.
- Isovalue: density threshold used for the extracted interface.
- Grid resolution: density grid resolution.
- Interfacial thickness: distance range used for interfacial particle classification.
- Only selected particles: restricts the density construction and classification source set.
- Select interfacial particles and select vapor particles: controls which particles are selected by the modifier.

Notes:

- The normal periodic correction path treats the simulation cell according to the active periodic geometry.
- Slab-specific vacuum handling should be treated as a separate modeling choice, not as the default behavior.
- The data inspector may contain surface-region and phase-count tables depending on the selected options.

## Coordination environment autocorrelation

`Coordination environment autocorrelation function` tracks whether a selected coordination environment persists over time.

Main inputs:

- Central atom type selector.
- Shell atom type selector.
- Distance cutoff defining the coordination shell.
- Indicator function controlling the reported correlation observable.
- Sampling frequency and maximum lag.

The modifier computes the selected observable only after `Run CACF analysis` is clicked.

## Hydrogen bond analysis

`Hydrogen bond analysis` identifies hydrogen-bond candidates over sampled trajectory frames and reports hydrogen-bond observables as tables and attributes.

Main inputs:

- Donor atom selector.
- Hydrogen atom selector.
- Acceptor atom selector.
- Donor-hydrogen cutoff.
- Hydrogen-bond definition mode.
- Sampling frequency and optional analysis interval.

For PMF-derived definitions, upstream PMF metadata must be available from a compatible hydrogen-bond analysis setup.

## Hydrogen bond kinetics

`Hydrogen bond kinetics` computes time-correlation style hydrogen-bond observables from sampled hydrogen-bond states.

Main inputs:

- Donor, hydrogen, and acceptor selectors.
- Donor-hydrogen cutoff or bond-topology pairing.
- Hydrogen-bond definition mode.
- Sampling frequency and maximum lag.

The PMF-derived kinetics mode expects selector and cutoff settings to match the upstream PMF-derived hydrogen-bond analysis.

## Survival probability

`Survival probability` tracks whether molecules or sites remain in a reference shell over lag time.

Main inputs:

- Reference/orient-around atom selector.
- Molecule-site atom selector.
- Distance cutoff.
- Intermittency, which allows short absences to be treated as continuous residence.
- Sampling frequency and maximum lag.

The result is written as a data table and can be opened in the data inspector.

## Molecule orientational relaxation

`Molecule orientational relaxation` computes orientational correlation functions for molecular descriptors such as a dipole vector.

Main inputs:

- Descriptor type.
- Direction start and end atom selectors.
- Orient-around atom selector.
- Molecule-site atom selector.
- Distance cutoff.
- Legendre order and descriptor subset.
- Sampling frequency and maximum lag.

## Molecular orientation

`Molecular orientation` computes orientation descriptors for selected molecular sites. The output can be inspected through the data inspector when a result table is produced.

## Water cage analysis

`Water cage analysis` identifies clathrate-style cages from the water oxygen network. It builds an O-O first-neighbor graph, finds chordless water rings, and searches for closed edge-saturated cage face sets.

Main inputs:

- Water oxygen atom type selector or expression.
- O-O neighbor cutoff.
- Standard cage families to detect: `5^12`, `5^12 6^2`, and `5^12 6^4`.
- Optional general complete cage search. General cages are closed ring-face cages satisfying the closed polyhedral topology condition, with configurable ring-size and face-count limits.
- Optional restriction to selected oxygen particles.
- Optional cage visualization bonds.
- Maximum ring/search states, which limits unusually expensive ring enumeration and topology searches.

Outputs:

- Global attributes including `WaterCage.total_cages`, `WaterCage.count_5_12`, `WaterCage.count_5_12_6_2`, and `WaterCage.count_5_12_6_4`.
- A cage-count data table named `water-cage-counts`. Standard cage type IDs are `1`, `2`, and `3`; general/nonstandard signatures use IDs starting at `100`.
- A visible cage visualization particle container named `water-cages`. It contains one cage-center particle and duplicated cage-oxygen particles for each detected cage.
- Cage visualization bonds connecting the cage oxygen vertices when the visualization option is enabled.
- `Cage ID` properties on the `water-cages` particles and cage bonds, allowing individual cages to be selected by ID.
- A particle property named `Water Cage Membership`, counting how many detected cages each oxygen belongs to.

Notes:

- Original molecular bonds are not modified. The cage bonds are generated in the separate `water-cages` container.
- Original oxygen particles receive a membership count instead of a single cage ID because one oxygen can belong to multiple cages.
- The general complete cage option is intentionally topology-based. It detects closed cages composed of the allowed ring sizes; it does not classify open/incomplete cage-like motifs as complete cages.

## Ring Finder

`Ring Finder` finds shortest closed rings in the current bond graph. A bond topology must exist upstream, for example from `Create bonds` or `Load topology`.

Main inputs:

- Minimum ring size.
- Maximum ring size.
- Optional polygon facets for visualizing the detected rings.

Outputs:

- Global attributes `RingCount` and `N-RingCount`, where `N` is each ring size in the selected range.
- A `ring-size-histogram` data table with absolute counts by ring size.
- `N-rings` data tables listing the particle indices in each detected ring.
- An optional surface mesh named `rings`, with one polygon facet per detected ring and a face property named `Ring Size`.

License note:

- This native modifier adapts the GPL-3.0-only [ovito-org/RingFinder](https://github.com/ovito-org/RingFinder) extension.
- The upstream triangulation helper is not bundled; polygon facets are emitted as native OVITO mesh faces.

## Transport analysis

`Transport` collects trajectory-wide transport observables for selected ions or molecule groups.

Available observables depend on the selected mode and input data, and may include:

- Mean-squared displacement style Einstein transport quantities.
- Displacement correlation contributions.
- Charge or species-resolved transport contributions.
- Strongly correlated ion-pair statistics.
- Velocity-autocorrelation and Green-Kubo style conductivity estimates when velocity data is available.

The modifier samples the selected trajectory interval only after the run button is clicked.

## Score-based denoising

`Score-based denoising` runs neural-network denoising models inspired by the MIT-licensed [ovito-org/ScoreBasedDenoising](https://github.com/ovito-org/ScoreBasedDenoising) extension. The native modifier keeps the OVITO-side user interface in C++ and delegates model inference to the Python executable selected in the modifier.

Main inputs:

- Structure/material: choose `FCC`, `BCC`, `HCP`, `SiO2`, or `Custom`; `None` leaves the data unchanged.
- Denoising steps: number of iterative denoising updates.
- Nearest-neighbor distance: set to `0` for automatic estimation in the built-in presets; in `Custom` mode this field is used directly as the model scale and must be positive.
- Model path: optional for presets, which use graphite's default denoiser model files when available; required for `Custom`.
- Python executable: Python environment containing `torch`, `torch-geometric`, `graphite`, `e3nn`, `scikit-learn`, and `numpy`.
- Install runtime: choose the PyTorch runtime installed by the setup button. `CPU` installs CPU-only PyTorch; CUDA choices install CUDA-enabled PyTorch wheels from the corresponding PyTorch wheel index.
- Install/repair denoising Python environment: creates or updates a dedicated Python virtual environment, installs the selected PyTorch runtime plus the ScoreBasedDenoising/graphite runtime stack, verifies the imports, and then sets `Python executable` to that environment. CUDA runtimes still require a compatible NVIDIA driver at run time.
- Device: `CPU`, `CUDA`, or `MPS`.
- Write back only selected particles: uses the full configuration as model context but only replaces coordinates of selected particles.

Outputs:

- Updated particle positions.
- `Score-based denoising convergence` and `Score-based denoising log convergence` data tables.
- Global attributes reporting the chosen structure, model scale, model path, updated particle count, and final convergence.

License note:

- The upstream `ovito-org/ScoreBasedDenoising` repository is MIT-licensed.
- Runtime Python packages and pretrained denoiser models are not bundled in this source tree.

## Atomic noising

`Atomic noising` adds controlled Gaussian positional noise to particles. It is a forward perturbation tool, not a learned inverse of the score-based denoiser.

Main inputs:

- Noise scale: choose whether `Amplitude` is an absolute coordinate sigma, a fraction of the nearest-neighbor distance, or a Lindemann-style 3D RMS fraction.
- Noise tensor: choose isotropic noise, diagonal anisotropic noise in Cartesian `X/Y/Z`, or diagonal anisotropic noise along the simulation-cell axes.
- Amplitude X / isotropic, Amplitude Y, Amplitude Z: noise size. Isotropic mode uses only `Amplitude X / isotropic`. Diagonal modes use all three amplitudes. In `Absolute coordinate sigma` mode these are Gaussian sigma values. In `Fraction of nearest-neighbor distance` mode, each sigma is `amplitude * nearest-neighbor distance`. In `Lindemann RMS fraction` mode, each component is divided by `sqrt(3)` so the isotropic case preserves the requested 3D RMS Lindemann fraction.
- Nearest-neighbor distance: set to `0` for automatic estimation from the current particle configuration, or enter a fixed distance.
- Sigma sampling: use a fixed sigma vector, or draw one scalar multiplier uniformly from `0` to the maximum for the current frame while preserving the tensor shape.
- Particle coupling: choose independent particle displacements or rigid molecule displacements. Rigid molecule mode requires `Molecule Identifier` and applies one shared translational displacement to every atom in each molecule, preserving intramolecular geometry.
- Spatially correlate noise: Gaussian-smooths the random displacement field over nearby particles or molecule centers, depending on `Particle coupling`.
- Correlation length: controls the smoothing length `xi`. The implementation uses neighbors within `3*xi`; after smoothing it renormalizes the final RMS displacement to keep the requested noise amplitude meaningful.
- Random seed: deterministic seed for reproducible perturbations.
- Frame-dependent seed: mixes the animation time into the seed so different frames receive different reproducible noise.
- Use only selected particles: in independent mode this displaces selected particles only; in rigid molecule mode any selected atom promotes the whole molecule.
- Wrap into periodic cell: folds displaced particles back into enabled periodic directions.
- Preserve center of mass: subtracts the mean displacement of the noised particles.
- Write noise properties: writes `Noise Displacement`, `Noise Magnitude`, and `Position Before Noising`.

Outputs:

- Updated particle positions.
- Optional particle properties describing the applied displacement.
- Global attributes reporting sigma components, RMS displacement, scale distance, noised particle count, noised molecule count, scale mode, tensor mode, coupling mode, spatial correlation setting, correlation length, and seed.

## Data table plot viewport layer

`Data table plot` is a native m-ovito viewport layer inspired by the MIT-licensed [ovito-org/DataTablePlotOverlay](https://github.com/ovito-org/DataTablePlotOverlay) Python overlay. The upstream package uses the newer `ViewportOverlayInterface` and `ovito.traits` API, which is not available in this source tree, so m-ovito implements the feature as a C++ viewport layer that reuses OVITO's built-in data-table plot renderer.

Main inputs:

- Pipeline: source pipeline that produces the data table.
- Data table: table to draw in the viewport.
- Plot type: auto-detect from the table or force line, histogram, bar chart, scatter, or heatmap/2D table.
- 2D heatmap columns: optional X, Y, value, and boundary-mask column names. Leave them empty to auto-detect common table columns such as `Distance`, `Theta`, `Free energy`, and `In HB basin`.
- Alignment, XY offset, width, height, and opacity.
- Optional title, axis-label, and color-scale-label overrides.
- Optional fixed y-axis range.
- Optional fixed color-scale range for heatmaps.
- Optional minor x/y ticks.
- Optional current-frame marker.

License note:

- The upstream `ovito-org/DataTablePlotOverlay` repository is MIT-licensed.
- This m-ovito implementation does not vendor the upstream Python package or add matplotlib as a dependency.

## Film analysis scripts

The `tools` folder contains standalone Python modifier scripts for film-related post-processing:

- `python_modifier_film_density_profile_time_averaged.py`
- `python_modifier_film_height_vs_time.py`
- `python_modifier_selected_temperature_vs_time.py`
- `python_modifier_vacf.py`
- `python_modifier_wc_cavity_volume_area_vs_time.py`
- `python_modifier_wc_filled_region_volume_area_vs_time.py`

These scripts are source-distributed helpers and are not required for the core OVITO application to start.
