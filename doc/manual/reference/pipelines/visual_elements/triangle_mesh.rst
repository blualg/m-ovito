.. _visual_elements.triangle_mesh:

Triangle mesh
-------------

.. image:: /images/visual_elements/triangle_mesh_panel.png
  :width: 30%
  :align: right

.. image:: /images/scene_objects/triangle_mesh_example.jpg
  :width: 30%
  :align: right

This :ref:`visual element <visual_elements>` controls the appearance of triangle meshes,
which are general polyhedral objects made of vertices and triangular faces connecting those vertices.
Typically, :ref:`triangle meshes <scene_objects.triangle_mesh>` are imported into OVITO from an external
data file, for example an STL or an OBJ file. See the :ref:`list of supported file formats <file_formats.input>`.

The faces or the vertices of a triangle mesh may be associated with color information loaded from the imported geometry file. 
If not present, the uniform color of the mesh is controlled by the user through this visualization element.

Parameters
""""""""""

Display color
  The color used for rendering the triangle mesh. Only takes effect if the mesh 
  has no per-vertex or per-face color information.

Transparency
  The degree of semi-transparency to use when rendering the mesh.

Highlight edges
  Activates the rendering of wireframe lines along the edges of the mesh's faces.

.. seealso::
  
  :py:class:`ovito.vis.TriangleMeshVis` (Python API)