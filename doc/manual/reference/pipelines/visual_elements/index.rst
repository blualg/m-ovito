.. _visual_elements:

Visual elements
===============

.. image:: /images/visual_elements/visual_elements_panel.png
  :width: 35%
  :align: right

OVITO distinguishes between the underlying data and the visual representations produced from that data.
One typical example for this separation between :ref:`data objects <scene_objects>`
and their visual representation is the ``Position`` particle
property, which holds the XYZ coordinates of a set of point-like particles. To visualize this data, OVITO automatically creates
a :ref:`Particles <visual_elements.particles>` *visual element*, which is responsible for rendering a
corresponding set of spheres in the viewports, using the ``Position`` particle property as source data.
The :ref:`Particles <visual_elements.particles>` visual element provides several parameters that let you control
the visual appearance of the particles, e.g. the glyph shape (spheres, discs, cubes, etc.).

This separation of *data objects* and *visual elements* provides additional flexibility within OVITO.
It becomes possible to visualize the same data in several different ways (multiple visual elements based on the same source data) and to
:ref:`visualize multiple datasets side by side <clone_pipeline>` (one visual element rendering several data objects in the same way).

Visual elements are usually created automatically by OVITO's :ref:`data pipeline system <usage.modification_pipeline>`
alongside with the imported or computed data. They all appear under the :guilabel:`Visual elements` section of
the :ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>` shown in the screenshot.
You can disable individual visual elements using the check boxes next to them in the pipeline editor.
This will turn off the visualization of the corresponding data in the interactive viewports and in rendered images.

.. table::
  :widths: 25 75

  ================================================================== =================
  Visual element                                                     Description
  ================================================================== =================
  :ref:`Particles <visual_elements.particles>`                       Renders a set of particles using different glyph shapes
  :ref:`Bonds <visual_elements.bonds>`                               Renders the bonds between particles as cylinders
  :ref:`Vectors <visual_elements.vectors>`                           Renders arrow glyphs to graphically represent a vector particle property
  :ref:`Simulation cell <visual_elements.simulation_cell>`           Renders the simulation box as a wireframe geometry
  :ref:`Surface mesh <visual_elements.surface_mesh>`                 Renders a smooth polygonal :ref:`surface mesh <scene_objects.surface_mesh>`
  :ref:`Triangle mesh <visual_elements.triangle_mesh>`               Renders a :ref:`triangle mesh <scene_objects.triangle_mesh>` loaded from a mesh geometry file
  :ref:`Voxel grid <visual_elements.voxel_grid>`                     Renders a :ref:`voxel grid <scene_objects.voxel_grid>`
  :ref:`Lines <visual_elements.lines>`                               Renders a set of continuous lines to visualize the particle trajectories created with the :ref:`particles.modifiers.generate_trajectory_lines` modifier or drawn using the :py:attr:`DataCollection.lines.create() <ovito.data.DataCollection.lines>` method
  :ref:`Dislocations <visual_elements.dislocations>`                 Renders dislocation lines extracted by the :ref:`particles.modifiers.dislocation_analysis` modifier
  ================================================================== =================

.. seealso::

  :py:class:`ovito.vis.DataVis` (Python API)

.. toctree::
  :maxdepth: 1
  :hidden:

  particles
  bonds
  vectors
  simulation_cell
  surface_mesh
  triangle_mesh
  voxel_grid
  lines
  dislocations
