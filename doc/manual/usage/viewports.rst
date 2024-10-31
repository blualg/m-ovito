.. _usage.viewports:

Viewport windows
================

The interactive viewport windows of OVITO show the three-dimensional visualization scene from different angles.
The text caption in the upper left corner and the axis tripod in the lower left corner of each viewport
indicate the orientation of the virtual camera. If you don't like the standard 2-by-2 viewport layout,
you can :ref:`tailor the layout <viewport_layouts>` to your specific needs.

.. _usage.viewports.navigation:

Camera navigation
-----------------

.. image:: /images/viewport_control_toolbar/viewport_screenshot.*
  :width: 30%
  :align: right

Use the mouse to rotate or move the virtual camera of a viewport:

* Left-click and drag to rotate the camera around the current orbit center, which is located in the center of the simulation model by default.
* Right-click and drag the mouse in order to move the camera parallel to the projection plane. You can also use the middle mouse button
  or :kbd:`Shift` + left mouse button for this.
* Use the mouse wheel to zoom in or out.
* Double-click an object to reposition the orbit center to the point under the mouse cursor.
  From now on the camera will rotate around that new location marked with a three-dimensional cross.
* Double-click in an empty region of a viewport to reset the orbit center to the center of the simulation model.

Note that the z-axis is considered the "up" (vertical) direction, and OVITO constrains the camera orientation
such that this axis always points upward in the viewports. You turn this behavior off in the :ref:`viewport context menu <usage.viewports.menu>`
or change the constraint axis in the :ref:`application settings <application_settings.viewports>` of OVITO.

.. _usage.viewports.toolbar:

Viewport toolbar
----------------

.. image:: /images/viewport_control_toolbar/viewport_toolbar.*
   :width: 16%
   :align: left

The viewport toolbar is located below the viewports and provides buttons for explicitly activating various navigation input modes.
In addition, you can find two other useful functions here:

.. image:: /images/viewport_control_toolbar/zoom_scene_extents.bw.*
   :width: 32
   :align: right

The :guilabel:`Zoom Scene Extents` button automatically adjusts the virtual camera of the active viewport
such that all objects in the scene become fully visible. Use the :kbd:`Ctrl` key (:kbd:`Command` key on macOS) to
do it for all viewports at once.

.. image:: /images/viewport_control_toolbar/maximize_viewport.bw.*
   :width: 32
   :align: right

The :guilabel:`Maximize Active Viewport` button enlarges the active viewport to fill the entire main window.
Click the button a second time to restore the original 2-by-2 viewport layout.

.. _usage.viewports.menu:

Viewport menu
-------------

.. image:: /images/viewport_control_toolbar/viewport_menu_screenshot.*
   :width: 40%
   :align: right

Click on the caption text in the upper left corner of a viewport window (e.g. *Perspective*, *Top*, etc.)
to open the *viewport menu*.

:guilabel:`Preview Mode` activates the display of a virtual frame in the viewport to
precisely indicate the rectangular region visible in :ref:`rendered output images <usage.rendering>`.
The aspect ratio of the frame is determined by the output image dimensions currently set in the :ref:`Render settings <core.render_settings>` panel.

.. image:: /images/viewport_control_toolbar/viewport_preview_mode.*
   :width: 55%

:guilabel:`Constrain Rotation` restricts the orientation of the virtual camera at all times such
that the z-axis of the simulation coordinate system points upward. If needed, you can also set the *x* or *y* axes
to remain vertical in the :ref:`application settings dialog <application_settings.viewports>`.

:guilabel:`View Type` lets you switch between different standard
viewing directions and between parallel (orthogonal) and perspective projection types.

:guilabel:`Adjust View` opens the :ref:`Adjust View dialog <viewports.adjust_view_dialog>`, which gives you precise numeric
control over the position and orientation of the viewport's virtual camera.

:guilabel:`Create Camera` inserts a movable camera object into the three-dimensional
scene. This camera object is linked to the viewport, and moving the camera updates the view
accordingly and vice versa. Since you can animate the position of the camera object, you can
create fly-by animations based on a :ref:`camera motion path <usage.animation.camera>`.

The :guilabel:`Window Layout` submenu provides several functions for manipulating the current viewport layout.
OVITO creates 4 standard viewport windows by default, which are arranged in a 2-by-2 grid. You can add
and remove viewports as needed, and adjust their relative positioning by dragging the separator
lines between them with the mouse. OVITO Pro provides the option to render images and animations showing
multiple views side by side, see the option :ref:`Render all viewports <core.render_settings>`.

The :guilabel:`Pipeline Visibility` submenu lists all :ref:`data pipelines <usage.modification_pipeline>` currently shown in the viewport.
By default, all pipelines that are part of the three-dimensional scene are shown in every viewport window, but here you have the option to turn off the
display of individual objects for specific viewports. This gives you the possibility to show different
models or visualizations in different viewports -- a very useful feature for creating comparative visualizations.

Select :guilabel:`Configure Graphics...` to open the :ref:`viewports.configure_graphics_dialog` dialog, which lets
you adjust the rendering method used by the interactive viewports.