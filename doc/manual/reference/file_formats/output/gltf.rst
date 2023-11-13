.. _file_formats.output.gltf:

glTF file exporter |ovito-pro|
------------------------------

.. versionadded:: 3.10.0

This file format writer exports the entire visualization scene from OVITO to a 3d model file in the `glTF <https://www.khronos.org/gltf/>`__ format, which can be imported in other
applications such as *Blender* or *PowerPoint*.

.. image:: /images/io/gltf_export_powerpoint.*
  :width: 100%

*glTF* is a triangle mesh-based format, which means round objects in OVITO, such as particle spheres and bond cylinders, must be
converted to triangle meshes before they can be exported. The parameter  :guilabel:`Mesh resolution level` controls the number of triangles
used to approximate the surface of round geometries. The higher the resolution, the more triangles are generated and the smoother the surface will look.
The default value of 3 is usually sufficient for most applications.

OVITO outputs a separate glTF material for each unique particle or bond color. Thus, for a scene with many different particle colors, the resulting glTF file
can become quite large and real-time rendering in the application displaying the glTF model may become slow.

Keep in mind that applications such as *PowerPoint* are not designed to handle complex 3d models with many objects and materials
and they do not employ optimized rendering techniques for particle-based models as OVITO does.
That's why they may be unable to display scenes containing more than a few thousand particles or bonds.

.. note::

  The OVITO file exporter produces *binary* glTF files with the ``.glb`` file extension.

.. _file_formats.output.gltf.python:

Python parameters
"""""""""""""""""

If you export a scene to the glTF file format with the :py:func:`~ovito.io.export_file` Python function, the following specific keyword parameter is available:

.. py:function:: export_file(None, file, "gltf", mesh_resolution = 3, ...)
  :noindex:

  :param int mesh_resolution: A numeric value in the range 1-5, which controls the number of triangles used to approximate the surface of round geometries.
                              The higher the resolution, the more triangles are generated and the smoother the surface will look.
