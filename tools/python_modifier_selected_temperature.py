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


def modify(frame, data, upstream, cache, progress):
    progress.text(f"Computing selected-atom temperature at frame {frame}")

    if data.particles is None:
        raise RuntimeError("The current pipeline output does not contain particles.")

    particles = data.particles

    if "Velocity" not in particles:
        raise RuntimeError("The current pipeline output does not contain a 'Velocity' particle property.")

    selection = _temperature_mask(particles)
    selected_count = int(np.count_nonzero(selection))

    if selected_count == 0:
        data.attributes["PythonModifier.selected_temperature_K"] = float("nan")
        data.attributes["PythonModifier.selected_atom_count"] = 0
        data.attributes["PythonModifier.selected_degrees_of_freedom"] = 0
        data.attributes["PythonModifier.selected_drift_vx_m_per_s"] = 0.0
        data.attributes["PythonModifier.selected_drift_vy_m_per_s"] = 0.0
        data.attributes["PythonModifier.selected_drift_vz_m_per_s"] = 0.0
        data.attributes["PythonModifier.selected_kinetic_energy_J"] = 0.0
        progress.text("No atoms are selected.")
        return

    masses_kg = _resolve_masses_kg(particles)[selection]
    velocities_mps = np.asarray(particles["Velocity"], dtype=float)[selection] * VELOCITY_UNIT_TO_M_PER_S

    total_mass = float(masses_kg.sum())
    if not np.isfinite(total_mass) or total_mass <= 0.0:
        raise RuntimeError("The total selected mass is not positive.")

    # Subtract the mass-weighted drift velocity of the selected atoms.
    drift_velocity = (masses_kg[:, None] * velocities_mps).sum(axis=0) / total_mass
    thermal_velocities = velocities_mps - drift_velocity

    kinetic_energy_joule = 0.5 * float(np.sum(masses_kg * np.sum(thermal_velocities**2, axis=1)))

    # Remove three translational degrees of freedom because the bulk drift was subtracted.
    degrees_of_freedom = 3 * selected_count - 3
    if degrees_of_freedom <= 0:
        raise RuntimeError(
            f"Need at least two selected atoms after removing drift degrees of freedom, got {selected_count}."
        )

    temperature_kelvin = (2.0 * kinetic_energy_joule) / (degrees_of_freedom * K_B)

    data.attributes["PythonModifier.selected_temperature_K"] = float(temperature_kelvin)
    data.attributes["PythonModifier.selected_atom_count"] = selected_count
    data.attributes["PythonModifier.selected_degrees_of_freedom"] = int(degrees_of_freedom)
    data.attributes["PythonModifier.selected_drift_vx_m_per_s"] = float(drift_velocity[0])
    data.attributes["PythonModifier.selected_drift_vy_m_per_s"] = float(drift_velocity[1])
    data.attributes["PythonModifier.selected_drift_vz_m_per_s"] = float(drift_velocity[2])
    data.attributes["PythonModifier.selected_kinetic_energy_J"] = float(kinetic_energy_joule)
    data.attributes["PythonModifier.temperature_particle_source"] = str(TEMPERATURE_PARTICLE_SOURCE)
    data.attributes["PythonModifier.liquid_phase_property"] = str(LIQUID_PHASE_PROPERTY_NAME)

    data.create_table(
        "Selected temperature",
        x=np.asarray([0.0], dtype=float),
        y=np.asarray([[temperature_kelvin]], dtype=float),
        title="Selected temperature",
        axis_label_x="sample",
        axis_label_y="Temperature (K)",
        y_component_names=["Temperature"],
    )

    progress.text(f"Selected temperature = {temperature_kelvin:.6g} K")
