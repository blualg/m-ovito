.. _usage.viewports:

Viewport windows
================

.. image:: /images/viewport_control_toolbar/viewport_screenshot.*
  :width: 30%
  :align: right

OVITO's interactive viewport windows display the 3D visualization scene from different angles.
Each viewport has a text caption (top-left corner) and an axis tripod (bottom-left corner) indicating the camera's
orientation relative to the global Cartesian coordinate system. The default 2×2 viewport layout can be
:ref:`customized <viewport_layouts>` to suit your needs.

.. _usage.viewports.navigation:

Camera navigation
-----------------

You can navigate the viewport using the mouse:

- **Rotate** - Left-click and drag to rotate around the orbit center (default: model center).
- **Pan** - Right-click and drag (or use the middle mouse button or :kbd:`Shift` + left-click).
- **Zoom** - Use the mouse wheel.
- **Reposition orbit center** - Double-click an object to move the orbit center to the clicked point (marked by a 3D cross).
- **Reset orbit center** - Double-click an empty area to reset it to the model center.

By default, the z-axis remains upright. If you want to move the camera more freely, this constraint can be disabled in the
:ref:`viewport context menu <usage.viewports.menu>` or adjusted in :ref:`application settings <application_settings.viewports>`.

.. _usage.viewports.toolbar:

Viewport toolbar
----------------

.. image:: /images/viewport_control_toolbar/viewport_toolbar.*
   :width: 16%
   :align: right

The toolbar below the viewports provides buttons for navigation modes and other useful functions, such as:

.. |zoom-scene-extents-icon| image:: /images/viewport_control_toolbar/zoom_scene_extents.bw.*
   :width: 32
   :align: middle

.. |maximize-viewport-icon| image:: /images/viewport_control_toolbar/maximize_viewport.bw.*
   :width: 32
   :align: middle

|zoom-scene-extents-icon| :guilabel:`Zoom Scene Extents`
   Adjusts the camera to fit all objects in the viewport. Hold :kbd:`Ctrl` (:kbd:`Command` on macOS) to apply this to all viewports.

|maximize-viewport-icon| :guilabel:`Maximize Active Viewport`
   Expands the active viewport to fill the window. Click again to restore the original layout.

.. _usage.viewports.menu:

Viewport menu
-------------

.. image:: /images/viewport_control_toolbar/viewport_menu_screenshot.*
   :width: 40%
   :align: right

Click a viewport's caption (*"Perspective"*, *"Top"*, etc.) in the top-left corner to open the *viewport menu*, which provides various options:

:guilabel:`Preview Mode`
   Displays a frame showing the visible area in :ref:`rendered images <usage.rendering>`.
   The frame's aspect ratio matches the output image dimensions specified in the :ref:`render settings <core.render_settings>`:

   .. image:: /images/viewport_control_toolbar/viewport_preview_mode.*
      :width: 55%

:guilabel:`Constrain Rotation`
   Keeps the z-axis upright during camera rotation. You can change the constraint axis in the :ref:`application settings <application_settings.viewports>`.

:guilabel:`View Type`
   Switch between standard viewing directions and perspective/orthogonal projections.

:guilabel:`Create Camera`
   Adds a movable camera object to the scene, which is linked to the viewport.
   This allows for animated fly-by sequences via a :ref:`camera motion path <usage.animation.camera>`.

:guilabel:`Adjust View`
   Opens the :ref:`Adjust View dialog <viewports.adjust_view_dialog>` for precise numeric control over the camera's position and orientation.

:guilabel:`Window Layout`
   Modify the viewport window arrangement. OVITO defaults to a 2×2 grid but allows adding, removing, and resizing viewports.
   You can adjust individual viewports by dragging the separator lines between them.
   OVITO Pro also supports rendering :ref:`multi-viewport images and animations <viewport_layouts>`.

:guilabel:`Pipeline Visibility`
   Control which :ref:`data pipelines <usage.modification_pipeline>` appear in each viewport.
   This is useful for comparative visualizations, allowing different models or visualizations in separate viewports.
   For details, see :ref:`usage.import.multiple_datasets` and :ref:`viewport_layouts.pipeline_visibility`.

:guilabel:`Configure Graphics`
   Opens the :ref:`Viewport Graphics Configuration <viewports.configure_graphics_dialog>` dialog, where you can adjust the real-time
   rendering method for interactive viewports.
