.. _tutorials.marker_particles:
.. _howto.marker_particles:

Motion visualization with marker particles
==========================================

.. image:: /images/howtos/shear_marker.gif
   :width: 40%
   :align: right

This step-by-step tutorial will guide you through creating an animation from an MD simulation of a simple shearing experiment, as shown on the right. You will learn how to highlight a specific group of atoms - initially located in a narrow region - using marker colors to visualize atomic motion within the crystal throughout the simulation.

You will learn about the :ref:`particles.modifiers.freeze_property` modifier in OVITO, which helps preserve the initial particle selection set from the first frame of the trajectory.

Step 1: Load simulation trajectory
""""""""""""""""""""""""""""""""""

Start by downloading the simulation trajectory file
`shear.dump <https://gitlab.com/stuko/ovito/-/blob/master/examples/data/shear.dump>`__ to your computer. This file was generated the ``shear`` simulation script found in the `LAMMPS examples folder <https://docs.lammps.org/Examples.html>`__. Use the :menuselection:`File --> Load File` function
to open the file :file:`shear.dump` in OVITO.

Step 2: Adjust animation speed
""""""""""""""""""""""""""""""

.. |play-button| image:: /images/animation_toolbar/play_animation.png
  :width: 22px
  :alt: Play button

.. |anim-settings-button| image:: /images/animation_toolbar/animation_settings.png
  :width: 22px
  :alt: Animation settings button

.. image:: /images/tutorials/marker_particles/animation_settings_dialog.jpg
   :width: 26%
   :align: right

The simulation trajectory consists of 41 frames, as indicated in the timeline below the viewports of OVITO.

  * Drag the time slider with the mouse to manually navigate through frames.
  * Press the :guilabel:`Play` |play-button| button in the animation toolbar to loop the animation in the interactive viewports.

To adjust the animation speed:

  1. Open the :ref:`Animation settings dialog <animation.animation_settings_dialog>` |anim-settings-button|.
  2. Set :guilabel:`Frames per second` to 15.

This setting affects both interactive playback and the frame rate of exported movie files.

Step 3: Create particle selection
"""""""""""""""""""""""""""""""""

.. image:: /images/tutorials/marker_particles/slice_modifier_panel.jpg
   :width: 26%
   :align: right

Next, insert the :ref:`particles.modifiers.slice` modifier into the :ref:`data pipeline <usage.modification_pipeline>` to select all particles within a narrow slab of the crystal:

  1. Select :ref:`particles.modifiers.slice` from the :guilabel:`Add modification...` drop-down list (`Modifications` section).
  2. The modifier will now appear as a new item in the :ref:`pipeline editor <usage.modification_pipeline.pipeline_listbox>`.

By default, the :ref:`particles.modifiers.slice` modifier removes particles on one side of the slicing plane.
However, in this case, we want to select particles instead of deleting them. In the :ref:`particles.modifiers.slice` modifier panel:

  1. Enable :guilabel:`Create selection (do not delete)`.
  2. Set :guilabel:`Slab width` to 5 angstroms.
  3. Check :guilabel:`Reverse orientation` to select particles located between the two parallel planes.

Step 4: Color the marker particles
""""""""""""""""""""""""""""""""""

OVITO highlights selected particles in bright red in the viewports, but this color is only a temporary indicator to show the selection state.
To permanently change the particle color for rendering:

  1. Select :ref:`particles.modifiers.assign_color` from the :guilabel:`Add modification...` drop-down list (`Coloring` section).
  2. Choose green as the new color for the selected particles.

Here is how the result looks like at different frames of the simulation:

.. image:: /images/tutorials/marker_particles/intermediate_frame0.jpg
   :width: 28%

.. image:: /images/tutorials/marker_particles/intermediate_frame20.jpg
   :width: 28%

.. image:: /images/tutorials/marker_particles/intermediate_frame40.jpg
   :width: 28%

Step 5: Freeze the particle colors
""""""""""""""""""""""""""""""""""

.. image:: /images/tutorials/marker_particles/freeze_property_color.jpg
   :width: 26%
   :align: right

If you play the animation now, you will notice that the particle selection is not static. The green slab remains straight, but different particles turn green as they enter the selection region, while others revert to white when leaving.

This happens because the `Slice` and `Assign Color` modifiers are applied dynamically in each frame.
OVITO automatically recalculates the selection and colors whenever particles move.

**Freezing the selection**

To ensure the same set of particles remains green throughout the animation, we need to "freeze" their color state.
Freezing a particle property means transferring the property values from the first frame of the trajectory to all other frames.
To freeze the particle colors:

  1. Add the :ref:`particles.modifiers.freeze_property` modifier to the pipeline.
  2. Set :guilabel:`Property to freeze` to `Color`.

This locks each particle's color from frame 0 and applies them consistently throughout the animation:

.. image:: /images/tutorials/marker_particles/final_frame0.jpg
   :width: 28%

.. image:: /images/tutorials/marker_particles/final_frame20.jpg
   :width: 28%

.. image:: /images/tutorials/marker_particles/final_frame40.jpg
   :width: 28%

.. note::

  We have placed the :ref:`particles.modifiers.freeze_property` modifier at the top of the modifier stack, which means
  it will be executed last - after the two other modifiers. This ordering is important: The :ref:`particles.modifiers.freeze_property` modifier
  is only able to preserve the particle state produced so far by preceding modifiers. Any effects of downstream modifiers in the pipeline would not be visible
  to the :ref:`particles.modifiers.freeze_property` modifier.

.. image:: /images/tutorials/marker_particles/freeze_property_selection.jpg
  :width: 26%
  :align: right

**Freezing the selection instead of the colors**

Instead of freezing the particle colors, you can freeze their selection state:

  1. Move the :ref:`particles.modifiers.freeze_property` modifier before (below) the :ref:`particles.modifiers.assign_color` modifier in the stack.
  2. Set :guilabel:`Property to freeze` to `Selection`.

Now, the frozen selection from frame 0 remains unchanged, and the :ref:`particles.modifiers.assign_color` modifier applies the green color to
the same selected particles in every frame.

Step 6: Render a movie
""""""""""""""""""""""

To complete this tutorial you will now render a movie of the simulation and save it as a video file.

Switch to the `Render` tab of the command panel and set the rendering range to :guilabel:`Complete animation`.
Click :guilabel:`Choose...` and specify the name and format of the video file to be written by OVITO, e.g. :file:`shear_marker.mp4`.
The option :guilabel:`Save to file` should now automatically be turned on.

.. image:: /images/tutorials/marker_particles/render_settings.jpg
  :width: 26%

Make sure the `Top` viewport is currently active. If there is no `Top` viewport, switch the current viewport
to top view using the :ref:`viewport menu <usage.viewports.menu>`. A `Top` viewport shows the current scene
from above, along the negative z-axis, using a parallel projection.

Finally, press the button :guilabel:`Render active viewport` to start the rendering process.

.. tip::

  To further refine the visualization you may want to perform a few additional steps:

  - Turn off the display of the :ref:`visual_elements.simulation_cell` visual element in the pipeline editor.
  - Adjust the display radius of the particles in the :ref:`visual_elements.particles` visual element to a value of **1.0**.
  - Activate :menuselection:`Preview Mode` in the :ref:`viewport menu <usage.viewports.menu>` to check the visible viewport region before rendering the video.

Download the tutorial solution
""""""""""""""""""""""""""""""

If you'd like to skip ahead or verify your solution, download the preconfigured OVITO session state file: `shear.ovito <https://gitlab.com/stuko/ovito/-/blob/master/examples/data/shear.ovito>`__

Save it in the same folder as the trajectory file :file:`shear.dump`. Open it using :menuselection:`File --> Load Session State`.

If you encounter any issues, feel free to contact us at support@ovito.org to help us improve this tutorial.
