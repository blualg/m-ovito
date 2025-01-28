.. _tutorials:

=========
Tutorials
=========

.. toctree::
  :maxdepth: 1
  :hidden:

  marker_particles
  turntable_animation
  remote_rendering

.. rubric:: :ref:`Visualizing motion with marker particles <tutorials.marker_particles>`

.. image:: /images/howtos/shear_marker.gif
   :width: 20%
   :align: right

This tutorial demonstrates how to highlight a group of atoms using marker colors to track their motion within
a crystal during a molecular dynamics simulation. You will learn how to use the :ref:`particles.modifiers.freeze_property` modifier in
OVITO to preserve the initial selection state of particles.

.. rubric:: :ref:`Turntable animation of a model <tutorials.turntable_animation>`

.. image:: /images/tutorials/turntable_animation/turntable.gif
   :width: 20%
   :align: right

Learn how to create an animated movie that rotates a simulation snapshot to showcase the model from all angles.
This tutorial covers OVITO's keyframe-based :ref:`parameter animation system <usage.animation>` and techniques
for repositioning models in a 3D scene.

.. rubric:: :ref:`Remote rendering tutorial <tutorials.remote_rendering>`

This tutorial guides you through rendering a simulation video on a high-performance computing cluster using the
:ref:`Render on Remote Computer <usage.remote_rendering>` feature in *OVITO Pro*.

.. - Identify local chemical ordering (PTM modifier)
.. - How to use the DXA modifier to analyze dislocations
.. - Analyze a bulk metallic glass simulation
.. - Visualize a LAMMPS simulations with separate topology/trajectory/bond files
.. - Creating good-looking renderings of a simulation model (OSPRay)
.. - Visualize particle resident time distribution (spatial binning, time averaging)
.. - Calculate diffusion constant (Python script)
.. - Python script modifier: Warren-Cowley-SRO