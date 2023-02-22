.. _howto.aspherical_particles:

Non-spherical particle shapes
=============================

.. figure:: /images/howtos/ellipsoid_particles_example1.*
   :figwidth: 25%
   :align: right

   Ellipsoidal particles

.. figure:: /images/howtos/particles_usershape_example.jpg
   :figwidth: 25%
   :align: right

   User-defined particle shapes

OVITO has built-in support for a range of different particle shapes aside from the standard spherical shape.
Furthermore, it supports user-defined particle shapes, which are specified in terms of polyhedral meshes:

 - spheres
 - ellipsoids
 - superquadrics
 - cubes, boxes
 - cylinders
 - spherocylinders (capsules)
 - circles (discs that follow the view direction)
 - squares (billboards that follow the view direction)
 - user-defined polygonal meshes

You can set the display shape of particles globally for the :ref:`Particles <visual_elements.particles>` visual element
or on a per particle type basis in the :ref:`Particle types <scene_objects.particle_types>` panel.

.. _howto.aspherical_particles.orientation:

Size and orientation of particles are controlled by properties
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

By assigning values to the following particle properties in OVITO, you can control the orientation and the dimensions or size of
each particle individually:

   - ``Radius`` (scalar)
   - ``Aspherical Shape`` (X, Y, Z)
   - ``Orientation`` (X, Y, Z, W)
   - ``Superquadric Roundness`` (Phi, Theta)

The property assignment can happen directly during import of your data file into OVITO by mapping values from corresponding
file columns to the right target properties in OVITO. Furthermore, you can subsequently assign values to these properties as needed by inserting
the :ref:`particles.modifiers.compute_property` modifier into the data pipeline.

The orientation of non-spherical particles is controlled by the ``Orientation`` particle property,
which consists of four components :math:`\mathrm{q} = (x, y, z, w)` forming a `quaternion <https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation>`__.
A quaternion is a mathematical representation of an arbitrary rotation in three-dimensional space, similar to (but more compact than)
a rotation matrix.

Note that OVITO employs a notation for quaternions that follows the `work of Ken Shoemake <https://www.ljll.math.upmc.fr/~frey/papers/scientific%20visualisation/Shoemake%20K.,%20Quaternions.pdf>`__.
This convention may slightly differ from other notations you find in the literature (e.g. on `Wikipedia <https://en.wikipedia.org/wiki/Quaternions_and_spatial_rotation>`__).
The four quaternion components :math:`(x, y, z, w)` are sometimes interchanged and written as :math:`(q_r,q_i,q_j,q_k) = (w',x',y',z')` instead.
Thus, during import of orientational data into OVITO, you may have to remap the four input components of the quaternions to the right target components.

The ``Orientation`` and ``Aspherical Shape`` particle properties are typically loaded from data files written by some simulation code. The LAMMPS simulation code, for example,
allows you to output this per-particle information to a dump file using the following LAMMPS script commands:

::

  compute orient all property/atom quatw quati quatj quatk
  compute diameter all property/atom shapex shapey shapez
  dump 1 all custom 100 ellipsoid.dump id type x y z &
                                       c_q[1] c_q[2] c_q[3] c_q[4] &
                                       c_diameter[1] c_diameter[2] c_diameter[3]

During import of the dump file in OVITO, you should map the ``quati``, ``quatj``, ``quatj``, and ``quatw`` atom attributes from LAMMPS
to the ``Orientation.X``, ``Orientation.Y``, ``Orientation.Z``, and ``Orientation.W`` particle properties in OVITO (in this exact order).
This mapping will be established automatically if you `assign the right names to the columns <https://docs.lammps.org/dump_modify.html>`__
when writing the dump file:

::

  dump_modify 1 colname c_q[1] quatw colname c_q[2] quati colname c_q[3] quatj colname c_q[4] quatk
  dump_modify 1 colname c_diameter[1] shapex colname c_diameter[2] shapey colname c_diameter[3] shapez

Similarly, the ``shapex``, ``shapey``, and ``shapez`` columns will be mapped to the properties ``Aspherical Shape.X``, ``Aspherical Shape.Y``, and ``Aspherical Shape.Z``
within OVITO, which controls the principal semi-axes of non-spherical particles as described below.

.. note::

   The correct mapping can be set up automatically by OVITO only if you name the dump file columns as specified
   :ref:`here <file_formats.input.lammps_dump.property_mapping>`. Otherwise, you may have to adjust the mapping by hand in the :guilabel:`Edit column mapping` dialog,
   which is accessible from the :ref:`file import panel <file_formats.input.lammps_dump>` after opening the dump or xyz file.

.. _howto.aspherical_particles.spheres:

Spheres
"""""""

.. image:: /images/howtos/spherical_particles.jpg
   :width: 25%
   :align: right

The `sphere <https://en.wikipedia.org/wiki/Sphere>`__ particle shape is defined by the mathematical equation

  :math:`{\displaystyle {\frac {x^2 + y^2 + z^2}{r^2}} = 1}`.

The sphere radius :math:`r` is controlled by the per-particle property ``Radius``. If not present, the radius is determined by the
:ref:`particle type <scene_objects.particle_types>` or, globally, by the :ref:`Particles <visual_elements.particles>` visual element.

The ``Position`` particle property specifies an additional translation of the spherical equation above away from coordinate system origin, of course.

.. _howto.aspherical_particles.ellipsoids:

Ellipsoids
""""""""""

.. image:: /images/howtos/ellipsoid_particles_example1.*
   :width: 25%
   :align: right

The `ellipsoid <https://en.wikipedia.org/wiki/Ellipsoid>`__ particle shape is defined by the equation

  :math:`{\displaystyle {\frac {x^2}{a^2}}+{\frac {y^2}{b^2}}+{\frac {z^2}{c^2}} = 1}`.

The length of the principal semi-axes :math:`a`, :math:`b`, :math:`c` of the ellipsoid are controlled by the per-particle property ``Aspherical Shape``,
which has three components `X`, `Y`, and `Z` (all positive). If all three components of the property are zero for a particle,
OVITO falls back to :math:`a=b=c=r`, with :math:`r` being the spherical radius of the particle as defined above.

.. _howto.aspherical_particles.superquadrics:

Superquadrics
"""""""""""""

.. image:: /images/howtos/superquadrics.jpg
   :width: 25%
   :align: right

The shape of `superquadric <https://en.wikipedia.org/wiki/Superquadrics>`__ particles is defined by the equation

  :math:`{\displaystyle \left( {\left| \frac{x}{a} \right| ^{(2/\phi)}} + {\left| \frac{y}{b} \right| ^{(2/\phi)}} \right) ^{(\phi/\theta)} + {\left| \frac{z}{c} \right| ^{(2/\theta)}} = 1}`.

Like ellipsoidal particles, the superquadric shape has three semi-axes :math:`a`, :math:`b`, :math:`c`, which are specified by
the ``Aspherical Shape`` particle property. The two parameters :math:`\phi` and :math:`\theta` are called *east-west* and *north-south* exponents and determine
the blockiness/roundness of the superquadric ellipsoid. Both must be strictly positive. The normal sphere (or ellipsoid) is reproduced by setting :math:`\phi = \theta = 1`.
In OVITO, the values of :math:`\phi` and :math:`\theta` are specified by the ``Superquadric Roundness`` property, which is a vector particle property having two components.

.. _howto.aspherical_particles.boxes:

Boxes
"""""

.. image:: /images/howtos/box_particles_example1.*
   :width: 25%
   :align: right

The size of box-shaped particles is given by the semi-axes :math:`a`, :math:`b`, :math:`c`, which are multiplied by a factor of two to yield the edge lengths of the box along the
Cartesian coordinate axes. In OVITO the semi-axes are determined by the particle property ``Aspherical Shape``, which has three components.
If not present, or if the components of ``Aspherical Shape`` are zero for a particle, OVITO falls back to using the ``Radius`` particle property and renders a cube.

.. _howto.aspherical_particles.cylinders:

Cylinders
"""""""""

The cylindrical shape is given by the radius :math:`r` and the height :math:`h` (in the local coordinate system of the cylinder). The natural orientation of the cylinder is along the positive z-axis,
with an optional rotation specified by the ``Orientation`` particle property. :math:`r` and :math:`h` are determined by the particle property components ``Aspherical Shape.X`` and ``Aspherical Shape.Z``.
The second vector component (`Y`) is ignored. If ``Aspherical Shape`` is not defined, OVITO will fall back to :math:`h = 2 r`, with :math:`r` taken from the ``Radius`` property instead.

.. _howto.aspherical_particles.capsules:

Spherocylinders (capsules)
""""""""""""""""""""""""""

.. image:: /images/howtos/spherocylinder_particles_example1.*
   :width: 25%
   :align: right

The size of spherocylindrical particles is controlled in the same way as cylindrical particles. OVITO additionally renders two hemispheres at each end of the cylinder extending its height.

.. _howto.aspherical_particles.circles_and_squares:

Circles and squares
"""""""""""""""""""

Circles and squares are two-dimensional, i.e. flat, shapes, whose size is controlled by the ``Radius`` particle property. The orientation
of each particle in three-dimensional space is determined automatically such that it exactly faces the viewer. Thus, their orientations are view-dependent,
and the ``Orientation`` particle property, if present, is ignored. In other words, you don't have the possibility to control their orientations explicitly
(use a mesh-based shape instead if you need control).

.. note::

  Rendering of flat circles and squares is only possible with the :ref:`OpenGL renderer <rendering.opengl_renderer>` of OVITO. The :ref:`Tachyon <rendering.tachyon_renderer>` and :ref:`OSPRay <rendering.ospray_renderer>` rendering engines
  do not support this kind of particle shape.

.. _howto.aspherical_particles.user_shapes:

User-defined shapes
"""""""""""""""""""

.. image:: /images/howtos/particles_usershape_example.jpg
   :width: 25%
   :align: right

On the level of individual particle types, you can assign custom particle shapes imported from external geometry files.
OVITO supports loading general polyhedral meshes, which can serve as user-defined particle shapes, from various :ref:`file input formats <file_formats.input>`
such as STL, OBJ, or VTK/VTP.

Some simulation formats such as HOOMD/GSD can embed information on particle shapes directly in the simulation output file, which will be :ref:`picked up by OVITO automatically <file_formats.input.gsd>`.
In most cases, however, you'll have to load the user-defined particle shape by hand for each particle type in the :ref:`Particle types <scene_objects.particle_types>` panel.
Set the particle type's :guilabel:`Shape` to `Mesh/User-defined` and import the shape geometry from a file that you have prepared outside of OVITO.

The vertex coordinates of the loaded polyhedral mesh get scaled by the value of the ``Radius`` property of each particle (if present)
and rotated by the quaternion stored in the ``Orientation`` property (if present). Alternatively, you can set the
:guilabel:`Display radius` parameter of the type to scale all particles of that type or adjust the :guilabel:`Radius scaling factor`
in the :ref:`Particles <visual_elements.particles>` visual element to scale all particles uniformly.