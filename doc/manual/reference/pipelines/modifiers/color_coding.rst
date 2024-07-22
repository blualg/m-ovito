.. _particles.modifiers.color_coding:

Color coding
------------

.. image:: /images/modifiers/color_coding_panel.png
  :width: 30%
  :align: right

This modifier assigns colors to particles, bonds, vector glyphs, :ref:`voxels <scene_objects.voxel_grid>` or :ref:`surfaces <scene_objects.surface_mesh>` according to a selected input property.
It thus provides a simple way of visualizing local values by means of a color scheme.

The field :guilabel:`Operate on` selects the type of data elements the modifier should assign colors to:

.. table::
  :widths: auto

  ================ =================================================================================
  Operate on       Description
  ================ =================================================================================
  Particles        Colors particles based on a local property.
  Bonds            Colors bonds based on a local bond property.
  Particle vectors Colors :ref:`vector glyphs <visual_elements.vectors>` by setting the ``Vector Color`` property of the associated particles.
  Voxel grids      Colors the cells of a :ref:`voxel grid <scene_objects.voxel_grid>` according to a local property.
  Mesh faces       Colors the facets of a :ref:`surface mesh <scene_objects.surface_mesh>` according to a local property.
  Mesh vertices    Colors the vertices of a :ref:`surface mesh <scene_objects.surface_mesh>` according to a local property.
  Mesh regions     Colors the volumetric components of a :ref:`surface mesh <scene_objects.surface_mesh>`.
  Lines            Colors :ref:`Lines <scene_objects.lines>` created by the :ref:`particles.modifiers.generate_trajectory_lines` modifier or the or added using the :py:attr:`DataCollection.lines.create() <ovito.data.DataCollection.lines>` method.
  ================ =================================================================================

The color getting assigned to an element (e.g. a particle) is determined by the modifier as follows:
First, the value of the selected input property of the element is linearly mapped (and clamped) to the unit interval [0,1] according to the
parameters :guilabel:`Start value` and :guilabel:`End value` of the modifier. The normalized value is then mapped to an RGB color based on the selected color gradient.

The function :guilabel:`Adjust range` resets the :guilabel:`Start value` and
:guilabel:`End value` parameters to the current minimum and maximum
of the selected input property, respectively. This ensures that no clamping of input values occurs for the current
simulation frame. The function :guilabel:`Adjust range (all frames)` does the same but takes into account
the entire simulation trajectory to determine the range of input property values.
The function :guilabel:`Reverse range` simply
swaps the :guilabel:`Start value` and :guilabel:`End value` parameters to effectively reverse the color scale.

Custom color gradients
""""""""""""""""""""""

The modifier offers a set of predefined color gradients that you can choose from. If you would like to define your
own type of color map instead, select :guilabel:`Load custom color map...` from the gradients list.
The program will ask you to pick an image file on your computer containing the custom color gradient.
You can prepare such a file using any image editing program. OVITO will use the RGB colors
of one row of pixels of the imported image to generate the user-defined color map. Here are two examples of color gradient images:

+-----------------------------------------------------------+
| .. image:: /images/modifiers/color_coding_custom_map.png  |
+-----------------------------------------------------------+

+-----------------------------------------------------------+
| .. image:: /images/modifiers/color_coding_custom_map2.png |
+-----------------------------------------------------------+

Color legend
""""""""""""

If you want to include the color map of the modifier in rendered images or animations as a legend, add the
:ref:`Color legend <viewport_layers>` layer to the active viewport.

Parameters
""""""""""

Operate on
  Selects the kind of data elements to be colored by the modifier (see table above). Note that the modifier lets you select only from those
  data elements that are currently present in the dataset the modifier receives as input from the data pipeline.

Property
  The input property of the data elements to be used as basis for the coloring.

Color gradient
  Selects the color gradient for mapping property values to colors.

Start & end value
  These parameters specify the source range of input property values getting mapped to the normalized [0,1] interval of the color map.

Automatic range
  With this option turned on the modifier will dynamically adjust the start and end value parameters to the current min/max range of input property values.

Symmetric range
When this option is enabled, the modifier will automatically adjust the value range to be symmetric around 0. 
Manually setting the maximum value will also adjust the minimum value to match.

Color only selected elements
  This option restricts the assignment of colors to currently selected particles or bonds. The colors of unselected particles or bonds will remain unchanged.

Keep selection
  OVITO highlights selected particles in red in the interactive viewports.
  Since this would mask the actual colors assigned by this modifier, the modifier clears the current selection by default.
  This option retains the current selection so that it remains available to subsequent modifiers in the data pipeline.

.. seealso::

  :py:class:`ovito.modifiers.ColorCodingModifier` (Python API)
