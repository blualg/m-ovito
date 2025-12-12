.. _scene_objects.simulation_cell:

Simulation cell
---------------

The **Simulation cell** :ref:`data object <scene_objects>` defines the geometry of the simulation domain in two or three dimensions,
including boundary conditions (PBC flags). It is typically loaded from the simulation file and its parameters can be inspected
by opening the *Simulation cell* tab of the :ref:`data inspector <data_inspector.simulation_cell>`.

The cell is visualized as a box or parallelepiped enclosing the particles. This visual representation can be configured
in the :ref:`simulation cell visual element <visual_elements.simulation_cell>` settings.

If the imported simulation file lacks simulation cell information, the **Simulation cell** data inspector page and the visual element will be missing.
In this case, OVITO implicitly assumes a non-periodic infinite cell in all pipeline computations.
If desired, you can turn on the option :guilabel:`Generate bounding box if needed` in the
:ref:`file reader settings <file_formats.input.xyz.simulation_cell>` to let OVITO automatically generate an ad-hoc simulation cell that
tightly encloses all particles in an axis-aligned bounding box.

Dimensionality
""""""""""""""

OVITO supports both 2D and 3D datasets. The *dimensionality* is automatically detected during file import but can be manually
adjusted in the **Simulation cell** panel.

- In **2D mode**, the Z-coordinates of particles and the third simulation cell vector are ignored in most calculations.

Boundary conditions
"""""""""""""""""""

The **periodic boundary condition (PBC)** flags determine whether OVITO applies periodicity when performing calculations.
Where possible, OVITO reads or infers these flags from the imported simulation file.
If needed, you can manually override these flags using the :ref:`particles.modifiers.edit_simulation_cell` modifier.

Note that, even though the PBC flags are labeled as *X*, *Y*, and *Z*, they actually
refer to the three vectors spanning the simulation cell, which are not necessarily aligned
with the coordinate axes.

Cell geometry
"""""""""""""

The shape of the simulation cell is defined by three cell vectors :math:`\bf{a}`, :math:`\bf{b}`, and :math:`\bf{c}` forming a parallelepiped and an **origin point**,
which defines the corner of the cell in the global Cartesian coordinate system, from where the edge vectors originate.

OVITO puts no restrictions on the lengths and angles of the three cell vectors. The only requirement is that they must not be collinear.
The vectors can be specified in any order, but the default is to use the following convention:
The cell vector :math:`\bf{a}` points in the positive X-direction, :math:`\bf{b}` in the positive Y-direction, and :math:`\bf{c}` in the positive Z-direction.

.. note::

  To modify the simulation cell geometry, boundary conditions, or dimensionality in OVITO, use
  the :ref:`particles.modifiers.edit_simulation_cell` modifier in the data pipeline, which provides direct control over all cell parameters.
  Alternatively, you can use the :ref:`particles.modifiers.affine_transformation` modifier to apply linear transformations to the cell.
  The final state of the cell after all applied modifications is shown in the :ref:`data inspector <data_inspector.simulation_cell>`.

.. seealso::

  * :py:class:`ovito.data.SimulationCell` (Python API)
  * :ref:`data_inspector.simulation_cell`
  * :ref:`particles.modifiers.edit_simulation_cell`