.. _particles.modifiers.select_particle_type:

Select type
-----------

.. image:: /images/modifiers/select_particle_type_panel.png
  :width: 35%
  :align: right

This modifier selects particles, bonds, and other data elements on the basis of a type property.

Common examples of typed properties are the ``Particle Type`` and the ``Structure Type`` properties
of particles, or the ``Bond Type`` property of bonds. This modifier allows you to select
elements of a certain type (or types), i.e., elements whose type property matches one of the given values.

Parameters
""""""""""

Operate on
  Selects the class of elements (particles, bonds, etc.) the modifier should operate on.

Property
  The type property to be used as a source for the selection. The drop-down box lists all available properties that store type information.

Types
  The list of types defined for the selected source property. Check one or more of them to let the modifier select all matching data elements.

.. seealso::
  
  :py:class:`ovito.modifiers.SelectTypeModifier` (Python API)
