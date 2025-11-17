.. _visual_elements.particles:

Particles
---------

.. image:: /images/visual_elements/particles_panel.png
  :width: 35%
  :align: right

This :ref:`visual element <visual_elements>` is responsible for rendering the :ref:`particles <scene_objects.particles>`.
Typically, particles are visualized as simple spheres, but you can switch to other, more complex geometric shapes
if desired. The *Particles* visual element provides parameters controlling the standard visual representation of particles.

.. _visual_elements.particles.hierarchy:

Settings hierarchy
""""""""""""""""""

OVITO uses a three-level hierarchy to determine the final appearance of each individual particle:

1. **Per-particle properties** (highest priority): Properties like ``Color``, ``Radius``, and ``Transparency``
   defined for individual particles always take precedence if present. See :ref:`visual_elements.particles.properties` below for details.

2. **Per-type settings** (medium priority): Settings configured for each particle type through the
   :ref:`particles.modifiers.edit_types` modifier. These include type-specific radius, color, and shape.
   Per-type settings can also include :ref:`custom mesh shapes <howto.aspherical_particles.user_shapes>` for advanced visualizations.

3. **Default settings** (lowest priority): The default particle shape and radius values configured in
   this visual element serve as the final fallback.

This hierarchy allows flexible control: you can set defaults that work for most particles, customize
specific types (e.g., different radii for different atomic species), and still override individual
particles when needed (e.g., highlighting specific atoms using :ref:`selection <particles.modifiers.selection>` and :ref:`coloring <particles.modifiers.coloring>` modifiers).

To modify these settings:

* **Per-particle properties**: Use the :ref:`particles.modifiers.compute_property` modifier or more specific :ref:`coloring <particles.modifiers.coloring>` modifiers
* **Per-type settings**: Use the :ref:`particles.modifiers.edit_types` modifier
* **Default settings**: Configure them directly in this visual element panel

.. _visual_elements.particles.properties:

Per-particle properties
"""""""""""""""""""""""

On the primary level, the visualization is affected by certain properties of the particles themselves, listed in the following table.
By setting these particle properties, for example using the :ref:`particles.modifiers.compute_property` modifier,
you can fully control the visualization on a per-particle basis.

.. table::
  :widths: 20 15 65

  ================================== ======================= ==============================================================================
  Particle property                  Data type               Description
  ================================== ======================= ==============================================================================
  ``Color``                          Real (R,G,B)            Display color of individual particles. Red, green and blue components must be in the range [0,1].
  ``Radius``                         Real                    Display size on a per-particle basis.
  ``Transparency``                   Real                    Transparency of individual particles. Must be in the range [0,1].
  ``Aspherical Shape``               Real (X,Y,Z)            Dimensions of particles with a non-spherical shape. The exact interpretation depends on the :ref:`particle shape <howto.aspherical_particles>`.
  ``Orientation``                    Real (X,Y,Z,W)          3D rotation of particles with non-symmetric shapes, specified as a `quaternion <https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation>`__.
                                                             See :ref:`howto.aspherical_particles.orientation` for further information.
  ``Superquadric Roundness``         Real (X,Y)              Roundness parameters for :ref:`superquadric particles <howto.aspherical_particles.superquadrics>`.
  ``Selection``                      Integer                 Marks currently selected particles (1 = selected, 0 = unselected). Selected particles are highlighted in red (only in interactive viewports but not in rendered images).
  ================================== ======================= ==============================================================================

To see the list of particle properties present in your dataset, open the :ref:`data inspector panel <data_inspector.particles>`.

Parameters
""""""""""

Universal settings
''''''''''''''''''

These settings apply globally to all particles in the system:

Radius scaling
  This global scaling factor (percentage) is applied to all particle radii at rendering time.
  It is applied in addition to any other factors controlling the size of particles (per-type radius, per-particle radius, default radius).
  By setting it to a value below 100%, you can generate a balls-and-sticks visualization of molecular systems,
  with reduced atomic spheres connected by cylindrical bonds.

Rendering quality
  This parameter controls the method used for rendering spherical particles in the interactive viewports using the :ref:`OpenGL renderer <rendering.opengl_renderer>`.
  The following modes are available:

  Automatic (default)
    OVITO automatically selects the rendering quality based on particle count:

    * Less than 4,000 particles: High quality
    * Between 4,000 and 400,000 particles: Medium quality
    * More than 400,000 particles: Low quality
    * High quality is always used for final output images regardless of particle count

  Low
    Particles are rendered as texture-mapped imposters facing the viewer. Particles do not have depth in this mode,
    and intersections between overlapping particles may not be displayed correctly. This mode offers the best performance.

  Medium
    Particles are rendered as texture-mapped imposters facing the viewer. An OpenGL fragment shader computes
    depth information for each pixel to produce reasonable sphere-sphere intersections for overlapping particles.

  High
    Particles are rendered as true spheres using an OpenGL fragment shader that computes the ray-sphere intersection
    for every rendered pixel. This provides the best visual quality.

Default particle shape
''''''''''''''''''''''

These settings provide default values for particles that don't have shape or size specified via per-type settings or per-particle properties:

Style
  Selects the default display shape for particles. The shape can be overridden on a per-type basis
  (see the note above about the :ref:`property hierarchy <visual_elements.particles.hierarchy>`). Available shapes:

  Sphere/Ellipsoid
    Particles are rendered as 3D spheres by default. This mode supports several advanced variations:

    * With ``Aspherical Shape`` property: Particles become :ref:`ellipsoids <howto.aspherical_particles.ellipsoids>` where the
      three components (X,Y,Z) control the half-lengths of the principal axes (the ``Radius`` property is ignored).
    * With ``Superquadric Roundness`` property: Particles are rendered as :ref:`superquadrics <howto.aspherical_particles.superquadrics>` with controllable roundness.
    * The particle's ``Orientation`` property, if present, rotates the ellipsoid or superquadric.

  Circle
    Particles are rendered as flat discs that always face the viewer.
    Note that some :ref:`rendering engines <rendering>` may not support this mode.

  Cube/Box
    Particles are rendered as :ref:`cubes <howto.aspherical_particles.boxes>` by default, with the ``Radius`` property controlling the edge half-length.

    * With ``Aspherical Shape`` property: Particles become rectangular boxes where the three components (X,Y,Z)
      specify the half-lengths along each axis.
    * The particle's ``Orientation`` property, if present, rotates the box.

  Square
    Particles are rendered as flat squares that always face the viewer.
    Note that some :ref:`rendering engines <rendering>` may not support this mode.

  Cylinder
    Particles are rendered as :ref:`cylinders <howto.aspherical_particles.cylinders>` aligned along the Z-axis by default.

    * The ``Aspherical Shape`` property is required: Its X-component controls the cylinder radius, its
      Z-component controls the cylinder length.
    * The ``Orientation`` property rotates the cylinder from its default Z-axis alignment.

  Spherocylinder
    Particles are rendered as :ref:`cylinders with hemispherical caps (capsules) <howto.aspherical_particles.capsules>`.
    Configuration is identical to the *Cylinder* mode above.

Radius
  Specifies the default particle size (in simulation distance units) used as a fallback when the size is not
  specified via per-particle or per-type settings. This value is only used for particles where:

    * The ``Radius`` particle property is absent or zero, AND
    * The particle's type (from ``Particle Type`` property) has no radius defined or has radius = 0

.. seealso::

  :py:class:`ovito.vis.ParticlesVis` (Python API)