import numpy as np


# -----------------------------------------------------------------------------
# User settings
# -----------------------------------------------------------------------------

# The MD integration time step in seconds.
SIMULATION_TIMESTEP_SECONDS = 1.0

# Number of MD steps between two saved trajectory frames.
DUMPING_FREQUENCY = 1

# Optional frame range controls.
START_FRAME = 0
STOP_FRAME = None
FRAME_STRIDE = 1

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

# Film height is measured along this axis.
# Allowed values: "X", "Y", "Z"
FILM_NORMAL_AXIS = "Z"

# Coordinate conversion for the reported height in meters.
# Example:
#   1.0e-10 for Angstrom -> m
#   1.0e-9  for nm -> m
LENGTH_UNIT_TO_M = 1.0e-10


def _normal_axis_index():
    axis = str(FILM_NORMAL_AXIS).strip().upper()
    if axis == "X":
        return 0
    if axis == "Y":
        return 1
    if axis == "Z":
        return 2
    raise RuntimeError(f"Unknown FILM_NORMAL_AXIS={FILM_NORMAL_AXIS!r}. Use 'X', 'Y', or 'Z'.")


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


def _frame_height(frame_data):
    if frame_data.particles is None:
        raise RuntimeError("The upstream frame does not contain particles.")

    particles = frame_data.particles
    if "Position" not in particles:
        raise RuntimeError("The upstream frame does not contain a 'Position' particle property.")

    mask = _film_mask(particles)
    count = int(np.count_nonzero(mask))
    if count == 0:
        return {
            "count": 0,
            "height_native": np.nan,
            "height_m": np.nan,
        }

    coords = np.asarray(particles["Position"], dtype=float)[mask, _normal_axis_index()]
    min_coord = float(np.min(coords))
    max_coord = float(np.max(coords))
    height_native = max_coord - min_coord
    return {
        "count": count,
        "height_native": height_native,
        "height_m": height_native * float(LENGTH_UNIT_TO_M),
    }


def _frame_step_number(frame_data, frame_index):
    if "Timestep" not in frame_data.attributes.keys():
        return int(frame_index) * int(DUMPING_FREQUENCY)

    try:
        timestep_float = float(frame_data.attributes["Timestep"])
    except (TypeError, ValueError):
        return int(frame_index) * int(DUMPING_FREQUENCY)

    if np.isfinite(timestep_float):
        return timestep_float
    return int(frame_index) * int(DUMPING_FREQUENCY)


def _frame_sequence(num_frames):
    stop_frame = num_frames if STOP_FRAME is None else min(int(STOP_FRAME), num_frames)
    start_frame = max(0, int(START_FRAME))
    stride = max(1, int(FRAME_STRIDE))
    if start_frame >= stop_frame:
        raise RuntimeError(
            f"Empty frame range: START_FRAME={START_FRAME}, STOP_FRAME={STOP_FRAME}, num_frames={num_frames}."
        )
    return list(range(start_frame, stop_frame, stride))


def modify(frame, data, upstream, cache, progress):
    frame_indices = _frame_sequence(int(upstream.num_frames))
    cache_key = (
        "film_height_vs_time",
        int(upstream.num_frames),
        tuple(frame_indices),
        float(SIMULATION_TIMESTEP_SECONDS),
        int(DUMPING_FREQUENCY),
        str(FILM_PARTICLE_SOURCE),
        str(LIQUID_PHASE_PROPERTY_NAME),
        tuple(int(v) for v in LIQUID_PHASE_VALUES),
        str(FILM_NORMAL_AXIS),
        float(LENGTH_UNIT_TO_M),
    )

    progress.text("Computing film height vs time")

    if cache.get("cache_key") != cache_key:
        heights_m = []
        counts = []
        step_numbers = []

        total = len(frame_indices)
        for i, frame_index in enumerate(frame_indices):
            progress.fraction((i + 1) / max(1, total))
            progress.text(f"Film height frame {frame_index} ({i + 1}/{total})")
            progress.check_canceled()

            frame_data = upstream.compute(frame_index)
            step_numbers.append(_frame_step_number(frame_data, frame_index))
            height_result = _frame_height(frame_data)
            heights_m.append(height_result["height_m"])
            counts.append(height_result["count"])

        step_numbers = np.asarray(step_numbers, dtype=float)
        times_seconds = step_numbers * float(SIMULATION_TIMESTEP_SECONDS)
        heights_m = np.asarray(heights_m, dtype=float)
        counts = np.asarray(counts, dtype=int)

        cache["cache_key"] = cache_key
        cache["frame_indices"] = np.asarray(frame_indices, dtype=int)
        cache["times_seconds"] = times_seconds
        cache["heights_m"] = heights_m
        cache["counts"] = counts

    progress.text(f"Writing film height table at frame {frame}")

    data.create_table(
        "Film height vs time",
        x=np.asarray(cache["times_seconds"], dtype=float),
        y=np.column_stack(
            (
                np.asarray(cache["heights_m"], dtype=float),
                np.asarray(cache["counts"], dtype=float),
            )
        ),
        title="Film height vs time",
        axis_label_x="Time (s)",
        axis_label_y="Film height metrics",
        y_component_names=[
            "Film height (m)",
            "Film atom count",
        ],
    )

    current_index = None
    if int(frame) in set(int(i) for i in cache["frame_indices"]):
        current_index = int(np.where(cache["frame_indices"] == int(frame))[0][0])

    if current_index is not None:
        data.attributes["PythonModifier.film_height_m"] = float(cache["heights_m"][current_index])
        data.attributes["PythonModifier.film_atom_count"] = int(cache["counts"][current_index])

    data.attributes["PythonModifier.film_height_time_series_frame_count"] = int(len(cache["frame_indices"]))
    data.attributes["PythonModifier.simulation_timestep_seconds"] = float(SIMULATION_TIMESTEP_SECONDS)
    data.attributes["PythonModifier.dumping_frequency"] = int(DUMPING_FREQUENCY)
    data.attributes["PythonModifier.film_particle_source"] = str(FILM_PARTICLE_SOURCE)
    data.attributes["PythonModifier.liquid_phase_property"] = str(LIQUID_PHASE_PROPERTY_NAME)
    data.attributes["PythonModifier.film_normal_axis"] = str(FILM_NORMAL_AXIS)

    progress.text(f"Wrote film height vs time for {len(cache['frame_indices'])} frame(s)")
