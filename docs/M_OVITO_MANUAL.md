# m-ovito additional modifications manual

This page documents the additions and behavior changes in the public `m-ovito` source tree. For the standard OVITO user manual, see [docs.ovito.org](https://docs.ovito.org/).

Repository: [blualg/m-ovito](https://github.com/blualg/m-ovito)

## General behavior

- The bottom data-inspector help button opens this public manual page when a local generated OVITO manual is not available.
- Custom trajectory-wide analysis modifiers use an explicit `Run` or `Start` button when they need to traverse many frames.
- Long trajectory-wide calculations report progress through OVITO's normal progress/status system instead of opening a separate blocking progress dialog.
- Result tables can be opened from modifier panels with `Show in data inspector` when the modifier produces a data table.

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

## Transport analysis

`Transport` collects trajectory-wide transport observables for selected ions or molecule groups.

Available observables depend on the selected mode and input data, and may include:

- Mean-squared displacement style Einstein transport quantities.
- Displacement correlation contributions.
- Charge or species-resolved transport contributions.
- Strongly correlated ion-pair statistics.
- Velocity-autocorrelation and Green-Kubo style conductivity estimates when velocity data is available.

The modifier samples the selected trajectory interval only after the run button is clicked.

## Film analysis scripts

The `tools` folder contains standalone Python modifier scripts for film-related post-processing:

- `python_modifier_film_density_profile_time_averaged.py`
- `python_modifier_film_height_vs_time.py`
- `python_modifier_selected_temperature_vs_time.py`
- `python_modifier_vacf.py`
- `python_modifier_wc_cavity_volume_area_vs_time.py`
- `python_modifier_wc_filled_region_volume_area_vs_time.py`

These scripts are source-distributed helpers and are not required for the core OVITO application to start.

