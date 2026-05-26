import numpy as np


# -----------------------------------------------------------------------------
# User settings
# -----------------------------------------------------------------------------

# Optional frame range controls.
START_FRAME = 0
STOP_FRAME = None
FRAME_STRIDE = 1

# Time-axis settings used for true time averaging.
# If the upstream frames provide a 'Timestep' global attribute, OVITO will use
# that value and multiply it by SIMULATION_TIMESTEP_SECONDS to obtain the sample
# time in seconds. If not, the script falls back to:
#   frame_index * DUMPING_FREQUENCY * SIMULATION_TIMESTEP_SECONDS
SIMULATION_TIMESTEP_SECONDS = 1.0
DUMPING_FREQUENCY = 1

# Film-particle source:
#   "liquid_phase"  -> build the film mask directly from a phase property
#   "selection"     -> use the upstream Selection property
FILM_PARTICLE_SOURCE = "liquid_phase"

# Direct liquid-particle source settings. By default this uses the
# Willard-Chandler phase output:
#   vapor   = -1
#   surface =  0
#   liquid  =  1
LIQUID_PHASE_PROPERTY_NAME = "Willard-Chandler Phase"
LIQUID_PHASE_VALUES = (1,)

# Number of bins used for the density profile.
BIN_COUNT = 200

# Density profile direction in the global coordinate system.
# Allowed values: "X", "Y", "Z"
PROFILE_AXIS = "Z"

# Length-unit conversion for positions and any manual geometric inputs below.
# Examples:
#   1.0e-10 for Angstrom -> m
#   1.0e-9  for nm -> m
LENGTH_UNIT_TO_M = 1.0e-10

# Mass conversion for the particle Mass property or TYPE_MASSES fallback.
# Leave as the default if Mass values are in amu.
MASS_UNIT_TO_KG = 1.66053906660e-27

# If no upstream Mass property exists, fill in this Particle Type -> mass map.
# Example:
# TYPE_MASSES = {
#     1: 100.0,
#     2: 12.011,
# }
TYPE_MASSES = {}

# Profile-range definition:
#   "manual"             -> use PROFILE_MIN_NATIVE and PROFILE_MAX_NATIVE directly
#   "first_frame_film"   -> determine the binning range from the selected film atoms in the first sampled frame
#   "first_frame_all"    -> determine the binning range from all atoms in the first sampled frame
#
# The runtime of OVITO's Python modifier does not expose the full simulation-cell
# matrix, so this script uses one of the three runtime-safe options above.
PROFILE_RANGE_MODE = "manual"

# Manual profile bounds in the same native length unit as particle coordinates.
PROFILE_MIN_NATIVE = 0.0
PROFILE_MAX_NATIVE = 100.0

# Cross-sectional area settings for converting mass-per-bin into density.
#   "box_lengths" -> compute area from the two perpendicular box lengths below
#   "direct_area" -> use CROSS_SECTION_AREA_M2 directly
CROSS_SECTION_AREA_MODE = "box_lengths"

# Box lengths in native length units.
BOX_LENGTH_X = 1.0
BOX_LENGTH_Y = 1.0
BOX_LENGTH_Z = 1.0

# Directly specified cross-sectional area in m^2 when
# CROSS_SECTION_AREA_MODE == "direct_area".
CROSS_SECTION_AREA_M2 = 1.0


def _axis_index():
    axis = str(PROFILE_AXIS).strip().upper()
    if axis == "X":
        return 0
    if axis == "Y":
        return 1
    if axis == "Z":
        return 2
    raise RuntimeError(f"Unknown PROFILE_AXIS={PROFILE_AXIS!r}. Use 'X', 'Y', or 'Z'.")


def _film_mask(particles):
    if FILM_PARTICLE_SOURCE == "selection":
        if "Selection" not in particles:
            raise RuntimeError(
                "FILM_PARTICLE_SOURCE='selection' requires an upstream 'Selection' particle property."
            )
        return np.asarray(particles["Selection"], dtype=int) != 0

    if FILM_PARTICLE_SOURCE == "liquid_phase":
        if LIQUID_PHASE_PROPERTY_NAME not in particles:
            raise RuntimeError(
                f"FILM_PARTICLE_SOURCE='liquid_phase' requires an upstream "
                f"'{LIQUID_PHASE_PROPERTY_NAME}' particle property."
            )
        phase_values = np.asarray(particles[LIQUID_PHASE_PROPERTY_NAME], dtype=int)
        return np.isin(phase_values, np.asarray(LIQUID_PHASE_VALUES, dtype=int))

    raise RuntimeError(
        f"Unknown FILM_PARTICLE_SOURCE={FILM_PARTICLE_SOURCE!r}. Use 'selection' or 'liquid_phase'."
    )


def _resolve_masses_kg(particles, mask=None):
    if "Mass" in particles:
        masses = np.asarray(particles["Mass"], dtype=float)
        if mask is not None:
            masses = masses[mask]
        return masses * MASS_UNIT_TO_KG

    if "Particle Type" not in particles:
        raise RuntimeError(
            "No 'Mass' particle property is available, and no 'Particle Type' "
            "property exists for TYPE_MASSES fallback."
        )

    if not TYPE_MASSES:
        raise RuntimeError(
            "No 'Mass' particle property is available. Fill TYPE_MASSES in the "
            "script with Particle Type -> mass values."
        )

    type_ids = np.asarray(particles["Particle Type"], dtype=int)
    if mask is not None:
        type_ids = type_ids[mask]

    masses = np.empty(len(type_ids), dtype=float)
    for i, type_id in enumerate(type_ids):
        if type_id not in TYPE_MASSES:
            raise RuntimeError(
                f"Missing mass for Particle Type {type_id}. "
                "Add it to TYPE_MASSES in the script."
            )
        masses[i] = TYPE_MASSES[type_id]
    return masses * MASS_UNIT_TO_KG


def _frame_sequence(num_frames):
    stop_frame = num_frames if STOP_FRAME is None else min(int(STOP_FRAME), num_frames)
    start_frame = max(0, int(START_FRAME))
    stride = max(1, int(FRAME_STRIDE))
    if start_frame >= stop_frame:
        raise RuntimeError(
            f"Empty frame range: START_FRAME={START_FRAME}, STOP_FRAME={STOP_FRAME}, num_frames={num_frames}."
        )
    return list(range(start_frame, stop_frame, stride))


def _frame_step_number(frame_data, frame_index):
    if not hasattr(frame_data, "attributes") or "Timestep" not in frame_data.attributes.keys():
        return int(frame_index) * int(DUMPING_FREQUENCY)

    try:
        timestep_float = float(frame_data.attributes["Timestep"])
    except (TypeError, ValueError):
        return int(frame_index) * int(DUMPING_FREQUENCY)

    if np.isfinite(timestep_float):
        return timestep_float
    return int(frame_index) * int(DUMPING_FREQUENCY)


def _sample_time_weights_seconds(step_numbers):
    times_seconds = np.asarray(step_numbers, dtype=float) * float(SIMULATION_TIMESTEP_SECONDS)
    if times_seconds.size == 0:
        raise RuntimeError("No frames were selected for time averaging.")
    if times_seconds.size == 1:
        return times_seconds, np.asarray([1.0], dtype=float)

    dt = np.diff(times_seconds)
    if np.any(~np.isfinite(dt)) or np.any(dt <= 0.0):
        raise RuntimeError(
            "Frame times are not strictly increasing. "
            "Check the trajectory Timestep values and SIMULATION_TIMESTEP_SECONDS."
        )

    weights = np.empty(times_seconds.size, dtype=float)
    weights[0] = 0.5 * dt[0]
    weights[-1] = 0.5 * dt[-1]
    if times_seconds.size > 2:
        weights[1:-1] = 0.5 * (dt[:-1] + dt[1:])
    return times_seconds, weights


def _cross_section_area_m2():
    mode = str(CROSS_SECTION_AREA_MODE).strip().lower()
    if mode == "direct_area":
        area = float(CROSS_SECTION_AREA_M2)
        if not np.isfinite(area) or area <= 0.0:
            raise RuntimeError("CROSS_SECTION_AREA_M2 must be positive when using direct_area mode.")
        return area

    if mode != "box_lengths":
        raise RuntimeError(
            f"Unknown CROSS_SECTION_AREA_MODE={CROSS_SECTION_AREA_MODE!r}. "
            "Use 'box_lengths' or 'direct_area'."
        )

    lx = float(BOX_LENGTH_X) * float(LENGTH_UNIT_TO_M)
    ly = float(BOX_LENGTH_Y) * float(LENGTH_UNIT_TO_M)
    lz = float(BOX_LENGTH_Z) * float(LENGTH_UNIT_TO_M)
    axis = str(PROFILE_AXIS).strip().upper()

    if axis == "X":
        area = ly * lz
    elif axis == "Y":
        area = lx * lz
    elif axis == "Z":
        area = lx * ly
    else:
        raise RuntimeError(f"Unknown PROFILE_AXIS={PROFILE_AXIS!r}. Use 'X', 'Y', or 'Z'.")

    if not np.isfinite(area) or area <= 0.0:
        raise RuntimeError("The cross-sectional area computed from box lengths is not positive.")
    return area


def _reference_axis_range_native(frame_data):
    if frame_data.particles is None:
        raise RuntimeError("The upstream frame does not contain particles.")
    if "Position" not in frame_data.particles:
        raise RuntimeError("The upstream frame does not contain a 'Position' particle property.")

    mode = str(PROFILE_RANGE_MODE).strip().lower()
    axis_index = _axis_index()
    positions = np.asarray(frame_data.particles["Position"], dtype=float)

    if mode == "manual":
        min_coord = float(PROFILE_MIN_NATIVE)
        max_coord = float(PROFILE_MAX_NATIVE)
    elif mode == "first_frame_all":
        coords = positions[:, axis_index]
        min_coord = float(np.min(coords))
        max_coord = float(np.max(coords))
    elif mode == "first_frame_film":
        mask = _film_mask(frame_data.particles)
        if not np.any(mask):
            raise RuntimeError(
                "PROFILE_RANGE_MODE='first_frame_film' found no selected film atoms in the first sampled frame."
            )
        coords = positions[mask, axis_index]
        min_coord = float(np.min(coords))
        max_coord = float(np.max(coords))
    else:
        raise RuntimeError(
            f"Unknown PROFILE_RANGE_MODE={PROFILE_RANGE_MODE!r}. "
            "Use 'manual', 'first_frame_film', or 'first_frame_all'."
        )

    if not np.isfinite(min_coord) or not np.isfinite(max_coord) or max_coord <= min_coord:
        raise RuntimeError("The chosen profile range is not valid. Make sure PROFILE_MAX_NATIVE > PROFILE_MIN_NATIVE.")
    return min_coord, max_coord


def _prepare_reference_geometry(frame_data):
    min_coord, max_coord = _reference_axis_range_native(frame_data)
    axis_extent = max_coord - min_coord
    if not np.isfinite(axis_extent) or axis_extent <= 0.0:
        raise RuntimeError("The profile extent along PROFILE_AXIS is not positive.")

    bin_edges_native = np.linspace(min_coord, max_coord, int(BIN_COUNT) + 1, dtype=float)
    bin_centers_native = 0.5 * (bin_edges_native[:-1] + bin_edges_native[1:])
    bin_centers_m = bin_centers_native * float(LENGTH_UNIT_TO_M)

    bin_width_m = (axis_extent * float(LENGTH_UNIT_TO_M)) / int(BIN_COUNT)
    bin_volume_m3 = bin_width_m * float(_cross_section_area_m2())

    return {
        "axis_min_native": min_coord,
        "axis_max_native": max_coord,
        "axis_extent_native": axis_extent,
        "bin_edges_native": bin_edges_native,
        "bin_centers_m": bin_centers_m,
        "bin_volume_m3": bin_volume_m3,
    }


def modify(frame, data, upstream, cache, progress):
    frame_indices = _frame_sequence(int(upstream.num_frames))
    cache_key = (
        "film_density_profile_time_averaged",
        int(upstream.num_frames),
        tuple(frame_indices),
        str(FILM_PARTICLE_SOURCE),
        str(LIQUID_PHASE_PROPERTY_NAME),
        tuple(int(v) for v in LIQUID_PHASE_VALUES),
        int(BIN_COUNT),
        str(PROFILE_AXIS),
        float(LENGTH_UNIT_TO_M),
        float(MASS_UNIT_TO_KG),
        tuple(sorted(TYPE_MASSES.items())),
        float(SIMULATION_TIMESTEP_SECONDS),
        int(DUMPING_FREQUENCY),
        str(PROFILE_RANGE_MODE),
        float(PROFILE_MIN_NATIVE),
        float(PROFILE_MAX_NATIVE),
        str(CROSS_SECTION_AREA_MODE),
        float(BOX_LENGTH_X),
        float(BOX_LENGTH_Y),
        float(BOX_LENGTH_Z),
        float(CROSS_SECTION_AREA_M2),
    )

    progress.text("Computing time-averaged film density profile")

    if cache.get("cache_key") != cache_key:
        first_frame = upstream.compute(int(frame_indices[0]))
        reference_geometry = _prepare_reference_geometry(first_frame)

        mass_histograms_kg = []
        count_histograms = []
        step_numbers = []

        total = len(frame_indices)
        for i, frame_index in enumerate(frame_indices):
            progress.fraction((i + 1) / max(1, total))
            progress.text(f"Density profile frame {frame_index} ({i + 1}/{total})")
            progress.check_canceled()

            frame_data = upstream.compute(int(frame_index))
            step_numbers.append(_frame_step_number(frame_data, frame_index))

            if frame_data.particles is None:
                raise RuntimeError("The upstream frame does not contain particles.")
            if "Position" not in frame_data.particles:
                raise RuntimeError("The upstream frame does not contain a 'Position' particle property.")

            particles = frame_data.particles
            mask = _film_mask(particles)
            if not np.any(mask):
                mass_histograms_kg.append(np.zeros(int(BIN_COUNT), dtype=float))
                count_histograms.append(np.zeros(int(BIN_COUNT), dtype=float))
                continue

            coords_native = np.asarray(particles["Position"], dtype=float)[mask, _axis_index()]
            masses_kg = _resolve_masses_kg(particles, mask=mask)

            mass_histograms_kg.append(np.histogram(
                coords_native,
                bins=reference_geometry["bin_edges_native"],
                weights=masses_kg,
            )[0])
            count_histograms.append(np.histogram(
                coords_native,
                bins=reference_geometry["bin_edges_native"],
            )[0])

        times_seconds, time_weights_seconds = _sample_time_weights_seconds(np.asarray(step_numbers, dtype=float))
        total_time_weight_seconds = float(np.sum(time_weights_seconds))
        bin_volume_m3 = float(reference_geometry["bin_volume_m3"])
        average_mass_density = (
            np.tensordot(time_weights_seconds, np.asarray(mass_histograms_kg, dtype=float), axes=(0, 0))
            / (total_time_weight_seconds * bin_volume_m3)
        )
        average_number_density = (
            np.tensordot(time_weights_seconds, np.asarray(count_histograms, dtype=float), axes=(0, 0))
            / (total_time_weight_seconds * bin_volume_m3)
        )

        cache["cache_key"] = cache_key
        cache["frame_indices"] = np.asarray(frame_indices, dtype=int)
        cache["times_seconds"] = times_seconds
        cache["time_weights_seconds"] = time_weights_seconds
        cache["bin_centers_m"] = np.asarray(reference_geometry["bin_centers_m"], dtype=float)
        cache["average_mass_density"] = np.asarray(average_mass_density, dtype=float)
        cache["average_number_density"] = np.asarray(average_number_density, dtype=float)
        cache["bin_volume_m3"] = bin_volume_m3
        cache["axis_min_native"] = float(reference_geometry["axis_min_native"])
        cache["axis_max_native"] = float(reference_geometry["axis_max_native"])
        cache["time_average_duration_seconds"] = total_time_weight_seconds

    axis = str(PROFILE_AXIS).strip().upper()
    progress.text(f"Writing time-averaged density profile at frame {frame}")

    data.create_table(
        "Time-averaged film density profile",
        x=np.asarray(cache["bin_centers_m"], dtype=float),
        y=np.column_stack(
            (
                np.asarray(cache["average_mass_density"], dtype=float),
                np.asarray(cache["average_number_density"], dtype=float),
            )
        ),
        title="Time-averaged film density profile",
        axis_label_x=f"Position along {axis} (m)",
        axis_label_y="Average film density",
        y_component_names=[
            "Mass density (kg/m^3)",
            "Number density (1/m^3)",
        ],
    )

    data.attributes["PythonModifier.density_profile_axis"] = axis
    data.attributes["PythonModifier.density_profile_bin_count"] = int(BIN_COUNT)
    data.attributes["PythonModifier.density_profile_frame_count"] = int(len(cache["frame_indices"]))
    data.attributes["PythonModifier.density_profile_bin_volume_m3"] = float(cache["bin_volume_m3"])
    data.attributes["PythonModifier.density_profile_time_average_duration_seconds"] = float(
        cache["time_average_duration_seconds"]
    )
    data.attributes["PythonModifier.density_profile_axis_min_native"] = float(cache["axis_min_native"])
    data.attributes["PythonModifier.density_profile_axis_max_native"] = float(cache["axis_max_native"])
    data.attributes["PythonModifier.density_profile_range_mode"] = str(PROFILE_RANGE_MODE)
    data.attributes["PythonModifier.density_profile_cross_section_area_m2"] = float(_cross_section_area_m2())
    data.attributes["PythonModifier.film_particle_source"] = str(FILM_PARTICLE_SOURCE)
    data.attributes["PythonModifier.liquid_phase_property"] = str(LIQUID_PHASE_PROPERTY_NAME)
    data.attributes["PythonModifier.simulation_timestep_seconds"] = float(SIMULATION_TIMESTEP_SECONDS)
    data.attributes["PythonModifier.dumping_frequency"] = int(DUMPING_FREQUENCY)

    progress.text(f"Wrote time-averaged density profile using {len(cache['frame_indices'])} frame(s)")
