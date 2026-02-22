.. _particles.modifiers.coordination_polyhedra:

Coordination polyhedra
----------------------

This modifier constructs *coordination polyhedra* around the currently selected atoms for visualization purposes.
The coordination polyhedron of an atom is the convex hull spanned by the bonded neighbors of the central atom.
The modifier itself has no configurable parameters; its operation is fully controlled by the input it receives from the
upstream data pipeline.

.. note::

  The modifier creates coordination polyhedra only for currently selected particles. That means you first need to select
  the particles for which you would like to generate coordination polyhedra -- using the 
  :ref:`particles.modifiers.select_particle_type` modifier, for example.

.. image:: /images/modifiers/coordination_polyhedra_example.png
  :width: 30%
  :align: right

The modifier takes into account the :ref:`bonds <scene_objects.bonds>` a selected particle has to its neighbors to build 
its coordination polyhedron. Thus, in case your system doesn't contain bonds yet, it is necessary to have OVITO generate 
the bonds first by inserting the :ref:`particles.modifiers.create_bonds` modifier.

The coordination polyhedra created by the modifier form a :ref:`surface mesh <scene_objects.surface_mesh>` and
their visual appearance, for example the surface transparency, can be configured through the associated 
:ref:`visual element <visual_elements.surface_mesh>` named :guilabel:`Polyhedra`, which appears in the :ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>` 
after the modifier has created the coordination polyhedra.

Properties of the input particles become available as local properties of the generated polyhedral mesh. 
Each polyhedron forms a separate *region* of the mesh and is associated with the properties of the central particle. 
Furthermore, each vertex of the polyhedral mesh is associated with the properties of the neighbor particle it was created from.
This gives you the possibility to visualize these particle properties on the polyhedra surfaces by making use of the color mapping function 
of the :ref:`surface mesh <visual_elements.surface_mesh>` visual element.

.. seealso::

  :py:class:`ovito.modifiers.CoordinationPolyhedraModifier` (Python API)