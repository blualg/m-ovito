.. _scene_objects.bonds:

Bonds
-----

Bonds represent connections between pairs of :ref:`particles <scene_objects.particles>`. In OVITO, they are managed as a separate
topological data structure that stores connectivity information -- a list of index pairs referring to the particle list -- along with
any additional bond properties.

The example below shows a simple molecule consisting of three atoms and two bonds. Note that indices are always zero-based.

.. image:: /images/scene_objects/bond_data_example.svg
  :width: 80%

The bond list is an optional component of a :ref:`molecular dataset <usage.data_model>`. Its presence depends on the imported :ref:`simulation file format <file_formats.input>`;
some formats include bond data, while others do not. If bonds are missing, you can generate them dynamically in OVITO using
the :ref:`particles.modifiers.create_bonds` modifier -- or the file importer may provide a built-in way to create ad-hoc bonds based on
the chemical species and separation of the atoms.

The visual representation of bonds is controlled by the :ref:`bonds visual element <visual_elements.bonds>` settings,
which appears in the top section of the :ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>`.

Bond properties
"""""""""""""""

Like particles, bonds can have additional attributes known as *bond properties*. One commonly used property is ``Bond Type``,
an integer identifier similar to ``Particle Type``. This property allows OVITO to associate different colors with different types of bonds
and is often used in molecular dynamics simulations to distinguish force field interaction types.

You can also control the display color on a per-bond basis by setting the ``Color`` property in OVITO. The :ref:`particles.modifiers.color_coding`
modifier is a common tool for setting bond colors based on other quantities associated with the bonds.

The ``Topology`` property, always present in bond datasets, defines the actual connectivity between particles as index pairs.
If particles are deleted from the system, OVITO automatically updates these indices to maintain consistency with the particles.

You can inspect all bond properties using the **Bonds** tab in the :ref:`data inspector panel <data_inspector.bonds>` beneath the viewport(s).
The :ref:`particles.modifiers.bond_analysis` modifier is a useful tool for computing statistical properties of the bond network,
such as bond length or bond angle histograms.

Additional bond generation tools in OVITO include the :ref:`particles.modifiers.voronoi_analysis` modifier, which creates bonds
between nearest-neighbor particles. Bonds and their properties can also be manipulated using general-purpose modifiers such as:

- :ref:`particles.modifiers.compute_property` -- Compute and assign new bond properties.
- :ref:`particles.modifiers.expression_select` -- Select bonds based on property values or bond length.
- :ref:`particles.modifiers.assign_color` -- Assign a new color to selected bonds.
- :ref:`particles.modifiers.delete_selected_particles` -- Remove selected bonds.
- :ref:`particles.modifiers.expand_selection` -- Find particles that are connected to already selected particles.

.. _usage.bond_properties.special:

Special bond properties
"""""""""""""""""""""""

Some bond properties play a key role in OVITO, affecting visualization of the bonds.
The table below describes these special properties:

========================= ========================== =======================================================================================
Bond property             Data type (components)     Description
========================= ========================== =======================================================================================
``Topology``              int64 (A, B)               Always present, this property stores the zero-based indices of the two connected particles.
``Bond Type``             int32                      Defines bond types, controlling the display color if the ``Color`` property is absent.
``Color``                 float32 (R, G, B)          Controls individual bond colors. RGB values range from [0,1].
``Transparency``          float32                    Determines bond transparency (range: [0,1]). If absent, bonds are fully opaque.
``Selection``             int8                       Bond selection state (1 = selected, 0 = unselected).
``Width``                 float32                    Defines per-bond diameter (in simulation units). If absent, bond width is set via
                                                     the :ref:`bonds visual element <visual_elements.bonds>`.
========================= ========================== =======================================================================================

.. seealso::

  :py:class:`ovito.data.Bonds` (Python API)