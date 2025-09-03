.. _particles.modifiers.difference_between_frames:

Difference between frames |ovito-pro|
-------------------------------------

.. image:: /images/modifiers/difference_between_frames_panel.png
  :width: 30%
  :align: right

.. versionadded:: 3.14.0

This modifier computes numeric differences (deltas) between the current trajectory frame and a reference frame.
The reference configuration can be either fixed or relative to the current frame (time offset), enabling sliding-window difference calculations.

The user interface of the modifier lets you select one or more input quantities or objects to process simultaneously.
The computed differences are stored as outputs next to the inputs with the suffix *(Difference)* appended. For example,
if you have selected the particle property ``Velocity`` as input, the modifier will create a new property named ``Velocity (Difference)``.
This output property has the same data type and number of components as the input.

The following kinds of inputs are currently supported by the modifier:

Attributes
  :ref:`Global attributes <usage.global_attributes>` are scalar values associated with the dataset as a whole that may vary over time.
  The modifier computes the frame-to-frame difference of such attributes.

Data tables
  The modifier can compute changes in :ref:`data tables <scene_objects.data_table>` (along *y*-axis).
  This requires the *x*-axis values or categories of the data table to remain constant over time.

Voxel grids
  Frame-to-frame differences of :ref:`voxel grid <scene_objects.voxel_grid>` properties are supported,
  provided the grid layout (dimensions and resolution) remains constant between frames.
  The result is added as a new property to the existing voxel grid.

Particles
  The modifier supports calculating time differences of :ref:`particle <scene_objects.particles>` properties.
  See :ref:`particles.modifiers.difference_between_frames.particle_identities` and :ref:`particles.modifiers.difference_between_frames.changing_particle_count` for
  important notes on simulations with changing particle counts or storage order.

  .. caution::

    This modifier does not account for periodic boundary conditions; it performs just a simple subtraction.
    When computing differences of particle coordinates or other spatial properties, this may thus lead to incorrect results.
    Use the :ref:`particles.modifiers.displacement_vectors` modifier instead to calculate proper displacement vectors.

.. _particles.modifiers.difference_between_frames.ref_data:

Reference data
""""""""""""""

By default, the modifier uses data from trajectory frame 0 as the reference frame.
This mode is called :guilabel:`Constant reference configuration` in the user interface.
You can select a different animation frame as the reference if desired.

Alternatively, use :guilabel:`Relative to current frame` to compute incremental differences with a sliding reference frame,
defined by a relative frame offset. Negative offsets reference earlier frames; for example, an offset of -1 compares each frame
to the one immediately before it.

.. _particles.modifiers.difference_between_frames.particle_identities:

Particle identities
"""""""""""""""""""

To match corresponding particles between the current and reference frames and subtract their property values, OVITO requires a one-to-one mapping.
If particles have a ``Particle Identifier`` property, OVITO uses these identifiers for matching.
This ensures correct mapping even if the particle storage order changes.

If unique identifiers are not present, OVITO assumes a fixed storage order throughout the trajectory.
This assumption may be invalid, as some simulation codes reorder particles during the run for performance reasons.

.. caution::

    If your simulation trajectory file does not include particle ID information, OVITO may compute incorrect differences due to changes
    in particle ordering. Some :ref:`file formats <file_formats.input>` preserve order (e.g., XDATCAR), while others (e.g., LAMMPS formats) may not.
    Use the :ref:`data_inspector` to check for the ``Particle Identifier`` property after importing your data.

.. _particles.modifiers.difference_between_frames.changing_particle_count:

Simulations with changing particle numbers
""""""""""""""""""""""""""""""""""""""""""

Generally, the unambiguous computation of property differences requires that the number of particles in the simulation remains constant over time.

If particles are removed in later frames, the modifier still computes differences for the remaining ones. That is, the reference frame may contain more particles than the current frame,
but not vice versa. If new particles appear during the simulation, the modifier raises an error by default, as no reference values can be determined for them.

To explicitly allow dynamic particle creation and override this default behavior, you can enable the following modifier options:

Tolerate newly appearing elements
  Allows the modifier to ignore new particles not present in the reference frame.
  These particles will be assigned a difference value of 0 in the output.

Select newly appearing elements
  Marks particles that have newly appeared in the current frame.
  This is useful for identifying dynamically added particles for which no differences were computed.

.. seealso::

    :py:class:`ovito.modifiers.DifferenceBetweenFramesModifier` (Python API)
