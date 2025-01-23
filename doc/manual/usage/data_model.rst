.. _usage.data_model:
.. _usage.particle_properties:

Data model
==========

This page introduces the data model used by OVITO to represent molecular structures and other particle-based datasets.
Understanding the basic concepts of OVITO's data model is important for working effectively with the program's data analysis and
visualization functions.

Particle properties
-------------------

:ref:`Particle properties <scene_objects.particles>` are numeric values associated with individual particles.
They play a central role in OVITO's data model and the way molecular and other structures are represented.
Typical particle properties include particle position, chemical type, and velocity. Users can assign any number of additional properties to particles,
either explicitly or as a result of computations performed by the program.

This general concept is also employed for other kinds of data elements, not just particles.
For instance, :ref:`interatomic bonds <scene_objects.bonds>` have *bond properties*, such as bond type or color.
While the following introduction focuses mainly on particles and their properties,
the same principles apply to other kinds of data elements in OVITO.

What is a particle property?
----------------------------

Technically, every OVITO particle property is presented by a uniform array of numeric values, storing one value per particle.
OVITO provides various tools for you to manipulate these property values and thereby change visual and non-visual aspects of the particles.

Each property has a unique name, for example, ``Position`` or ``Potential Energy``.
OVITO has a built-in list of :ref:`commonly-used property names <particle-properties-list>`, which have a specific meaning to the program and a prescribed data layout, but
you are free to define additional properties with user-defined names and import them from a simulation file, for example.

At least the ``Position`` particle property is always present, because particles cannot exist without spatial coordinates.
Other properties such as ``Color``, ``Radius``, or ``Selection`` are optional; they may or may not be present.
You can load property values from the simulation files, you you can add new properties within the program using various tools.
The mentioned standard properties :ref:`affect how OVITO renders the particles <usage.particle_properties.special>`.
By manipulating the values to these properties, you can control the visual appearance of the particles.

Property values can have different data types (floating-point or integer) and dimensionalities (e.g., scalar, vector, tensor).
The ``Position`` property, for instance, is a vectorial property with three components per particle, referred to as
``Position.X``, ``Position.Y``, and ``Position.Z`` within OVITO's user interface.

.. seealso::

   * :ref:`usage.particle_properties.special`
   * :ref:`scene_objects.particle_types`

Inspecting properties
---------------------

.. figure:: /images/usage/properties/particle_inspection_example.*
   :figwidth: 50%
   :align: right

   Data inspector displaying the table of particle properties

Standard properties such as ``Position``, ``Particle Type``, or ``Velocity`` are typically initialized from data found in the
imported simulation file. Some file formats, such as *LAMMPS dump* or the `extended XYZ format <http://libatoms.github.io/QUIP/io.html#module-ase.io.extxyz>`_,
can store an arbitrary number of extra data columns. These auxiliary attributes are :ref:`automatically mapped to corresponding particle properties in OVITO <file_formats.input.lammps_dump.property_mapping>`.

To find out which properties are currently associated with the particles, you can open OVITO's :ref:`Data inspector <data_inspector>` panel,
which is depicted in the screenshot on the right. Alternatively, you can simply point the mouse cursor at a particle in the viewports
see all of its property values in the status bar.

Assigning property values
-------------------------

OVITO provides a rich set of functions for setting or modifying the properties of particles. These so-called *modifiers*
will be introduced in more detail in a later section of this manual. But to give you a first idea of the principle:
The :ref:`particles.modifiers.assign_color` modifier function lets you assign a uniform color of your choice
to all currently selected particles. It does this by setting the ``Color`` property of the
particles to the given RGB value. If the ``Color`` property doesn't exist yet, it is automatically created by the modifier.
Which particles are part of the current selection set is determined by their ``Selection`` property: Particles whose ``Selection``
property is non-zero are selected, whereas particles with a zero value are unselected.

OVITO offers several selection modifiers, which let you create a particle selection by appropriately setting the ``Selection`` property of each particle.
For example, the :ref:`Select type <particles.modifiers.select_particle_type>` modifier uses the ``Particle Type``
property of each particle to decide whether or not to select that particle. This allows you to select all atoms of a particular chemical type, for example.

:ref:`particles.modifiers.coordination_analysis` is another typical modifier.
It computes the number of neighbors of each particle within a given cutoff range and stores the computation results in a new particle
property named ``Coordination``. Subsequently, you can use the values of this property, e.g., select particles having a
certain maximum coordination or color all particles based on their coordination number (see :ref:`particles.modifiers.color_coding` modifier).

Of course, it is also possible to export the computed per-particle  values to an output file. OVITO supports a variety of output formats for that (see
:ref:`usage.export`). For instance, the *XYZ* format is a simple tabular file format supporting an arbitrary set of output columns.