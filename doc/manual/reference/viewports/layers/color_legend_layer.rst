.. _viewport_layers.color_legend:

Color legend layer
------------------

.. image:: /images/viewport_layers/color_legend_overlay_panel.*
   :width: 30%
   :align: right

This :ref:`viewport layer <viewport_layers>` renders the color scale for a :ref:`Color coding <particles.modifiers.color_coding>` modifier 
or the list of discrete element types and corresponding colors into the output picture. 
The color legend helps viewers to associate object colors with corresponding numeric values or element types. 
The following figure shows two typical examples of color legends: A color map for a continuous particle property 
and a discrete color legend for the ``Particle Type`` property:

.. image:: /images/viewport_layers/color_legend_example.*
   :width: 60%

Color source
""""""""""""

The selected :guilabel:`Color legend source` determines where the legend takes 
its colors from. In the current program version, three types of sources are available 
in the drop-down list:

Color coding modifier
  If the current data pipeline contains a :ref:`Color coding <particles.modifiers.color_coding>` modifier, the legend can display its 
  gradient and min/max value interval.

Typed properties
  Alternatively, the legend can display a list of discrete element types associated with a :ref:`typed property <scene_objects.particle_types>` in the data pipeline's output. 
  Typical examples are the particle properties ``Particle Type``, ``Structure Type`` or ``Residue Type``. 
  The legend will display the name and color of each type defined for the selected source property. 

  .. note::

    Note that the color legend layer itself is not responsible for coloring the particles or bonds. 
    It may be necessary to insert a :ref:`Color by type <particles.modifiers.color_by_type>` modifier 
    into the pipeline to actually give particles and bonds their corresponding colors.

Pseudo color mappings
  Some :ref:`visual elements <visual_elements>` provide an option to color 
  objects or surfaces based on a local property. If such a color mapping function has been activated
  for some visual element in the scene, it can be selected as the source of the color legend layer.

Parameters
""""""""""

The other parameters of the color legend layer let you control the color legend's size, positioning, and appearance in the rendered picture.
By default, the legend will be labeled with the name of the source property and, if based on a :ref:`Color coding <particles.modifiers.color_coding>` modifier, the 
numeric range set for that modifier. You can override the text of the labels by entering something into the input fields :guilabel:`Custom title` and :guilabel:`Custom label`.

.. note::

  It is possible to include HTML markup elements in the label texts to format them, e.g., to produce special notations such as superscripts or subscripts.
  See :ref:`here <viewport_layers.text_label.text_formatting>` for further information.

If the legend is associated with a :ref:`Color coding <particles.modifiers.color_coding>` modifier, the numeric format of the min/max values is 
controlled by a format specification string. You have the choice between decimal notation (``%f``), exponential notation (``%e``), and an 
automatic mode (``%g``), which picks the best representation depending on the value's magnitude. 
The format string also gives you explicit control over the output precision, i.e., the number of digits that
appear after the decimal point. Use ``%.2f``, for example, to always show two digits after the decimal point. 
The format string must follow the rules of the standard  `printf() C function <https://en.cppreference.com/w/cpp/io/c/fprintf>`__.

Furthermore, it is possible to include a physical unit as part of the format string, e.g., ``%g eV``. 

.. seealso::

  :py:class:`ovito.vis.ColorLegendOverlay` (Python API)
