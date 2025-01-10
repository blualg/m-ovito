.. _particles.modifiers.expression_select:

Expression selection
--------------------

.. image:: /images/modifiers/expression_select_panel.png
  :width: 30%
  :align: right

This modifier let you select particles, bonds or other data elements based on user-defined criteria, i.e., by entering a Boolean expression,
which is evaluated by the modifier for every input element.
Those elements get selected for which the Boolean expression yields a non-zero result (*true*);
all other elements, for which the expression evaluates to zero (*false*), get deselected.

The Boolean expression can contain references to local properties as well as global quantities, e.g. the simulation cell size or the current timestep number.
Hence, the modifier can be used to dynamically select elements based on properties such as position,
type, energy, etc. and any combination thereof. The list of available input variables that may be incorporated into the expression
is displayed in the lower panel as shown in the screenshot.

Boolean expressions can contain comparison operators like ``==``, ``!=``, ``>=``, etc.,
and several conditions can be combined using logical *AND* and *OR* operators (``&&`` and ``||``).

Note that variable names and function names are case-sensitive and restricted to alphanumeric characters and
underscores. That's why OVITO automatically replaces invalid characters in property names with an underscore to generate valid variable names
that can be used in the expression.

Expression syntax
"""""""""""""""""

The expression syntax is very similar to the C programming language. Arithmetic expressions can be created from
float literals, variables or functions using the following operators in this order of precedence:

.. table::
  :widths: auto

  ======================================================== ========================================================================================
  Operator                                                 Description
  ======================================================== ========================================================================================
  ``(...)``                                                expressions in parentheses are evaluated first
  ``A^B``                                                  exponentiation (A raised to the power B)
  ``A*B``, ``A/B``                                         multiplication and division
  ``A+B``, ``A-B``                                         addition and subtraction
  ``A==B``, ``A!=B``, ``A<B``, ``A<=B``, ``A>B``, ``A>=B`` comparison between A and B (result is either 0 or 1)
  ``A && B``                                               logical AND operator: result is 1 if A and B differ from 0, else 0
  ``A || B``                                               logical OR operator: result is 1 if A or B differ from 0, else 0
  ``A ? B : C``                                            if A differs from 0 (i.e. is true), the resulting value of this expression is B, else C
  ======================================================== ========================================================================================

The expression parser supports the following functions:

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

The expression parser supports the following constants:

.. table::
  :widths: auto

  =================== =========================================================================
  Constant name       Description
  =================== =========================================================================
  *pi*                Pi (3.14159...)
  *inf*               Infinity (∞)
  =================== =========================================================================

Support for typed properties
############################

Properties in OVITO are stored as integer values. Some properties, called :ref:`typed properties <scene_objects.particle_types>`, can also store a 
non-unique name. One example is *Structure Type*, where a numeric value of `1` maps to the name *"FCC"*. Similarly, *Particle Types* may  
store the chemical symbol as their names. 

These names can also be referenced inside the expressions. This allows the usage of expressions like ``ParticleType == "Cu"``, where *"Cu"*
is a type name connected to the *Particle Type* property. Note, that these type names must be enclosed in double quotes: ``"<Type Name>"``. References to
undefined type names (i.e., names with no numeric mapping) are transformed into either ``true`` or ``false`` depending on the context they are used in. 
For example, ``ParticleType != "<Type Name>"`` is alway true if the type ``"<Type Name>"`` is not defined.

In the simplest case, the mapping from numeric IDs to names is unique. For example, the particle type *"Cu"* might map exclusively to the number `1`. 
Here, the expression::

  ParticleType == "Cu"

can be transformed trivially to::

  ParticleType == 1

In this context all binary comparison operators listed in the table above are supported.

However, there are also cases where the mapping between names and numeric IDs is not unique. For example, 
particle types ``2``, ``3``, and ``5`` might all map to the name *"Ni"*.
In this case, only the ``==`` and ``!=`` comparison operators are allowed for these type names. Depending on the operator used, the resulting conditions are either 
linked by *or* (``||``) or *and* (``&&``) operators as shown in the following examples::

  1.) ParticleType == "Ni"
  2.) ParticleType != "Ni"

which will be interpreted as::

  1.) ParticleType == 2 || ParticleType == 3 || ParticleType == 5
  2.) ParticleType != 2 && ParticleType != 3 && ParticleType != 5

Duplicate type names across different type properties cannot always be resolved uniquely. For instance, if both 
*Particle Type* and *Structure Type* have a type name *alpha* associated with numeric IDs 3 and 5, respectively, 
the expression parser will attempt to resolve the ambiguity. For example, in queries like ``ParticleType == "alpha"`` 
or ``StructureType == "alpha"``, the left-hand side of the expression helps determine the user's intend.
There are, however, expression in which such a resolution is impossible: ``(<condition> ? ParticleType : StructureType) == "alpha"``. 
This leads to an error if the numeric IDs of the two *alpha* types are different. 
Such expressions are valid only if both *alpha* types map to the same numeric value.

Ternary expression, are supported on both on the condition and the result side. This can be seen in the following
examples. Assuming the same *Particle Type* mapping used above
and *Structure Type* mappings of *FCC* to ``1`` and *HCP* to ``2``. The expression::

   StructureType == (ParticleType == "Ni" ? "FCC" : "HCP")

corresponds to::

  StructureType == (ParticleType == 2 || ParticleType == 3 || ParticleType == 5 ? 1 : 2)

similarly,::
  
  ParticleType == (<condition> ? "Ni" : "Cu") 
  
will be interpreted as::

  (ParticleType == (<condition> ? 2 : 1)) || (ParticleType == (<condition> ? 3 : 1)) || (ParticleType == (<condition> ? 5 : 1))

Usage examples
""""""""""""""

The first expression below will select all particles of numeric type 1 or 2, similar to what the :ref:`particles.modifiers.select_particle_type` modifier
does. The second expression will select particles within a cylindrical region of radius 10::

    ParticleType==1 || ParticleType==2

    sqrt(Position.X*Position.X + Position.Y*Position.Y) < 10.0

.. seealso::

  :py:class:`ovito.modifiers.ExpressionSelectionModifier` (Python API)