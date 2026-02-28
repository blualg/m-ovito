.. _tutorials:

=========
Tutorials
=========

.. toctree::
  :maxdepth: 1
  :hidden:

  marker_particles
  turntable_animation
  voronoi_cell
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

.. rubric:: :ref:`Voronoi cell visualization <tutorials.voronoi_cell>`

.. image:: /images/tutorials/voronoi_cell/voronoi_cell_final.png
   :width: 20%
   :align: right

Learn how to visualize the Voronoi cell of a single particle in a 3D atomic structure. This tutorial covers the
:ref:`particles.modifiers.voronoi_analysis` modifier, selection of individual mesh regions, bonds, and particles
using the :ref:`particles.modifiers.expression_select` modifier, and how to isolate nearest neighbors using the
:ref:`particles.modifiers.expand_selection` modifier.

.. rubric:: :ref:`Remote rendering tutorial <tutorials.remote_rendering>`

This tutorial guides you through rendering a simulation video on a high-performance computing cluster using the
:ref:`Render on Remote Computer <usage.remote_rendering>` feature of *OVITO Pro*.

