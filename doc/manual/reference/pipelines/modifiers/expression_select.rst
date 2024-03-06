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

Usage examples
""""""""""""""

The first expression below will select all particles of numeric type 1 or 2, similar to what the :ref:`particles.modifiers.select_particle_type` modifier
does. The second expression will select particles within a cylindrical region of radius 10::

    ParticleType==1 || ParticleType==2

    sqrt(Position.X*Position.X + Position.Y*Position.Y) < 10.0

.. seealso::

  :py:class:`ovito.modifiers.ExpressionSelectionModifier` (Python API)