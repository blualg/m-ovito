.. _particles.modifiers.affine_transformation:

Affine transformation
---------------------

.. image:: /images/modifiers/affine_transformation_panel.png
  :width: 30%
  :align: right

This modifier applies an affine transformation to the system or specific parts of it. It may be used to translate, scale, rotate or shear
the particles, the simulation cell and/or other elements. The transformation can either be specified explicitly in terms of a 3x3
matrix plus a translation vector, or implicitly by prescribing a fixed target shape for the simulation cell.

Given a 3x3 linear transformation matrix :math:`\mathbf{M}`
and a translation vector :math:`\mathbf{t}`, which together describe a general affine transformation,
the transformed position of a particle at the original Cartesian position :math:`\mathbf{x}`
is computed as :math:`\mathbf{x}' =  \mathbf{M} \cdot \mathbf{x} + \mathbf{t}`.
This notation uses column vectors.

The button :guilabel:`Enter rotation` opens a dialog box which lets you specify a rotation
axis, a rotation angle and a center of rotation. Based on these inputs, OVITO computes the corresponding
affine transformation for you.

Translation in reduced coordinates
""""""""""""""""""""""""""""""""""

The option :guilabel:`In reduced cell coordinates` changes the affine transformation method
such that the translation vector :math:`\mathbf{t}` is specified in reduced cell coordinates instead of Cartesian coordinates, i.e. 
in terms of the three vectors that span the simulation cell (after they have been transformed by the 
linear matrix :math:`\mathbf{M}`).

In other words, activating this option changes the affine transformation equation to :math:`\mathbf{x}' =  \mathbf{M} \cdot (\mathbf{x} + \mathbf{H} \cdot \mathbf{t})`
with :math:`\mathbf{H}` being the 3x3 cell matrix formed by the three edge vectors of the original simulation cell.

Transform to target cell
""""""""""""""""""""""""

The :guilabel:`Transform to target cell` option lets the modifier dynamically compute the affine transformation 
based the original shape of the simulation cell and a specified target cell shape. The automatically determined transformation 
maps the existing simulation cell exactly onto the given target cell shape. All contents of the simulation cell (e.g. particles, surface meshes, etc.) will be mapped into the new
cell shape accordingly, unless you turn off their transformation (see next section).

Note that you can use this option to replace a varying simulation cell loaded from the input trajectory file(s)
with a constant cell shape of your choice.

Transformed elements
""""""""""""""""""""

You can select the types of data elements that should get transformed by the modifier:

.. table::
  :widths: auto

  =============================================================== =================================================================================
  Element type                                                    Description
  =============================================================== =================================================================================
  :ref:`Particles <scene_objects.particles>`                      Applies the affine transformation to the coordinates of particles (i.e. the ``Position`` particle property).
  :ref:`Vector properties <usage.particle_properties>`            Applies the linear part :math:`\mathbf{M}` of the affine transformation to vectorial properties, e.g. the particle properties ``Velocity``, ``Force`` and ``Displacement``. Vectorial properties are those which have a :ref:`visual_elements.vectors` visual element attached and which consist of three floating-point components.
  :ref:`Simulation cell <scene_objects.simulation_cell>`          Applies the affine transformation to the origin of the :ref:`simulation cell <scene_objects.simulation_cell>` and the linear part to the three cell vectors.
  :ref:`Surfaces <scene_objects.surface_mesh>`                    Applies the affine transformation to the vertices of :ref:`surface meshes <scene_objects.surface_mesh>` and :ref:`triangle meshes <scene_objects.triangle_mesh>`.
  :ref:`Voxel grids <scene_objects.voxel_grid>`                   Applies the affine transformation to the domain shape of a :ref:`voxel grid <scene_objects.voxel_grid>`.
  :ref:`Dislocations <scene_objects.dislocations>`                Applies the affine transformation to a set of :ref:`dislocation lines <scene_objects.dislocations>`.
  =============================================================== =================================================================================

The option :guilabel:`Transform selected elements only` restricts the application of the transformation to
the currently selected particles.

.. seealso::
  
  :py:class:`ovito.modifiers.AffineTransformationModifier` (Python API)