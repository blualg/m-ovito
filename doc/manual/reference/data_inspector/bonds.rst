.. _data_inspector.bonds:

Bonds
=====

.. image:: /images/data_inspector/bonds_page.*
  :width: 50%
  :align: right

This page of the :ref:`data inspector <data_inspector>` provides a comprehensive view of :ref:`bond <scene_objects.bonds>` data
in the current dataset. The page displays all bonds and their associated properties in a tabular format and includes
interactive tools for bond inspection and filtering. This tab appears only when the current dataset contains bonds.
You can apply the :ref:`particles.modifiers.create_bonds` modifier in the data pipeline to generate bonds if necessary.

Interface elements
------------------

The bonds tab includes the following elements:

1. **Filter expression field** (top center): Allows dynamic filtering of the bond list using Boolean expressions.
   The expression syntax is identical to that used by the :ref:`Expression selection <particles.modifiers.expression_select>` modifier.
   The field provides auto-completion suggestions for available bond and particle properties.

2. **Toolbar** (top left):

   * :guilabel:`Select in viewports` button (crosshair icon): Activates an interactive bond picking mode that allows
     you to select bonds directly in the :ref:`viewport windows <usage.viewports>`. Click on bonds to select them individually, or
     hold down the :kbd:`Ctrl` key (:kbd:`Command` key on macOS) to select multiple bonds. When bonds are
     selected, the filter expression is automatically updated to display only the selected bonds using their
     bond indices. Click the button again or right-click in a viewport to deactivate the selection mode.

3. **Bond count display** (top right): Shows the number of bonds currently displayed in the table after
   applying any filter expressions.

The displayed data always reflects the state of the bonds at the end of the current :ref:`data pipeline <usage.modification_pipeline>`,
i.e., after all modifiers have been applied to the imported simulation data.

Filtering expressions
---------------------

Filter expressions can reference both bond properties and properties of the connected particles.
To access particle properties, use the ``@1.`` and ``@2.`` prefixes to refer to properties of the
two particles connected by the bond:

**Example expressions:**

To show all bonds connected to the first particle (index 0):

.. code-block:: none

  @1.ParticleIndex==0 || @2.ParticleIndex==0

or, equivalently, referring directly to the ``Topology`` property of the bonds:

.. code-block:: none

  Topology.A==0 || Topology.B==0

To list only bonds connecting different particle types:

.. code-block:: none

  @1.ParticleType != @2.ParticleType

To select bonds connecting hydrogen to carbon atoms:

.. code-block:: none

  (@1.ParticleType == "H" && @2.ParticleType == "C") ||
  (@2.ParticleType == "H" && @1.ParticleType == "C")

Note that bond directionality is arbitrary, so ``@1`` and ``@2`` can refer to either particle.
Complex expressions may need to account for both possibilities, as shown in the last example above.

.. seealso:: :ref:`particles.modifiers.expression_select.bonds`