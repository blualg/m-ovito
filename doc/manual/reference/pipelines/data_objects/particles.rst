.. _scene_objects.particles:

Particles
---------

In OVITO, a system of *N* particles is represented as a collection of :ref:`particle properties <usage.particle_properties>`,
each stored as a uniform data array of length *N*:

.. image:: /images/scene_objects/particles_data_model.png
  :width: 85%

The ``Position`` property array is always present and contains the Cartesian coordinates of the particles.
All other properties are optional and depend on the loaded simulation file and any :ref:`modifiers <usage.modification_pipeline>`
applied within OVITO. Modifiers can dynamically add new properties, for example, to store computed results.

You can view all currently available particle properties in the dataset using OVITO's :ref:`data inspector <data_inspector>` panel,
which is located below the viewport(s).

.. _usage.particle_properties.special:

Special particle properties
"""""""""""""""""""""""""""

Some particle properties have a unique role in OVITO, influencing the visual appearance of particles or other aspects of the dataset.
The table below describes these special properties:

=============================== ========================== =======================================================================================
Particle property               Data type (components)     Description
=============================== ========================== =======================================================================================
``Position``                    float64 (X, Y, Z)          The particle coordinates. In 2D systems, the Z-component is set to 0.
``Color``                       float32 (R, G, B)          If present, controls the particle display color. Red, green, and blue values range from [0,1].
``Radius``                      float32                    If present, determines the display size of spherical and cubic particles.
``Particle Type``               int32                      Stores the :ref:`type ID of each particle <scene_objects.particle_types>`, defining its display size and color if ``Radius`` or ``Color`` are absent.
``Particle Identifier``         int64                      A unique ID assigned to each particle. Used by some modifiers to track particles across trajectory frames.
``Transparency``                float32                    A value in the range [0,1] that controls particle transparency. If absent, particles are fully opaque.
``Selection``                   int8                       Indicates a particle's selection state (1 = selected, 0 = unselected).
=============================== ========================== =======================================================================================

.. _scene_objects.particle_types:

Typed properties
""""""""""""""""

.. image:: /images/scene_objects/particle_types_panel.png
  :width: 30%
  :align: right

A *typed property* is a particle property where each numeric value identifies a *type*,
and the corresponding descriptor of the type, which is stored in a separate lookup table, holds additional attributes.
The ``Particle Type`` property is a common example: it stores each particle's type ID (e.g., 1, 2, 3, …), with an associated table defining the
display color, radius, and other attributes of each type. This is more space efficient than storing the attributes directly for each particle.

.. image:: /images/scene_objects/typed_property.png
  :width: 55%

A particle structure may have multiple typed properties, allowing for different orthogonal classifications of particles.
For example, in addition to ``Particle Type``, a dataset might include properties such as ``Residue Type``,
``Structure Type``, or ``Molecule Type``.

The typed properties imported from a simulation file can be accessed and edited in the pipeline editor,
as shown in the screenshot on the right. You can edit the types defined by the ``Particle Type`` property, which
:ref:`affects particle rendering <visual_elements.particles>` (e.g. radius and color of each type).

If a particle type's name corresponds to a standard chemical element, OVITO automatically assigns default values for
display color, display radius, van der Waals radius, and mass -- based on an internal database of presets. You can change these defaults
using the presets menu (see screenshot) or the :ref:`application settings dialog <application_settings.particles>`.

Additionally, you can predefine attributes for user-defined or unnamed particle types (e.g., "Type 1", "Type 2"),
which is useful when loading files that contain only numeric type identifiers, such as :ref:`LAMMPS dump files <file_formats.input.lammps_dump>`.

.. seealso::

  * :py:class:`ovito.data.Particles` (Python API)
  * :py:class:`ovito.data.Property` (Python API)
  * :py:attr:`ovito.data.Property.types` (Python API)