.. _particles.modifiers.color_by_type:

Color by type |ovito-pro|
-------------------------

.. image:: /images/modifiers/color_by_type_panel.png
  :width: 30%
  :align: right

This modifier sets the **Color** property of particles, bonds, or other data elements
to visualize the discrete values of one of their *typed properties*. A typed property 
is one that has some named types defined, associating each numeric value with a corresponding name and color, for example:

  * Typed particle properties:
  
    - ``Particle Type``     
    - ``Residue Type``        
    - ``Atom Name``           
    - ``Structure Type``      
    
  * Typed bond properties:
  
    - ``Bond Type``     

While this modifier allows you to color the data elements based on one of these properties, 
the :ref:`color legend <viewport_layers.color_legend>` layer may be added to the active viewport to include a legend with type names
and the corresponding colors in the rendered picture. The following picture shows two different applications 
of the *Color by type* modifier to visualize the ``Residue Type`` and the ``Particle Type``
particle properties of a molecule, respectively:

.. image:: /images/modifiers/color_by_type_example.png
  :width: 65%

The *Color by type* modifier lets you color particles and bonds based on some *discrete* property with predefined named types.
OVITO furthermore offers the :ref:`particles.modifiers.color_coding` modifier, which is the right tool to visualize the values of *continuous* properties. 

Parameters
""""""""""

Operate on
  Selects the type of data elements (particles, bonds, voxels, etc.) the modifier should operate on.

Property
  The typed property that serves as input for the modifier and which the assigned colors should be based on. 
  The drop-down list shows only properties with discrete values for which at least one element type has been defined, 
  e.g., a particle type.

Color only selected elements
  This option lets the modifier assign new colors only to the currently selected data elements. 
  Existing colors of the unselected elements will be preserved. This allows you to color only a subset of the 
  particles or bonds. Use the :ref:`particles.modifiers.select_particle_type` modifier, for example,
  to first create a selection and define the subset of data elements that are going to be colored by type.

  The :guilabel:`Clear selection` option tells the modifier to reset the element selection after 
  it has assigned the colors. This is useful in order to see the assigned colors in the interactive viewports of
  OVITO, which may otherwise be masked by the red highlighting color used for rendering selected elements.

Types
  The list of types defined by the selected input property and their associated colors. 
  Note that this list is not directly editable in the modifier panel. To change the colors representing the individual types,
  you have to modify the original particle or bond types created by the pipeline's :ref:`data source <data_sources>`.

  
.. seealso::

  :py:class:`ovito.modifiers.ColorByTypeModifier` (Python API)