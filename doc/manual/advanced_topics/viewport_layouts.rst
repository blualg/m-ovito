.. _viewport_layouts:

Multi-viewport layouts
----------------------

.. image:: /images/howtos/multi_viewport_layout.jpg
  :width: 60%
  :align: right

OVITO allows you to customize the default 2×2 layout of the :ref:`interactive viewports <usage.viewports>` to suit your needs.
You can also import :ref:`multiple datasets <usage.import.multiple_datasets>` or use :ref:`branched data pipelines <clone_pipeline>`
to visualize different simulations or analysis results in separate viewports.

Customizing the viewport layout
"""""""""""""""""""""""""""""""

The viewport layout consists of nested boxes, each subdivided either horizontally or vertically.
Each sub-cell can contain another nested layout box or an actual viewport window.
You can resize viewports by dragging the divider lines with the mouse.

.. image:: /images/howtos/viewport_layout_schematic.svg
  :width: 35%

**Important:** The multi-viewport layout is only visible when no viewport is maximized.
To restore the full layout view, toggle the maximize button in the :ref:`viewport toolbar <usage.viewports.toolbar>`.

To modify the layout, right-click on any blue divider line to open a context menu with options to:

  - Further subdivide a layout cell.
  - Remove a layout cell to merge adjacent viewports.
  - Reset the viewport sizes to their default proportions.

Additionally, left-click on a viewport's caption text to access the :ref:`viewport menu <usage.viewports.menu>`,
where you can split a viewport into two sections or delete the viewport from the layout.

.. figure:: /images/howtos/viewport_layout_splitter_menu.jpg
  :figwidth: 40%

  Right-click a divider line to access layout options.

.. figure:: /images/howtos/viewport_layout_viewport_menu.jpg
  :figwidth: 40%

  Left-click a viewport caption for additional options.

All layout modifications can be undone via :menuselection:`Edit --> Undo`.

.. _viewport_layouts.pipeline_visibility:

Controlling what is shown in the viewports
""""""""""""""""""""""""""""""""""""""""""

By default, all viewports display the same scene from different camera angles.
If :ref:`multiple datasets or pipelines <usage.import.multiple_datasets>` are loaded, they will appear in every viewport.

To control visibility on a per-viewport basis, use the :menuselection:`Pipeline Visibility` function
in the :ref:`viewport menu <usage.viewports.menu>`. This lets you toggle individual pipelines in specific viewports,
allowing different objects to be displayed in separate windows.

For more details, refer to the :ref:`pipeline cloning <clone_pipeline>` section to learn how to
duplicate a pipeline and modify its visualization style or input simulation file.

.. _viewport_layouts.rendering:

Rendering images or movies of a viewport layout |ovito-pro|
"""""""""""""""""""""""""""""""""""""""""""""""""""""""""""

In OVITO Pro, the :ref:`render settings panel <core.render_settings>` includes the option :guilabel:`Render all viewports`,
which allows you to capture the entire viewport layout in a rendered image or animation.

.. image:: /images/howtos/viewport_layout_rendering.jpg
  :width: 100%

For better composition, enable :guilabel:`Preview visible region` to display guides in the interactive viewports,
indicating the portions that will appear in the final render output.

.. tip::

  To make your custom viewport layout the default in OVITO,
  refer to :ref:`custom_initial_session_state`.
