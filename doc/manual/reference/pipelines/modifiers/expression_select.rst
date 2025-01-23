..
  This page was last revised on 15-JAN-25.

.. _particles.modifiers.expression_select:

Expression selection
--------------------

.. image:: /images/modifiers/expression_select_panel.png
  :width: 30%
  :align: right

This modifier allows you to select :ref:`particles <scene_objects.particles>`, :ref:`bonds <scene_objects.bonds>`, or other data elements based on user-defined criteria,
using a Boolean expression. The modifier evaluates the expression for each input element, selecting those for which the Boolean expression returns a non-zero result (*true*).
All other elements, for which the expression evaluates to zero (*false*), are deselected.

The Boolean expression can include references to both local properties and :ref:`global quantities <usage.global_attributes>`, such as the simulation cell size or the
current timestep number. This flexibility allows for dynamic selection of elements based on properties like position, type, energy, or combinations of these.
The lower panel of the modifier's user interface displays a list of input variables that can be included in the expression, as shown in the accompanying screenshot.

Expressions can include comparison operators (e.g., ``==``, ``!=``, ``>=``), logical operators (AND and OR, represented as ``&&`` and ``||``),
arithmetic calculations and math functions:

.. code-block:: none

  ParticleType == 1 && Position.Z > 2.5

.. _particles.modifiers.expression_select.syntax:

Expression syntax
"""""""""""""""""

The syntax for expressions is similar to the C programming language. You can build arithmetic expressions using
float literals, variables, or functions, combined with operators.

.. attention::

  The expression parser of OVITO is case-sensitive. This means that variable names must be spelled exactly as they appear in the list of available input variables.
  Spaces in property names are simply left out in the corresponding variable names. For example, the particle property *Particle Type* is accessible as the variable
  ``ParticleType`` in the expression. Other invalid characters in property names are replaced by underscores in the variable names.

Operators are evaluated in the following precedence:

.. table::
  :widths: auto

  ======================================================== ========================================================================================
  Operator                                                 Description
  ======================================================== ========================================================================================
  ``(...)``                                                Parentheses for explicit precedence
  ``A^B``                                                  Exponentiation (*A* raised to the power *B*)
  ``A*B``, ``A/B``                                         Multiplication and division
  ``A+B``, ``A-B``                                         Addition and subtraction
  ``A==B``, ``A!=B``, ``A<B``, ``A<=B``, ``A>B``, ``A>=B`` Comparisons between *A* and *B* (yielding either 0 or 1)
  ``A && B``                                               Logical AND: True if both *A* and *B* are non-zero
  ``A || B``                                               Logical OR: True if *A*, *B*, or both are non-zero
  ``A ? B : C``                                            Conditional (ternary) operator: If *A* differs from 0, yields *B*, else *C*
  ======================================================== ========================================================================================

OVITO's expression parser supports the following functions:

.. table::
  :widths: auto

  =================== =========================================================================
  Function name       Description
  =================== =========================================================================
  ``abs(A)``          Absolute value of A. If A is negative, returns -A otherwise returns A.
  ``acos(A)``         Arc-cosine of A. Returns the angle, measured in radians, whose cosine is A.
  ``acosh(A)``        Same as ``acos()`` but for hyperbolic cosine.
  ``asin(A)``         Arc-sine of A. Returns the angle, measured in radians, whose sine is A.
  ``asinh(A)``        Same as ``asin()`` but for hyperbolic sine.
  ``atan(A)``         Arc-tangent of A. Returns the angle, measured in radians, whose tangent is A.
  ``atan2(Y,X)``      Two argument variant of the arctangent function. Returns the angle, measured in radians. see `here <http://en.wikipedia.org/wiki/Atan2>`__.
  ``atanh(A)``        Same as ``atan()`` but for hyperbolic tangent.
  ``avg(A,B,...)``    Returns the average of all arguments.
  ``cos(A)``          Cosine of A. Returns the cosine of the angle A, where A is measured in radians.
  ``cosh(A)``         Same as ``cos()`` but for hyperbolic cosine.
  ``exp(A)``          Exponential of A. Returns the value of e raised to the power A where e is the base of the natural logarithm, i.e. the non-repeating value approximately equal to 2.71828182846.
  ``fmod(A,B)``       Returns the floating-point remainder of A/B (rounded towards zero).
  ``rint(A)``         Rounds A to the closest integer. 0.5 is rounded to 1.
  ``ln(A)``           Natural (base e) logarithm of A.
  ``log10(A)``        Base 10 logarithm of A.
  ``log2(A)``         Base 2 logarithm of A.
  ``max(A,B,...)``    Returns the maximum of all values.
  ``min(A,B,...)``    Returns the minimum of all values.
  ``sign(A)``         Returns: 1 if A is positive; -1 if A is negative; 0 if A is zero.
  ``sin(A)``          Sine of A. Returns the sine of the angle A, where A is measured in radians.
  ``sinh(A)``         Same as ``sin()`` but for hyperbolic sine.
  ``sqrt(A)``         Square root of a value.
  ``sum(A,B,...)``    Returns the sum of all parameter values.
  ``tan(A)``          Tangent of A. Returns the tangent of the angle A, where A is measured in radians.
  =================== =========================================================================

The parser supports the following constants in expressions:

.. table::
  :widths: auto

  =================== =========================================================================
  Constant name       Description
  =================== =========================================================================
  ``pi``              Pi (3.14159...)
  ``inf``             Infinity (∞)
  =================== =========================================================================

.. _particles.modifiers.type_names:

Type names in expressions
"""""""""""""""""""""""""

.. versionadded:: 3.12.0

Particle types and bond types are represented by unique numeric identifiers in OVITO, i.e., the ``ParticleType`` variable evaluates to an integer value
uniquely identifying the type of the current particle. The same applies to other :ref:`typed properties <scene_objects.particle_types>`
in OVITO such as the *Structure Type* property.

Each type may have a human-readable name associated with it, e.g., the numeric type 1 may be named *"Cu"*.
You can use these type names in expressions, e.g., ``ParticleType == "Cu"``, where the type name is enclosed in double quotes.

There may be cases where multiple numeric types are associated with the same name, e.g., both types 2 and 3 may be named *"Ni"*,
because there are two different kinds of "nickel" atoms in the simulation. In such cases, only the ``==`` and ``!=`` comparison operators are
allowed. The expression parser performs an implicit expansion for you:

.. code-block:: none

  ParticleType == "Ni"    -->    ParticleType == 2 || ParticleType == 3

.. code-block:: none

  ParticleType != "Ni"    -->    ParticleType != 2 && ParticleType != 3

References to nonexistent type names, i.e., names with no corresponding numeric value, are implicitly replaced with the special value ``inf``:

.. code-block:: none

  ParticleType == "undefined"    -->    ParticleType == inf

This expression evaluates to *false* for any numeric particle type.

.. attention::

  The type names are case-sensitive. For example, the type name ``"Cu"`` is not the same as ``"cu"``.

.. note::

  If there are multiple :ref:`typed properties <scene_objects.particle_types>`, each defining a type with the same name but different numeric IDs,
  the expression parser will attempt to resolve the ambiguity based on the context. For example, in the expressions
  ``ParticleType == "alpha"`` and ``StructureType == "alpha"``, the left-hand side of the expression helps determine the user's intent.
  The parser can replace ``"alpha"`` with the right numeric type ID for that particle property.
  However, there are cases where such a resolution is impossible. Then the expression parser will raise an error.

Selection of bonds
""""""""""""""""""

In bond selection mode, the modifier makes the properties of the current bond available as normal expression variables.
In addition, properties of the two particles connected by the bond can be accessed by prepending
``@1.`` or ``@2.`` to the particle property name. For example, the following expression selects bonds that
connect two particles of different type and whose length exceeds a threshold value of 2.8:

.. code-block:: none

  @1.ParticleType != @2.ParticleType && BondLength > 2.8

Note that a bond's direction is arbitrary. It either points from particle *A* to particle
*B*, or vice versa. Accordingly, ``@1.`` and ``@2.`` alike can refer to either one of the
two particles. More complex expressions may thus be necessary to account
for the two possibilities:

.. code-block:: none

  (@1.ParticleType == "H" && @2.ParticleType == "C") || (@2.ParticleType == "H" && @1.ParticleType == "C")

Additional expression examples
""""""""""""""""""""""""""""""

Select particles within a cylindrical region of radius 10:

.. code-block:: none

  sqrt(Position.X*Position.X + Position.Y*Position.Y) <= 10.0

Select all particles in the upper half of the simulation box:

.. code-block:: none

  ReducedPosition.Z > 0.5

Given an existing selection of particles (represented by a non-zero ``Selection`` property), filter the set
to include only those with a positive charge:

.. code-block:: none

  Selection && Charge > 0

Additional selection tools in OVITO
"""""""""""""""""""""""""""""""""""

:ref:`particles.modifiers.select_particle_type` modifier:
  Use this modifier to select particles based on just their type.
  It is a more user-friendly alternative to the *Expression selection* modifier.

:ref:`particles.modifiers.compute_property` modifier:
  Allows you to directly set the ``Selection`` property of
  particles based on a user-defined expression. Thus, it may also be used to create a particle selection by setting the
  ``Selection`` property to 1 for selected particles and 0 for unselected particles. In contrast to the *Expression selection* modifier, the
  :ref:`particles.modifiers.compute_property` modifier has the option to include neighboring particles, up to some user-defined cutoff distance,
  in the selection process. For example, you can select particles based on whether they are surrounded by a certain number or certain kind of neighbors.

`Match Molecule <https://github.com/ovito-org/MatchMolecule>`__ modifier:
  Available as an extension for OVITO Pro, it allows you to create
  selections in bonded (molecular) systems using query expressions. These selection expressions are based on a simplified version of the SMILES language
  and let you to select atoms, bonds, or sections of a molecule based on their chemical environment.

:ref:`particles.modifiers.python_script` modifier:
  Use custom Python functions in OVITO Pro to implement complex selection logic. The Python interface provides
  useful spatial queries, e.g., cutoff-based neighbor searches, k-nearest neighbor searches,
  or point-in-region tests.

.. seealso::

  :py:class:`ovito.modifiers.ExpressionSelectionModifier` (Python API)