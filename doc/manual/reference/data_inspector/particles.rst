.. _data_inspector.particles:

Particles
=========

.. image:: /images/data_inspector/particles_page.*
  :width: 50%
  :align: right

This page of the :ref:`data inspector <data_inspector>` provides a comprehensive view of particle data and
interactive tools for particle inspection and geometric measurements. The page consists of a main property table
and optional measurement tables for analyzing spatial relationships between selected particles.

Interface elements
------------------

The particles tab includes the following elements:

1. **Filter expression field** (top center): Allows dynamic filtering of the particle list using Boolean expressions.
   For example, entering ``Coordination==11`` will display only particles with a coordination number of 11, assuming that the :ref:`coordinations numbers have been computed <particles.modifiers.coordination_analysis>` in the pipeline.
   Multiple criteria can be combined using logical *AND* and *OR* operators. The expression syntax is identical
   to that used by the :ref:`Expression selection <particles.modifiers.expression_select>` modifier. The field
   provides auto-completion suggestions for available particle properties.

2. **Toolbar** (top left):

   * :guilabel:`Select in viewports` button (crosshair icon): Activates an interactive particle picking mode that allows
     you to select particles directly in the :ref:`viewport windows <usage.viewports>`. Click on particles to select them individually, or
     hold down the :kbd:`Ctrl` key (:kbd:`Command` key on macOS) to select multiple particles. When particles are
     selected, the filter expression is automatically updated to display only the selected particles. The selection
     is based on the particles' unique identifiers if available, otherwise particle indices are used. Click the button again or
     right-click in a viewport to deactivate the selection mode.

   * :guilabel:`Show distances and angles` button (ruler icon): Toggles the display of measurement tables that show
     geometric relationships between selected particles. When activated, two additional tables appear next to the particle
     property table:

     - **Distance table**: Displays pair-wise distances between selected particles
     - **Angle table**: Shows angles formed by triplets of selected particles

     Additionally, when this mode is active, connection lines are rendered between selected particles in the
     viewport windows to visualize the measured relationships.

3. **Particle count display** (top right): Shows the number of particles currently displayed in the table after
   applying any filter expressions.

Particle property table
-----------------------

The central area displays a table with all particle properties:

* The leftmost column shows particle indices (starting from 0)
* Subsequent columns display various :ref:`particle properties <usage.particle_properties>` such as position,
  type, color, and any custom properties added by modifiers.
* The data being displayed reflects the state of the particles at the end of the current :ref:`data pipeline <usage.modification_pipeline>`.

Distance measurements
---------------------

When the :guilabel:`Show distances and angles` mode is activated and particles are selected (via filter expression or
interactive selection), the distance table displays:

* **Pair A-B**: The indices or identifiers of particle pairs
* **Distance**: The Euclidean distance between each pair of particles
* **Vector**: The displacement vector from particle A to particle B (X, Y, Z components)

Distance calculations are performed for up to the first 4 particles in the filtered list, generating up to 6
pair-wise measurements. Note that periodic boundary conditions are NOT taken into account for these measurements.
If you need to measure distances across periodic boundaries, you should :ref:`create bonds <particles.modifiers.create_bonds>`
between the particles and inspect the bond lengths instead.

Angle measurements
------------------

The angle table displays angles formed by particle triplets:

* **Triplet A-B-C**: The indices or identifiers of three particles, where B is the vertex particle
* **Angle**: The angle in degrees between vectors BA and BC

Angle calculations are performed for up to the first 3 particles in the filtered list, showing all possible
angles with each particle serving as the vertex.
