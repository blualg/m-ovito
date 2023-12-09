.. _file_formats.output.gltf:

glTF file exporter |ovito-pro|
------------------------------

.. versionadded:: 3.10.0

This file writer exports the entire visualization scene from OVITO to the `glTF <https://www.khronos.org/gltf/>`__ format, which
is a universal file format for 3d models. It can be imported in other applications such as *Blender* or *PowerPoint*,
or it can be embedded into web pages.

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
  This is the recommended format for glTF files, because it is more compact than the *text-based* ``.gltf`` format.

.. _file_formats.output.gltf.webpages:

Publishing glTF models on the web
"""""""""""""""""""""""""""""""""

.. |model-viewer-demo| raw:: html

  <script type="module" src="https://ajax.googleapis.com/ajax/libs/model-viewer/3.3.0/model-viewer.min.js"></script>
  <model-viewer src="../../../_static/ovito_logo.glb" camera-controls poster="../../../_static/ovito_logo_poster.webp" shadow-intensity="0.91" exposure="0.89" shadow-softness="0.03" environment-image="legacy" camera-orbit="155.9deg 67.65deg 28.94m" field-of-view="29.33deg"> </model-viewer>

You can use the `<model-viewer> <https://modelviewer.dev/>`__ web component or other, similar tools to embed glTF 3d models into web pages.
Here is an example of how to embed the OVITO logo model into a web page: *Click and drag to rotate the model.*
|model-viewer-demo|

Note that the 3d viewer component works only in the `online version of this document <https://docs.ovito.org/reference/file_formats/output/gltf.html>`__ due to web browser security restrictions.
In the offline version of the OVITO docs, you will see only a static image of the model.

.. _file_formats.output.gltf.python:

Python parameters
"""""""""""""""""

If you export a scene to the glTF file format with the :py:func:`~ovito.io.export_file` Python function, the following specific keyword parameter is available:

.. py:function:: export_file(None, file, "gltf", mesh_resolution = 3, ...)
  :noindex:

  :param int mesh_resolution: A numeric value in the range 1-5, which controls the number of triangles used to approximate the surface of round geometries.
                              The higher the resolution, the more triangles are generated and the smoother the surface will look.
