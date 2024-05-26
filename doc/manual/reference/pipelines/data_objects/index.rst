.. _scene_objects:

Data objects
============

A dataset loaded into OVITO from a simulation file may consist of several individual *data objects*, which represent different
facets of the information, e.g. the simulation cell geometry, the atomic coordinates, the bond list, etc.
:ref:`Modifiers <particles.modifiers>` operate on these data objects and may add dynamically computed data objects to the dataset as it is processed in
the :ref:`data pipeline <usage.modification_pipeline>`.

Note that, in OVITO, the visualization of data is delegated to so-called :ref:`visual elements <visual_elements>`,
which are responsible for producing the three-dimensional graphical representation of the numerical data stored in the data objects.
That means data objects themselves have no configurable visualization parameters; only the :ref:`visual elements <visual_elements>`
rendering the graphical representation do.

.. table::
  :widths: auto

  ================================================================== ==================================================================
  Data object type                                                   Description
  ================================================================== ==================================================================
  :ref:`Particles <scene_objects.particles>`                         A set of particles which may be associated with an arbitrary set of per-particle property values
  :ref:`Bonds <scene_objects.bonds>`                                 A set of bonds connecting pairs of particles
  :ref:`Simulation cell <scene_objects.simulation_cell>`             The simulation cell geometry and boundary conditions
  :ref:`Surface mesh <scene_objects.surface_mesh>`                   A mesh structure representing a two-dimensional closed manifold embedded in the simulation domain
  :ref:`Triangle mesh <scene_objects.triangle_mesh>`                 A general polyhedral mesh made of vertices and triangular faces
  :ref:`Data table <scene_objects.data_table>`                       A table of values arranged in columns and rows, which can be visualized as a 2d data plot
  :ref:`Voxel grid <scene_objects.voxel_grid>`                       A structured 2d or 3d grid made of uniform voxel elements
  :ref:`Lines <scene_objects.lines>`                                 Trajectory lines created by the :ref:`particles.modifiers.generate_trajectory_lines` modifier or the :py:attr:`DataCollection.lines.create() <ovito.data.DataCollection.lines>` method
  :ref:`Dislocations <scene_objects.dislocations>`                   Line crystal defects extracted by the :ref:`particles.modifiers.dislocation_analysis` modifier
  :ref:`Vectors <scene_objects.vectors>`                             Arrow glyphs created by the :py:attr:`DataCollection.vectors.create() <ovito.data.DataCollection.vectors>` method
  ================================================================== ==================================================================


.. seealso::

  :py:class:`ovito.data.DataObject` (Python API)


.. toctree::
  :maxdepth: 1
  :hidden:

  particles
  bonds
  simulation_cell
  surface_mesh
  triangle_mesh
  data_table
  voxel_grid
  lines
  dislocations
  vectors