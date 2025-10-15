.. _data_inspector.simulation_cell:

Simulation cell
===============

.. image:: /images/data_inspector/simulation_cell_page.*
  :width: 50%
  :align: right

This page of the :ref:`data inspector <data_inspector>` displays information about the
:ref:`simulation cell <scene_objects.simulation_cell>` of the current dataset. The simulation cell defines
the spatial domain of the atomistic or particle-based simulation and specifies whether periodic
boundary conditions are enabled along each spatial direction.

The page is divided into four sections:

**Geometry**
  Displays the three cell vectors **a**, **b**, and **c** that define the edges of the simulation cell,
  as well as the cell origin **o**. Each vector is shown as a triplet of X, Y, Z components in the global
  Cartesian coordinate system. The cell vectors form a parallelepiped and do not need to be orthogonal;
  they can have arbitrary lengths and angles to represent non-orthogonal simulation domains (e.g., triclinic cells).

**Bounding box**
  Shows the dimensions of the axis-aligned bounding box that encloses the simulation cell.
  The width, length, and height values represent the extent of the cell along the X, Y, and Z coordinate axes,
  computed from the cell vectors. Note that these values are only meaningful for orthogonal cells that are
  aligned with the coordinate axes.

**Periodic boundary conditions**
  Indicates which spatial directions have periodic boundary conditions (PBC) enabled.
  Note that the PBC flags labeled as *X*, *Y*, and *Z* actually refer to the three cell vectors,
  which are not necessarily aligned with the coordinate axes in case of non-orthogonal cells.

**Dimensionality**
  Indicates whether the simulation is two-dimensional (2D) or three-dimensional (3D). In 2D mode,
  the Z-coordinates of particles and the third simulation cell vector are ignored in most calculations.

.. hint::

  The information shown in the data inspector is read-only and reflects the state of the simulation cell at the end of the current :ref:`data pipeline <usage.modification_pipeline>`.
  To modify the simulation cell geometry in OVITO, use the :ref:`particles.modifiers.affine_transformation` modifier in the data pipeline.

.. note::

  If the imported simulation file does not contain any cell information, this tab is not shown in the data inspector.
  You can turn on the option :guilabel:`Generate bounding box if needed` in the settings of the :ref:`file reader <file_formats.input.xyz.simulation_cell>` to let OVITO
  generate an ad-hoc simulation cell that tightly encloses all imported particles in an axis-aligned bounding box.

.. seealso:: :ref:`scene_objects.simulation_cell`