.. _scene_objects.surface_mesh:

Surface meshes
--------------

A **surface mesh** is a :ref:`data object <scene_objects>` representing a closed, orientable two-dimensional manifold.
Surface meshes in OVITO are typically generated using modifiers such as:

- :ref:`particles.modifiers.construct_surface_mesh` -- Constructs a closed surface around a set of particles.
- :ref:`particles.modifiers.create_isosurface` -- Generates an isosurface from scalar field data.
- :ref:`particles.modifiers.voronoi_analysis` -- Computes the Voronoi tessellation of a particle system.
- :ref:`particles.modifiers.coordination_polyhedra` -- Creates polyhedral representations based on particle coordination.

The appearance of a surface mesh is controlled by a corresponding :ref:`visual_elements.surface_mesh` visual element,
which is found under the `Visual elements` section of the :ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>`.

.. image:: /images/visual_elements/surface_mesh_example.png
  :width: 35%
  :align: right

Periodic surfaces
"""""""""""""""""

Surface meshes can exist within a **periodic simulation domain**, meaning their triangle faces may wrap around and
connect vertices on opposite sides of a :ref:`periodic simulation cell <scene_objects.simulation_cell>`.
OVITO automatically handles these periodic intersections, ensuring a correct non-periodic representation of the triangle mesh for rendering.

See also :ref:`particles.modifiers.construct_surface_mesh.cap_polygons` for more information on this topic.

Interior and exterior region
""""""""""""""""""""""""""""

Since surface meshes are **closed orientable manifolds**, they define several spatial regions: one or more **interior** regions and an **exterior** region.

- When a surface mesh is generated using the :ref:`particles.modifiers.construct_surface_mesh` modifier,
  the enclosed volume represents the interior region filled with particles, while the exterior is the empty space (including pores).
- There may be no interior region at all, meaning the exterior region is space-filling,
  resulting in a degenerate mesh with zero faces.
- The opposite can occur as well (only in periodic domains): the interior region may extend over the entire periodic domain,
  leaving no exterior region. Again, this results in a degenerate surface mesh with zero faces.

.. seealso:: :ref:`particles.modifiers.construct_surface_mesh.regions`

Exporting to a file
"""""""""""""""""""

OVITO can export surface meshes as :ref:`triangle meshes <scene_objects.triangle_mesh>` in a non-periodic form. During export:

- Triangles intersecting periodic domain boundaries are truncated.
- `Cap polygons` are generated to fill any gaps at the periodic boundary intersections.

To export a surface mesh, use OVITO's :ref:`file export function <usage.export>` and select the **VTK** format.

.. seealso::

  * :ref:`file_formats.output`
  * `Surface mesh file writer (code example) <https://github.com/ovito-org/SurfaceMeshIO>`__
  * :py:class:`ovito.data.SurfaceMesh` (Python API)