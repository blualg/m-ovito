.. _particles.modifiers.manual_selection:

Manual selection
----------------

.. image:: /images/modifiers/manual_selection_panel.png
  :width: 30%
  :align: right

This modifier lets the you manually select individual particles or bonds in the viewports using the mouse.
You can use it to restrict the action of subsequent modifiers in the pipeline to a selected
subset of elements.

Two selection tools are available: 
:guilabel:`Pick` mode lets you toggle the selection state of individual particles or bonds by clicking on them in the viewports.
:guilabel:`Fence selection` mode lets you draw a closed fence around a group of particles or bonds with the mouse cursor ("lasso tool").
All elements within the fence will be selected. Hold down the :kbd:`Ctrl` key (:kbd:`Command` on Mac) to add elements to the existing selection set.
The :kbd:`Alt` key can be used to remove elements from an existing selection set.

.. image:: /images/modifiers/manual_selection_fence_mode.png
  :width: 30%
