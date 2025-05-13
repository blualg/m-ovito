.. _scene_objects.simulation_cell:

Simulation cell
---------------

.. image:: /images/scene_objects/simulation_cell_panel.png
  :width: 35%
  :align: right

The **Simulation cell** :ref:`data object <scene_objects>` defines the geometry of the simulation domain in two or three dimensions,
including boundary conditions (PBC flags). The simulation cell loaded from the imported simulation file can be found in the
**Data source** section of the :ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>` (see screenshot).

**Note:** Additionally, the upper section of the pipeline editor shows a second item labeled **Simulation cell**, which is the
corresponding :ref:`visual element <visual_elements.simulation_cell>`. This visual element controls
how the simulation cell appears in rendered images.

If the imported simulation file does not specify cell dimensions, OVITO automatically generates an ad-hoc simulation cell that
encloses all particles in an axis-aligned bounding box.

Dimensionality
""""""""""""""

OVITO supports both 2D and 3D datasets. The *dimensionality* is automatically detected during file import but can be manually
adjusted in the **Simulation cell** panel.

- In **2D mode**, the Z-coordinates of particles and the third simulation cell vector are ignored in most calculations.

Boundary conditions
"""""""""""""""""""

The **periodic boundary condition (PBC)** flags determine whether OVITO applies periodicity when performing calculations.
Where possible, OVITO reads or infers these flags from the imported simulation file.
If needed, you can manually override these flags in the **Simulation cell** panel.

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

For informational purposes, the panel also reports the axis-aligned dimensions of the simulation cell (bounding box) as **Width (X)**, **Height (Y)**, and **Depth (Z)**,
which are calculated from the three cell vectors. These values are only meaningful for orthogonal cells using the default convention.

.. note::

  The **Simulation cell** panel, which is found under the **Data source** section of the pipeline editor, always displays the original cell
  shape loaded or inferred from the imported simulation file. To modify the simulation cell vectors in OVITO, use
  the :ref:`particles.modifiers.affine_transformation` modifier in the data pipeline.

.. seealso::

  :py:class:`ovito.data.SimulationCell` (Python API)