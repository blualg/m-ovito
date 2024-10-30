.. _rendering.opengl_renderer:

OpenGL renderer
===============

.. image:: /images/rendering/opengl_renderer_panel.*
  :width: 30%
  :align: right

This is the default built-in :ref:`rendering engine <usage.rendering>`,
which is also used by OVITO for rendering the interactive viewports.

The "More Options" (vertical ellipsis) button next to each numerical parameter opens a context menu with
the option to reset each parameter to its default value.

Parameters
""""""""""

Anti-aliasing level
  To reduce `aliasing effects <http://en.wikipedia.org/wiki/Aliasing>`__, the output image is usually rendered at a higher resolution
  than the final image (*supersampling*). This factor controls how much larger this
  resolution is. A factor of 1 turns anti-aliasing off. Higher values lead to better quality.

Transparency rendering method
  Selects the method for rendering semi-transparent objects when they overlap with each other.
  See :ref:`viewports.configure_graphics_dialog` for more information on the available choices.

.. seealso::

  :py:class:`~ovito.vis.OpenGLRenderer` (Python API)