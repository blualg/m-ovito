import numpy as np


# -----------------------------------------------------------------------------
# User settings
# -----------------------------------------------------------------------------
#
# Mass conversion:
#   Set this to convert the mass values used by the script into kilograms.
#   If your upstream pipeline provides a 'Mass' particle property in atomic mass
#   units (amu), leave this as the default below.
#
MASS_UNIT_TO_KG = 1.66053906660e-27  # amu -> kg

# Velocity conversion:
#   Set this to convert the 'Velocity' particle property into m/s.
#   Common examples:
#     100.0   for Angstrom / ps   (e.g. LAMMPS metal units)
#     1.0e5   for Angstrom / fs   (e.g. LAMMPS real units)
#     1.0     for m / s
#
VELOCITY_UNIT_TO_M_PER_S = 100.0

# The MD integration time step in seconds.
#
# Example:
#   2.0e-15 for 2 fs
#   1.0e-12 for 1 ps
#
SIMULATION_TIMESTEP_SECONDS = 1.0

# Number of MD steps between two saved trajectory frames.
#
# Example:
#   1000 if the trajectory is dumped every 1000 timesteps
#
DUMPING_FREQUENCY = 1

# Optional frame range controls.
# Leave STOP_FRAME = None to use the full trajectory.
START_FRAME = 0
STOP_FRAME = None
FRAME_STRIDE = 1

# Temperature particle source:
#   "liquid_phase"  -> build the temperature mask directly from a phase property
#   "selection"     -> use the upstream Selection property
#
TEMPERATURE_PARTICLE_SOURCE = "liquid_phase"

# Direct liquid-particle source settings. By default this uses the
# Willard-Chandler phase output:
#   vapor   = -1
#   surface =  0
#   liquid  =  1
#
# Set LIQUID_PHASE_VALUES to (1,) for bulk-liquid only, or e.g. (0, 1) if you
# want liquid + interfacial atoms included in the temperature calculation.
#
LIQUID_PHASE_PROPERTY_NAME = "Willard-Chandler Phase"
LIQUID_PHASE_VALUES = (1,)

# HFE-7000 / Novec 7000 heat-capacity settings.
# Default c_p is evaluated at T0 = 300 K using the 3M Novec 7000 datasheet fit:
#   c_p [J kg^-1 K^-1] = 1223.2 + 3.0803 * T[degC]
# In the heat-transfer expressions below, m_f is taken as the summed mass of
# the liquid-mask atoms used for the film temperature calculation.
#
REFERENCE_TEMPERATURE_K = 300.0
SPECIFIC_HEAT_CP_J_PER_KG_K = 1223.2 + 3.0803 * (REFERENCE_TEMPERATURE_K - 273.15)

# Cross-sectional area settings for heat-flux calculations.
# "box_lengths": compute area from the two box lengths perpendicular to the chosen normal
# "direct_area": use CROSS_SECTION_AREA_M2 directly
#
CROSS_SECTION_AREA_MODE = "box_lengths"

# Chosen normal direction for the film cross section.
# Allowed values: "X", "Y", "Z"
FILM_NORMAL_AXIS = "Z"

# Box lengths for the cross-sectional area calculation when
# CROSS_SECTION_AREA_MODE == "box_lengths".
# Set these to the simulation box lengths in your working length unit.
BOX_LENGTH_X = 1.0
BOX_LENGTH_Y = 1.0
BOX_LENGTH_Z = 1.0

# Length-unit conversion for the box lengths above.
# Example:
#   1.0e-10 for Angstrom -> m
#   1.0e-9  for nm -> m
#
LENGTH_UNIT_TO_M = 1.0e-10

# Directly specified cross-sectional area in m^2 when
# CROSS_SECTION_AREA_MODE == "direct_area".
CROSS_SECTION_AREA_M2 = 1.0

# Vapor fraction settings:
#   The script can also compute vapor_atoms / total_atoms for each frame using
#   a phase-classification particle property produced upstream.
#
#   By default this expects the Willard-Chandler modifier output:
#     vapor   = -1
#     surface =  0
#     liquid  =  1
#
VAPOR_PHASE_PROPERTY_NAME = "Willard-Chandler Phase"
VAPOR_PHASE_VALUE = -1

# Optional particle mask for the vapor fraction calculation.
# If not None, only atoms with mask != 0 are counted in both the vapor count
# and total count. Leave as None to use all particles.
#
# Example:
#   VAPOR_COUNT_MASK_PROPERTY_NAME = "Fluid Selection"
#
VAPOR_COUNT_MASK_PROPERTY_NAME = None

# Boltzmann constant in J/K.
K_B = 1.380649e-23

# If the upstream data does not provide a 'Mass' property, the script can fall
# back to this Particle Type -> mass mapping.
# Leave it empty if a Mass property already exists upstream.
#
# Example:
# TYPE_MASSES = {
#     1: 15.9994,  # oxygen in amu
#     2: 1.008,    # hydrogen in amu
# }
#
TYPE_MASSES = {}


def _normal_axis_index():
    axis = str(FILM_NORMAL_AXIS).strip().upper()
    if axis == "X":
        return 0
    if axis == "Y":
        return 1
    if axis == "Z":
        return 2
    raise RuntimeError(f"Unknown FILM_NORMAL_AXIS={FILM_NORMAL_AXIS!r}. Use 'X', 'Y', or 'Z'.")


def _temperature_mask(particles):
    if TEMPERATURE_PARTICLE_SOURCE == "selection":
        if "Selection" not in particles:
            raise RuntimeError(
                "TEMPERATURE_PARTICLE_SOURCE='selection' requires an upstream "
                "'Selection' particle property."
            )
        return np.asarray(particles["Selection"], dtype=int) != 0

    if TEMPERATURE_PARTICLE_SOURCE == "liquid_phase":
        if LIQUID_PHASE_PROPERTY_NAME not in particles:
            raise RuntimeError(
                f"TEMPERATURE_PARTICLE_SOURCE='liquid_phase' requires an upstream "
                f"'{LIQUID_PHASE_PROPERTY_NAME}' particle property."
            )
        phase_values = np.asarray(particles[LIQUID_PHASE_PROPERTY_NAME], dtype=int)
        return np.isin(phase_values, np.asarray(LIQUID_PHASE_VALUES, dtype=int))

    raise RuntimeError(
        f"Unknown TEMPERATURE_PARTICLE_SOURCE={TEMPERATURE_PARTICLE_SOURCE!r}. "
        "Use 'selection' or 'liquid_phase'."
    )


def _resolve_masses_kg(particles):
    if "Mass" in particles:
        masses = np.asarray(particles["Mass"], dtype=float)
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
    masses = np.empty(len(type_ids), dtype=float)
    for i, type_id in enumerate(type_ids):
        if type_id not in TYPE_MASSES:
            raise RuntimeError(
                f"Missing mass for Particle Type {type_id}. "
                "Add it to TYPE_MASSES in the script."
            )
        masses[i] = TYPE_MASSES[type_id]
    return masses * MASS_UNIT_TO_KG


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
    axis = str(FILM_NORMAL_AXIS).strip().upper()

    if axis == "X":
        area = ly * lz
    elif axis == "Y":
        area = lx * lz
    elif axis == "Z":
        area = lx * ly
    else:
        raise RuntimeError(f"Unknown FILM_NORMAL_AXIS={FILM_NORMAL_AXIS!r}. Use 'X', 'Y', or 'Z'.")

    if not np.isfinite(area) or area <= 0.0:
        raise RuntimeError("The cross-sectional area computed from box lengths is not positive.")
    return area


def _frame_temperature(frame_data):
    if frame_data.particles is None:
        raise RuntimeError("The upstream frame does not contain particles.")

    particles = frame_data.particles
    if "Velocity" not in particles:
        raise RuntimeError("The upstream frame does not contain a 'Velocity' particle property.")

    selection = _temperature_mask(particles)
    selected_count = int(np.count_nonzero(selection))

    if selected_count == 0:
        return {
            "temperature_K": np.nan,
            "selected_count": 0,
            "degrees_of_freedom": 0,
            "drift_velocity_m_per_s": np.zeros(3, dtype=float),
            "thermal_kinetic_energy_J": 0.0,
            "total_kinetic_energy_J": 0.0,
            "film_mass_kg": 0.0,
        }

    masses_kg = _resolve_masses_kg(particles)[selection]
    velocities_mps = np.asarray(particles["Velocity"], dtype=float)[selection] * VELOCITY_UNIT_TO_M_PER_S

    total_mass = float(masses_kg.sum())
    if not np.isfinite(total_mass) or total_mass <= 0.0:
        raise RuntimeError("The total selected mass is not positive.")

    drift_velocity = (masses_kg[:, None] * velocities_mps).sum(axis=0) / total_mass
    thermal_velocities = velocities_mps - drift_velocity
    thermal_kinetic_energy_joule = 0.5 * float(np.sum(masses_kg * np.sum(thermal_velocities**2, axis=1)))
    total_kinetic_energy_joule = 0.5 * float(np.sum(masses_kg * np.sum(velocities_mps**2, axis=1)))

    degrees_of_freedom = 3 * selected_count - 3
    if degrees_of_freedom <= 0:
        return {
            "temperature_K": np.nan,
            "selected_count": selected_count,
            "degrees_of_freedom": int(degrees_of_freedom),
            "drift_velocity_m_per_s": drift_velocity,
            "thermal_kinetic_energy_J": thermal_kinetic_energy_joule,
            "total_kinetic_energy_J": total_kinetic_energy_joule,
            "film_mass_kg": total_mass,
        }

    temperature_kelvin = (2.0 * thermal_kinetic_energy_joule) / (degrees_of_freedom * K_B)
    return {
        "temperature_K": float(temperature_kelvin),
        "selected_count": selected_count,
        "degrees_of_freedom": int(degrees_of_freedom),
        "drift_velocity_m_per_s": drift_velocity,
        "thermal_kinetic_energy_J": thermal_kinetic_energy_joule,
        "total_kinetic_energy_J": total_kinetic_energy_joule,
        "film_mass_kg": total_mass,
    }


def _frame_vapor_fraction(frame_data):
    if frame_data.particles is None:
        raise RuntimeError("The upstream frame does not contain particles.")

    particles = frame_data.particles
    if VAPOR_PHASE_PROPERTY_NAME not in particles:
        raise RuntimeError(
            f"This script expects an upstream '{VAPOR_PHASE_PROPERTY_NAME}' particle property "
            "for the vapor-fraction calculation."
        )

    phase_values = np.asarray(particles[VAPOR_PHASE_PROPERTY_NAME], dtype=int)
    mask = np.ones(len(phase_values), dtype=bool)

    if VAPOR_COUNT_MASK_PROPERTY_NAME is not None:
        if VAPOR_COUNT_MASK_PROPERTY_NAME not in particles:
            raise RuntimeError(
                f"The configured vapor-count mask property '{VAPOR_COUNT_MASK_PROPERTY_NAME}' "
                "does not exist upstream."
            )
        mask = np.asarray(particles[VAPOR_COUNT_MASK_PROPERTY_NAME], dtype=int) != 0

    total_count = int(np.count_nonzero(mask))
    if total_count == 0:
        return {
            "vapor_count": 0,
            "total_count": 0,
            "vapor_fraction": np.nan,
        }

    vapor_count = int(np.count_nonzero(mask & (phase_values == int(VAPOR_PHASE_VALUE))))
    vapor_fraction = float(vapor_count / total_count)
    return {
        "vapor_count": vapor_count,
        "total_count": total_count,
        "vapor_fraction": vapor_fraction,
    }


def _frame_film_height(frame_data):
    if frame_data.particles is None:
        raise RuntimeError("The upstream frame does not contain particles.")

    particles = frame_data.particles
    if "Position" not in particles:
        raise RuntimeError("The upstream frame does not contain a 'Position' particle property.")

    selection = _temperature_mask(particles)
    selected_count = int(np.count_nonzero(selection))
    if selected_count == 0:
        return {
            "film_height_native": np.nan,
            "film_height_m": np.nan,
        }

    coords = np.asarray(particles["Position"], dtype=float)[selection, _normal_axis_index()]
    min_coord = float(np.min(coords))
    max_coord = float(np.max(coords))
    height_native = max_coord - min_coord
    return {
        "film_height_native": height_native,
        "film_height_m": height_native * float(LENGTH_UNIT_TO_M),
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
        "selected_temperature_vs_time",
        int(upstream.num_frames),
        tuple(frame_indices),
        float(MASS_UNIT_TO_KG),
        float(VELOCITY_UNIT_TO_M_PER_S),
        float(SIMULATION_TIMESTEP_SECONDS),
        int(DUMPING_FREQUENCY),
        tuple(sorted(TYPE_MASSES.items())),
        str(TEMPERATURE_PARTICLE_SOURCE),
        str(LIQUID_PHASE_PROPERTY_NAME),
        tuple(int(v) for v in LIQUID_PHASE_VALUES),
        float(REFERENCE_TEMPERATURE_K),
        float(SPECIFIC_HEAT_CP_J_PER_KG_K),
        str(CROSS_SECTION_AREA_MODE),
        str(FILM_NORMAL_AXIS),
        float(BOX_LENGTH_X),
        float(BOX_LENGTH_Y),
        float(BOX_LENGTH_Z),
        float(LENGTH_UNIT_TO_M),
        float(CROSS_SECTION_AREA_M2),
        str(VAPOR_PHASE_PROPERTY_NAME),
        int(VAPOR_PHASE_VALUE),
        None if VAPOR_COUNT_MASK_PROPERTY_NAME is None else str(VAPOR_COUNT_MASK_PROPERTY_NAME),
    )

    progress.text("Computing selected-atom temperature vs time")

    if cache.get("cache_key") != cache_key:
        temperatures = []
        film_kinetic_energies_j = []
        film_masses = []
        film_heights_m = []
        vapor_fractions = []
        step_numbers = []

        total = len(frame_indices)
        for i, frame_index in enumerate(frame_indices):
            progress.fraction((i + 1) / max(1, total))
            progress.text(f"Temperature frame {frame_index} ({i + 1}/{total})")
            progress.check_canceled()

            frame_data = upstream.compute(frame_index)
            step_numbers.append(_frame_step_number(frame_data, frame_index))
            temperature_result = _frame_temperature(frame_data)
            vapor_result = _frame_vapor_fraction(frame_data)
            film_height_result = _frame_film_height(frame_data)
            temperatures.append(temperature_result["temperature_K"])
            film_kinetic_energies_j.append(temperature_result["thermal_kinetic_energy_J"])
            film_masses.append(temperature_result["film_mass_kg"])
            film_heights_m.append(film_height_result["film_height_m"])
            vapor_fractions.append(vapor_result["vapor_fraction"])

        step_numbers = np.asarray(step_numbers, dtype=float)
        times_seconds = step_numbers * float(SIMULATION_TIMESTEP_SECONDS)
        film_kinetic_energies_j = np.asarray(film_kinetic_energies_j, dtype=float)
        film_masses = np.asarray(film_masses, dtype=float)
        film_heights_m = np.asarray(film_heights_m, dtype=float)
        temperatures = np.asarray(temperatures, dtype=float)
        area_m2 = float(_cross_section_area_m2())
        safe_temperatures = np.nan_to_num(
            temperatures,
            nan=float(REFERENCE_TEMPERATURE_K),
            posinf=float(REFERENCE_TEMPERATURE_K),
            neginf=float(REFERENCE_TEMPERATURE_K),
        )
        sensible_heat_j = film_masses * float(SPECIFIC_HEAT_CP_J_PER_KG_K) * np.maximum(
            safe_temperatures - float(REFERENCE_TEMPERATURE_K), 0.0
        )
        sensible_heat_density_j_m2 = sensible_heat_j / area_m2

        heat_flux_w_m2 = np.full(len(frame_indices), np.nan, dtype=float)
        cumulative_film_kinetic_energy_j = film_kinetic_energies_j - film_kinetic_energies_j[0]
        cumulative_heat_flux_j_m2 = sensible_heat_density_j_m2 - sensible_heat_density_j_m2[0]

        if len(frame_indices) >= 2:
            dt_seconds = np.diff(times_seconds)
            valid = dt_seconds > 0.0
            heat_flux_w_m2[1:][valid] = np.diff(sensible_heat_density_j_m2)[valid] / dt_seconds[valid]

        cache["cache_key"] = cache_key
        cache["frame_indices"] = np.asarray(frame_indices, dtype=int)
        cache["step_numbers"] = step_numbers
        cache["times_seconds"] = times_seconds
        cache["temperatures"] = temperatures
        cache["cross_section_area_m2"] = area_m2
        cache["film_kinetic_energies_j"] = film_kinetic_energies_j
        cache["film_heights_m"] = film_heights_m
        cache["cumulative_film_kinetic_energy_j"] = cumulative_film_kinetic_energy_j
        cache["heat_flux_w_m2"] = heat_flux_w_m2
        cache["cumulative_heat_flux_j_m2"] = cumulative_heat_flux_j_m2
        cache["vapor_fractions"] = np.asarray(vapor_fractions, dtype=float)

    progress.text(f"Writing selected temperature table at frame {frame}")

    data.create_table(
        "Selected temperature vs time",
        x=np.asarray(cache["times_seconds"], dtype=float),
        y=np.column_stack(
            (
                np.asarray(cache["temperatures"], dtype=float),
                np.asarray(cache["film_heights_m"], dtype=float),
                np.asarray(cache["film_kinetic_energies_j"], dtype=float),
                np.asarray(cache["cumulative_film_kinetic_energy_j"], dtype=float),
                np.asarray(cache["vapor_fractions"], dtype=float),
                np.asarray(cache["heat_flux_w_m2"], dtype=float),
                np.asarray(cache["cumulative_heat_flux_j_m2"], dtype=float),
            )
        ),
        title="Selected temperature vs time",
        axis_label_x="Time (s)",
        axis_label_y="Film temperature / heat-flux metrics",
        y_component_names=[
            "Temperature (K)",
            "Film height (m)",
            "Film kinetic energy (J)",
            "Film cumulative kinetic energy (J)",
            "Vapor fraction",
            "Film heat flux (W/m^2)",
            "Film cumulative heat flux (J/m^2)",
        ],
    )

    current_index = None
    if int(frame) in set(int(i) for i in cache["frame_indices"]):
        current_index = int(np.where(cache["frame_indices"] == int(frame))[0][0])

    if current_index is not None:
        data.attributes["PythonModifier.selected_temperature_K"] = float(cache["temperatures"][current_index])
        data.attributes["PythonModifier.film_height_m"] = float(cache["film_heights_m"][current_index])
        data.attributes["PythonModifier.film_kinetic_energy_J"] = float(cache["film_kinetic_energies_j"][current_index])
        data.attributes["PythonModifier.film_cumulative_kinetic_energy_J"] = float(
            cache["cumulative_film_kinetic_energy_j"][current_index]
        )
        data.attributes["PythonModifier.vapor_fraction"] = float(cache["vapor_fractions"][current_index])
        data.attributes["PythonModifier.film_heat_flux_W_per_m2"] = float(cache["heat_flux_w_m2"][current_index])
        data.attributes["PythonModifier.film_cumulative_heat_flux_J_per_m2"] = float(
            cache["cumulative_heat_flux_j_m2"][current_index]
        )

    data.attributes["PythonModifier.temperature_time_series_frame_count"] = int(len(cache["frame_indices"]))
    data.attributes["PythonModifier.simulation_timestep_seconds"] = float(SIMULATION_TIMESTEP_SECONDS)
    data.attributes["PythonModifier.dumping_frequency"] = int(DUMPING_FREQUENCY)
    data.attributes["PythonModifier.temperature_particle_source"] = str(TEMPERATURE_PARTICLE_SOURCE)
    data.attributes["PythonModifier.liquid_phase_property"] = str(LIQUID_PHASE_PROPERTY_NAME)
    data.attributes["PythonModifier.reference_temperature_K"] = float(REFERENCE_TEMPERATURE_K)
    data.attributes["PythonModifier.specific_heat_cp_J_per_kg_K"] = float(SPECIFIC_HEAT_CP_J_PER_KG_K)
    data.attributes["PythonModifier.cross_section_area_m2"] = float(cache["cross_section_area_m2"])
    data.attributes["PythonModifier.film_normal_axis"] = str(FILM_NORMAL_AXIS)
    data.attributes["PythonModifier.vapor_phase_property"] = str(VAPOR_PHASE_PROPERTY_NAME)
    data.attributes["PythonModifier.vapor_phase_value"] = int(VAPOR_PHASE_VALUE)

    progress.text(f"Wrote selected temperature vs time for {len(cache['frame_indices'])} frame(s)")
