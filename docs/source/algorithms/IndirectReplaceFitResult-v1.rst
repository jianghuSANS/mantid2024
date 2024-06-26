.. algorithm::

.. summary::

.. relatedalgorithms::

.. properties::

Description
-----------

During a sequential fit in Inelastic QENS Fitting, the parameters fitted for a spectrum become the start parameters
for the next spectrum. This can be a problem if the next spectrum is not 'related' to the previous spectrum and will
lead to a poor fit for that spectrum. The ReplaceIndirectFitResult algorithm allows you to replace this poorly fitted
value.

This algorithm takes a *_Result* workspace from a sequential fit for multiple spectra (1), and a *_Result* workspace
for a singly fit spectrum (2) and it will replace the corresponding fit data in workspace (1) with the single fit
data found in workspace (2).

Note that workspaces (1) and (2) should be *_Result* workspaces generated by a fit with the same fit functions and
minimizers. Also note that the output workspace is inserted back into the *_Results* workspace group in which the Input
workspace (1) is found.

Uses the :ref:`CopyDataRange <algm-CopyDataRange>` algorithm to replace the data in workspace (1) with data in workspace (2).

.. categories::

.. sourcelink::
