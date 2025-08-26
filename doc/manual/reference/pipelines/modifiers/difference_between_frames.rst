.. _particles.modifiers.difference_between_frames:

Difference between frames |ovito-pro|
-------------------------------------

This modifier computes the difference between a selected property or attribute in the current frame and a reference frame.
The reference frame can be defined either as a fixed frame or as a relative offset, enabling sliding-window difference calculations.

If the input is named ``<name>``, the output is stored in a new property named ``<name> (Difference)``.
The result has the same data type and number of components as the input. The following kinds of input quantities are currently supported:

Attributes
  :ref:`Global attributes <usage.global_attributes>` are scalar values associated with the dataset that may vary over time.
  The modifier computes the frame-to-frame difference of such attributes.

Data tables
  The modifier can compute differences of properties (columns) stored in :ref:`data tables <scene_objects.data_table>`.
  This requires the *x*-values of the input and reference tables to match exactly.

Voxel grids
  Frame-to-frame differences of :ref:`voxel grid <scene_objects.voxel_grid>` properties are supported,
  provided the grid layout (dimensions and resolution) remains constant between frames.
  The result is added as a new property to the existing voxel grid.

Particles
  The modifier supports frame-wise differences of :ref:`particle <scene_objects.particles>` properties.
  See the sections below for important notes on changing particle numbers and storage order.

  .. caution::

      This modifier does not account for periodic boundary conditions or apply the minimum image convention.
      When computing differences of particle positions or other spatial properties, this may produce incorrect results.
      Use :ref:`particles.modifiers.displacement_vectors` for proper displacement calculations.

Reference data
""""""""""""""

By default, the modifier uses data from simulation timestep 0 as the reference frame.
This mode is called :guilabel:`Constant reference configuration` in the user interface.
You can select a different animation frame as the reference if desired.

Alternatively, use :guilabel:`Relative to current frame` to compute incremental differences with a sliding reference frame,
defined by a relative time offset. Negative offsets reference earlier frames; for example, an offset of -1 compares each frame
to the one immediately before it.

Particle identities
"""""""""""""""""""

To match particles between the current and reference frames, OVITO requires a one-to-one mapping.
If particles have a ``Particle Identifier`` property, OVITO uses these identifiers for matching.
This ensures correct mapping even if the particle storage order changes.

If unique identifiers are not present, OVITO assumes a fixed storage order between frames.
This assumption may be invalid, as some simulation codes reorder particles during the run for performance reasons.

.. caution::

    If your simulation does not include particle IDs, OVITO may compute incorrect differences due to changes
    in particle ordering. Some file formats preserve order (e.g., XDATCAR), while others (e.g., LAMMPS formats) may not.
    Use the :ref:`data_inspector` to check for the ``Particle Identifier`` property after importing your data.

Simulations with changing particle numbers
""""""""""""""""""""""""""""""""""""""""""

The *Difference between frames* modifier assumes the number of particles remains constant over time.
This allows unambiguous computation of property differences for each particle.

If particles are removed in later frames, the modifier continues to compute differences for the remaining ones.
However, if new particles are added, the modifier raises an error by default, as no reference values exist for them.

To handle dynamic particle creation, you can enable the following options:

Tolerate newly appearing particles
    Allows the modifier to ignore new particles not present in the reference frame.
    These particles will be assigned a default value of 0 in the output.

Select newly appearing particles
    Marks particles that appear after the reference frame.
    This is useful for analyzing dynamically created particles.

.. seealso::

    :py:class:`ovito.modifiers.DifferenceBetweenFramesModifier` (Python API)
