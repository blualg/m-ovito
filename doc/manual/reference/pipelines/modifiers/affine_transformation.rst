.. _particles.modifiers.affine_transformation:

Affine transformation
---------------------

.. image:: /images/modifiers/affine_transformation_panel.png
  :width: 35%
  :align: right

This modifier applies an affine transformation to the system or specific parts of it. It may be used to translate, scale, rotate or shear
the particles, the simulation cell and/or other elements. The transformation can either be specified explicitly in terms of a 3-by-3
matrix plus a translation vector, or implicitly by prescribing a constant target shape for the simulation cell.

Given a 3-by-3 linear transformation matrix :math:`\mathbf{M}`
and a translation vector :math:`\mathbf{t}`, which together describe a general affine transformation,
the transformed position of a particle at the original Cartesian position :math:`\mathbf{x}`
is computed as :math:`\mathbf{x}' =  \mathbf{M} \cdot \mathbf{x} + \mathbf{t}`.
This notation uses column vectors.

The button :guilabel:`Enter rotation` opens a dialog box which lets you specify a 3D rotation
axis, a rotation angle and a center of rotation. Based on these inputs, OVITO computes the corresponding
affine transformation for you.

Translation in reduced coordinates
""""""""""""""""""""""""""""""""""

The option :guilabel:`In reduced cell coordinates` changes the affine transformation method
such that the translation vector :math:`\mathbf{t}` is specified in reduced cell coordinates instead of Cartesian coordinates, i.e.
in terms of the three vectors that span the simulation cell (after they have been transformed by the
linear matrix :math:`\mathbf{M}`).

In other words, activating this option changes the affine transformation equation to :math:`\mathbf{x}' =  \mathbf{M} \cdot (\mathbf{x} + \mathbf{H} \cdot \mathbf{t})`
with :math:`\mathbf{H}` being the 3-by-3 cell matrix formed by the three edge vectors of the original simulation cell.

Transform to target cell
""""""""""""""""""""""""

In :guilabel:`Target cell` mode, the modifier dynamically computes the affine transformation to be applied to the system
from the current shape of the :ref:`simulation cell <scene_objects.simulation_cell>` and the specified target shape.
The contents of the simulation box (e.g. particles, surface meshes, etc.) will be mapped to the new cell shape accordingly, unless you turn off their transformation (see next section).

.. tip::

  Use this option to replace a time-varying cell, e.g. from a constant-pressure simulation, with a constant cell shape of your choice.

Transformed elements
""""""""""""""""""""

You can select the types of data elements the modifier should touch:

.. table::
  :widths: auto

  =============================================================== =================================================================================
  Element type                                                    Description
  =============================================================== =================================================================================
  :ref:`Particles <scene_objects.particles>`                      Applies the transformation to the coordinates of particles (``Position`` property) and their orientations if present (``Orientation`` property).
  :ref:`Vector properties <usage.particle_properties>`            Applies the linear part :math:`\mathbf{M}` of the affine transformation to vectorial properties, e.g. the particle properties ``Velocity``, ``Force`` and ``Displacement``. Vectorial properties are those which have a :ref:`visual_elements.vectors` visual element attached and which consist of three floating-point components.
  :ref:`Simulation cell <scene_objects.simulation_cell>`          Applies the transformation to the origin of the :ref:`simulation cell <scene_objects.simulation_cell>` and the linear part to the three cell vectors.
  :ref:`Surfaces <scene_objects.surface_mesh>`                    Applies the transformation to the vertices of :ref:`surface meshes <scene_objects.surface_mesh>` and :ref:`triangle meshes <scene_objects.triangle_mesh>`.
  :ref:`Voxel grids <scene_objects.voxel_grid>`                   Applies the transformation to the domain shape of a :ref:`voxel grid <scene_objects.voxel_grid>`.
  :ref:`Lines <scene_objects.lines>`                              Applies the transformation to all :ref:`lines <scene_objects.lines>`.
  :ref:`Vectors <scene_objects.vectors>`                          Applies the transformation to all :ref:`vector objects <scene_objects.vectors>`.
  :ref:`Dislocations <scene_objects.dislocations>`                Applies the transformation to :ref:`dislocation lines <scene_objects.dislocations>` and their Burgers vectors.
  =============================================================== =================================================================================

The option :guilabel:`Transform only selected particles/vertices` restricts the function to the subset of currently selected particles or vertices of :ref:`meshes <scene_objects.surface_mesh>` and :ref:`lines <scene_objects.lines>`.

.. seealso::

  :py:class:`ovito.modifiers.AffineTransformationModifier` (Python API)