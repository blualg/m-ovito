.. _tutorials.voronoi_cell:

Voronoi cell visualization
==========================

.. figure:: /images/tutorials/voronoi_cell/voronoi_cell_final.png
   :figwidth: 30%
   :align: right

   The final result of this tutorial: one Voronoi cell together with the central atom, its nearest neighbors, and the bonds connecting them.

This step-by-step tutorial teaches you how to visualize the Voronoi cell of a single particle in a 3D atomic structure using OVITO.
Starting from an amorphous structure, you will compute the Voronoi tessellation, isolate the polyhedron belonging to one specific atom,
and display it together with the atom's nearest neighbors and connecting bonds.

You will learn how to use the :ref:`particles.modifiers.voronoi_analysis` modifier to generate Voronoi polyhedra and neighbor bonds,
the :ref:`particles.modifiers.expression_select` modifier to select specific particles, bonds, and mesh regions,
the :ref:`particles.modifiers.expand_selection` modifier to include nearest neighbors in a selection,
and the :ref:`particles.modifiers.delete_selected` modifier to remove unwanted elements from the scene.

Step 1: Load simulation file
""""""""""""""""""""""""""""

.. image:: /images/tutorials/voronoi_cell/input_structure.jpg
   :width: 30%
   :align: right

Start by downloading the simulation file
`amorphous.dump <https://gitlab.com/ovito-org/ovito-sample-data/-/raw/master/tutorial/amorphous.dump?inline=false>`__
to your computer. This file contains an amorphous atomic structure. Use the :menuselection:`File --> Load File` function
to open the file :file:`amorphous.dump` in OVITO.

The goal of this tutorial is to visualize the Voronoi cell of the atom located near the center of the simulation box, which has particle ID 29.

Step 2: Voronoi analysis
""""""""""""""""""""""""

Next, insert the :ref:`particles.modifiers.voronoi_analysis` modifier into the :ref:`data pipeline <usage.modification_pipeline>`:

  1. Open the :guilabel:`Add modification...` drop-down and select `Voronoi analysis` from the `Analysis` section.
  2. In the modifier's parameter panel, activate the :guilabel:`Voronoi polyhedra` output option. This instructs the modifier to generate a :ref:`surface mesh <scene_objects.surface_mesh>` representing the Voronoi cells of all particles.
  3. Also activate the :guilabel:`Neighbor bonds` option. This generates :ref:`bonds <scene_objects.bonds>` between neighboring particles that share a Voronoi face.

After the modifier has been applied, you should see the Voronoi tessellation overlaid on the atomic structure in the viewport:

.. image:: /images/tutorials/voronoi_cell/voronoi_polyhedra.jpg
   :width: 30%

Step 3: Inspect the Voronoi cell
""""""""""""""""""""""""""""""""

Before we isolate the Voronoi cell of particle 29, let us first inspect its properties using OVITO's :ref:`Data Inspector <data_inspector>`.
The Voronoi analysis modifier computes several properties for each cell, such as its volume, surface area, and number of faces.

To look up the properties of the Voronoi cell belonging to particle ID 29, open the :ref:`Data Inspector <data_inspector>` panel at the bottom of the OVITO window:

  1. Select the :guilabel:`Surfaces` tab.
  2. Click the :guilabel:`Regions` button in the right toolbar to switch to the sub-tab that lists the individual Voronoi cells and their properties.
  3. Enter the filter expression ``ParticleIdentifier == 29`` into the search field at the top to show only the row corresponding to the Voronoi cell of particle 29.

.. image:: /images/tutorials/voronoi_cell/data_inspector.jpg
   :width: 75%

Step 4: Delete unwanted Voronoi cells
"""""""""""""""""""""""""""""""""""""

.. image:: /images/tutorials/voronoi_cell/select_surface_mesh_regions.jpg
   :width: 30%
   :align: right

The viewport currently shows all Voronoi cells at once, which is too cluttered. To isolate the cell of particle 29, we will first select
all *other* Voronoi cells using the :ref:`particles.modifiers.expression_select` modifier and then delete them.

Insert the :ref:`particles.modifiers.expression_select` modifier into the pipeline:

  1. In the :guilabel:`Operate on` drop-down box, select :guilabel:`Voronoi polyhedra --> Mesh Regions` as the data element type to operate on. This tells the modifier to evaluate its expression for each Voronoi cell (mesh region) rather than for each particle.
  2. Enter the Boolean expression ``ParticleIdentifier != 29``. This selects all Voronoi cells *except* the one belonging to particle 29.

.. image:: /images/tutorials/voronoi_cell/delete_selected_modifier.jpg
   :width: 30%
   :align: right

Now insert the :ref:`particles.modifiers.delete_selected` modifier to remove the selected mesh regions from the scene.
Make sure the modifier will operate on data elements of type :guilabel:`Mesh regions` by checking the corresponding box.

After applying both modifiers, only the Voronoi polyhedron of particle 29 remains visible:

.. image:: /images/tutorials/voronoi_cell/voronoi_cell_29.jpg
   :width: 30%

Step 5: Show only the central particle and its nearest neighbors
""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

.. image:: /images/tutorials/voronoi_cell/expand_selection_pipeline.jpg
   :width: 30%
   :align: right

.. image:: /images/tutorials/voronoi_cell/only_nearest_neighbors.png
   :width: 30%
   :align: right

The Voronoi cell is now visible, but the viewport still shows all particles in the system. To clean up the visualization,
we will keep only the central particle (ID 29) and its nearest neighbors, i.e., those atoms that share a Voronoi face with it.

  1. Insert another :ref:`particles.modifiers.expression_select` modifier (this one operating on :guilabel:`Particles`). Enter the expression ``ParticleIdentifier == 29`` to select the central particle.
  2. Insert the :ref:`particles.modifiers.expand_selection` modifier. Set the expansion mode to :guilabel:`Bonded`. This extends the selection from particle 29 to all particles that are connected to it by a bond. Since the Voronoi analysis modifier generated bonds between Voronoi neighbors in Step 2, the expanded selection now includes exactly the nearest neighbors of particle 29.
  3. Insert the :ref:`particles.modifiers.invert_selection` modifier to invert the selection. Now all particles *except* particle 29 and its nearest neighbors are selected.

Note that you do not need to insert an additional :ref:`particles.modifiers.delete_selected` modifier here. Simply make sure to place the three new modifiers *before* (below) the existing :ref:`particles.modifiers.delete_selected` modifier from Step 4 in the pipeline. Since that modifier deletes all selected elements regardless of type, it will remove both the selected mesh regions and the selected particles in one step.

Step 6: Show only bonds incident on the central particle
""""""""""""""""""""""""""""""""""""""""""""""""""""""""

.. image:: /images/tutorials/voronoi_cell/select_bonds_panel.jpg
   :width: 30%
   :align: right

There may still be unwanted bonds visible in the scene. To clean up, we will remove all bonds that are *not* connected to the central particle.

Insert another :ref:`particles.modifiers.expression_select` modifier *before* (below) the :ref:`particles.modifiers.delete_selected` modifier
in the pipeline and set it to operate on :guilabel:`Bonds`. Enter the following Boolean expression:

.. code-block:: none

  @1.ParticleIdentifier != 29 && @2.ParticleIdentifier != 29

In bond selection mode, the prefixes ``@1.`` and ``@2.`` provide access to the properties of the two particles connected by each bond.
The expression above selects all bonds for which *neither* endpoint is particle 29 -- in other words, bonds that are not incident on the central particle.

No additional :ref:`particles.modifiers.delete_selected` modifier is needed. The one already present at the end of the pipeline removes all
currently selected elements -- mesh regions, particles, and bonds -- in a single step.

The final result shows the Voronoi cell of particle 29 together with the central atom, its nearest neighbors, and the bonds connecting them:

.. image:: /images/tutorials/voronoi_cell/voronoi_cell_final.png
   :width: 30%

If you encounter any problems with this tutorial, please drop us an email at support@ovito.org to help us improve
the instructions.
