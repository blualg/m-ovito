.. _particles.modifiers.ambient_occlusion:

Ambient occlusion
-----------------

.. image:: /images/modifiers/ambient_occlusion_panel.png
  :width: 30%
  :align: right

This modifier performs a quick lighting calculation to shade particles based on their
degree of occlusion by other particles. Ambient occlusion shading can provide
an improved visual perception of complex three-dimensional structures. The example pictures below
show the useful effect of the particle shading. The algorithm approximately calculates the amount of light
that is received by each particle from an omni-directional light source enclosing the entire structure (similar to an overcast sky).
OVITO uses the graphics card to quickly perform this illumination calculation.

.. figure:: /images/modifiers/ambient_occlusion_input.png
  :figwidth: 35%

  Original particles (uniform brightness)

.. figure:: /images/modifiers/ambient_occlusion_output.png
  :figwidth: 35%

  With AO modulated brightness

Note that the ambient occlusion modifier is only needed if you are using the standard :ref:`OpenGL renderer <rendering.opengl_renderer>`
or if you want quick shading of structures displayed in the interactive viewports.
Other :ref:`rendering backends <usage.rendering>` in OVITO Pro support ambient occlusion shading as a direct part
of the image rendering process itself, which typically gives better visual quality than this modifier can produce.

The modifier should always be placed at the very end of a data pipeline to obtain correct lighting results.
The modifier manipulates the existing RGB values stored in the ``Color`` particle property by modulating
them with the computed per-particle brightness factors. If the ``Color`` property is not present yet, it is created by the modifier and initialized with the
default particle colors.

Parameters
""""""""""

Shading intensity
  Percentage value controlling the magnitude of the shading effect. The
  higher the shading intensity, the darker occluded particles will appear.

Number of exposure samples
  The uniform lighting produced by an overcast sky is approximated by a finite number
  of parallel light sources, which are placed evenly around the particle structure. This parameter
  controls the number of light sources to compute the ambient occlusion shading. A small number of
  exposure samples reduces computation time, but can lead to cast shadow artifacts.

Render buffer resolution
  To compute the illumination produced by a single directed light source, the modifier
  renders the particles as seen from that light source. All visible particles will receive
  a certain amount of light while occluded particles do not. This parameter controls
  the resolution of the internal rendering buffer. For systems with a large number of particles it
  might be necessary to increase this value such that every particle occupies at least one
  pixel in the rendering buffer.

Implementation details
""""""""""""""""""""""

The modifier implements the following simple `ambient occlusion <http://en.wikipedia.org/wiki/Ambient_occlusion>`__ algorithm to
determine an illumination factor for each particle, which is used to modulate the particle color. The illumination of a sky sphere
enclosing the input structure is approximated by a large number of parallel light sources directed
at the structure. The amount of light received by a particle from one such light source is determined
by rendering a projected image of the structure from the point of view of the light source to an offscreen frame buffer.
All particles appearing in that image, which are not occluded by other particles, receive an amount of light
that is proportional to their exposed area (i.e. the number of pixels they cover in the image). Finally, the
light contributions from all light sources are added up to compute the per-particle illumination factors.

.. seealso::

  :py:class:`ovito.modifiers.AmbientOcclusionModifier` (Python API)