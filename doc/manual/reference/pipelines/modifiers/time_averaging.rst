.. _particles.modifiers.time_averaging:

Time averaging |ovito-pro|
--------------------------

.. image:: /images/modifiers/time_averaging_panel.png
  :width: 35%
  :align: right

This modifier can compute the mean value of one or more input quantities, averaged over the frames of the loaded simulation trajectory.
The following kinds of input quantities are currently supported:

Attributes
  :ref:`Global attributes <usage.global_attributes>` are scalar variables associated with the dataset,
  which may vary with time. The modifier will compute the global mean value
  of the selected input attribute and output it as a new attribute, which does not change with time.

Data tables
  The modifier can compute the time-averaged versions of :ref:`data tables <scene_objects.data_table>`.
  Data tables typically contain dynamically computed structural or statistical information such as a :ref:`radial distribution function <particles.modifiers.coordination_analysis>`
  or :ref:`histograms <particles.modifiers.histogram>` of some particle property.
  With the help of the time averaging modifier you can average such time-varying tables over the entire trajectory.

Properties
  The modifier also supports computing time averages of element-wise properties (e.g. :ref:`particle <scene_objects.particles>` or :ref:`voxel grid <scene_objects.voxel_grid>` properties).
  For the computation to work, the number of data elements (e.g. particles) must not change with time.
  Thus, you should place the time averaging modifier in the data pipeline *before* any filter operations
  that remove a variable number of particles from the system.

  .. attention::

    If you time-average the ``Position`` properties of particles in a system with periodic boundary conditions, make sure
    the particles are not wrapped back into the simulation box during the trajectory, i.e., the time averaging modifier
    requires *unwrapped* coordinates as input. You can use the :ref:`particles.modifiers.unwrap_trajectories` modifier to
    unwrap the particle coordinates first.

Simulation cell
  The modifier allows computing the time average of the cell shape (cell matrix elements).

Note that the modifier has to step through all frames of the simulation trajectory to compute the time average of the
selected quantity. This can be a lengthy process depending on the extent of the trajectory and the dataset size. However, the averaging will happen
in the background, and you can continue working with the program while the modifiers is performing the calculation.
Once the averaging calculation is completed, you can press the button :guilabel:`Show in data inspector` button
to reveal the computed average quantity in the :ref:`data inspector <data_inspector>` of OVITO.

The averaged values are output as a new quantities by default, with the suffix "Average" appended, to keep the
original and averaged values separate. You can change that by selecting the option :guilabel:`Replace original values`,
which lets the modifier overwrite the current time-dependent values with the static averaged values.

.. caution::

  When averaging integer particle properties (e.g. particle types, coordination number, etc.) the result will be rounded to the
  nearest integer value if the :guilabel:`Replace original values` option is selected. This can lead to a loss of precision
  in the averaged values. If :guilabel:`Create new values` is selected, the averaged values will always be stored as
  floating-point properties with full precision.

.. seealso::

  :py:class:`ovito.modifiers.TimeAveragingModifier` (Python API)
