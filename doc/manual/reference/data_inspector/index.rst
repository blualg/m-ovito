.. _data_inspector:

Data inspector
==============

.. image:: /images/usage/miscellaneous/data_inspector.*
  :width: 50%
  :align: right

The data inspector is a panel located below the viewport area, providing a comprehensive view of the data
loaded from the input file and/or dynamically computed by the data pipeline. The data inspector panel is initially
in a collapsed state, with only the tab bar visible. It can be expanded by clicking anywhere in the tab bar.
The panel consists of several pages, each being dynamically shown depending on the kinds of
data that are present in the current dataset. For example, there is a page
listing all particles and their property data in tabular form.

The following pages are part of the data inspector and are shown only if the corresponding type
of data is present in the pipeline output:

* :ref:`data_inspector.particles`
* :ref:`data_inspector.bonds`
* Angles
* Dihedrals
* Impropers
* :ref:`data_inspector.types`
* :ref:`data_inspector.simulation_cell`
* :ref:`data_inspector.attributes`
* :ref:`data_inspector.data_tables`
* :ref:`data_inspector.voxel_grids`
* Surfaces
* :ref:`data_inspector.dislocations`

.. toctree::
  :hidden:

  particles
  bonds
  types
  simulation_cell
  attributes
  data_tables
  voxel_grids
  dislocations
