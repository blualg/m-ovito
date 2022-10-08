.. _viewport_layers.text_label:

Text label layer
----------------

.. image:: /images/viewport_layers/text_label_overlay_panel.*
  :width: 35%
  :align: right

Add this :ref:`viewport layer <viewport_layers>` to a viewport to display text or other information
on top of the three-dimensional scene. The label's text may contain placeholders, which get replaced with the current values
of :ref:`global attributes <usage.global_attributes>` dynamically computed in OVITO's data pipeline.
This makes it possible to include dynamic data such as the current simulation time, the number of atoms,
or numeric results in rendered images or movies.

.. _viewport_layers.text_label.text_formatting:

HTML text formatting
""""""""""""""""""""

You can include HTML markup elements and Cascading Style Sheet (CSS) attributes in the text to format individual words and characters.
OVITO supports a subset of the HTML standard, which is `documented here <https://doc.qt.io/qt-6/richtext-html-subset.html>`__.
The following table gives a few examples of HTML markup elements:

.. |br| raw:: html

   <br/>

.. |underlined| raw:: html

   <span style="text-decoration: underline">underlined</span>

.. role:: raw-html(raw)
   :format: html

.. list-table::
  :widths: 50 50
  :header-rows: 1

  * - Markup text
    - Formatted output
  * - nm\ :raw-html:`<font color="green"><b>`\ <sup>\ :raw-html:`</b></font>`\ 3\ :raw-html:`<font color="green"><b>`\ </sup>\ :raw-html:`</b></font>`
    - nm\ :sup:`3`  
  * - e\ :raw-html:`<font color="green"><b>`\ <sub>\ :raw-html:`</b></font>`\ z\ :raw-html:`<font color="green"><b>`\ </sub>\ :raw-html:`</b></font>`
    - e\ :sub:`z`  
  * - First line\ :raw-html:`<font color="green"><b>`\ <br>\ :raw-html:`</b></font>`\ Second line
    - First line |br| Second line
  * - Some :raw-html:`<font color="green"><b>`\ <i>\ :raw-html:`</b></font>`\ italic\ :raw-html:`<font color="green"><b>`\ </i>\ :raw-html:`</b></font>` text
    - Some *italic* text
  * - Some :raw-html:`<font color="green"><b>`\ <span style="text-decoration: underline">\ :raw-html:`</b></font>`\ underlined\ :raw-html:`<font color="green"><b>`\ </span>\ :raw-html:`</b></font>` text
    - Some |underlined| text

Including computed values in the text
"""""""""""""""""""""""""""""""""""""

The text label has access to the list of :ref:`global attributes <usage.global_attributes>` provided by the 
selected pipeline. Attributes are variables containing numeric values or text strings 
loaded from the simulation file or dynamically computed by modifiers in the data pipeline. 
You can incorporate attribute values in the label's text by inserting placeholders of the form ``[attribute_name]``.

Whenever a placeholder references an attribute with a numeric value, the floating-point value 
gets formatted according to the specified :guilabel:`Value format string`. You have the choice between decimal notation (``%f``), exponential notation (``%e``),
and an automatic mode (``%g``). Furthermore, the format string gives you control of the output precision, i.e., the number of decimal places that
appear after the decimal point. Use ``%.2f``, for example, to always show two digits after the decimal point. 
The format string must follow the standard convention of the `printf() C function <https://en.cppreference.com/w/cpp/io/c/fprintf>`__.

Defining new attributes
"""""""""""""""""""""""

Attributes computed by the data pipeline may not always have the desired 
format, units, or normalization for using them directly in a text label. For instance, 
the ``CommonNeighborAnalysis.counts.BCC`` attribute calculated by the
:ref:`Common neighbor analysis <particles.modifiers.common_neighbor_analysis>` modifier
counts the total number of bcc atoms in the system. But what if you would rather
like to print the *fraction* of bcc atoms, not the absolute count?
In such situations, some kind of conversion and/or transformation of the attribute's value is required,
and you will have to define a new attribute that derives its value from the original attribute. 

.. highlight:: python

This can be accomplished by inserting a :ref:`Python script <particles.modifiers.python_script>` modifier 
into the data pipeline. This modifier executes a simple, user-defined Python function that computes the value of our 
new attribute based on the existing attributes(s)::

  def modify(frame, data):
      bcc_count = data.attributes['CommonNeighborAnalysis.counts.BCC']
      data.attributes['bcc_fraction'] = bcc_count / data.particles.count

In this example, we access the existing attribute ``CommonNeighborAnalysis.counts.BCC`` (the atom count) and 
convert it into a fraction by dividing by the total number of atoms. The new value is output
as a new attribute named ``bcc_fraction``, making it available for inclusion in the text label layer.

.. seealso::
  
  :py:class:`ovito.vis.TextLabelOverlay` (Python API)


