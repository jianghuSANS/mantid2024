"""
api
===

Defines Python objects that wrap the C++ API namespace.

"""

###############################################################################
# The _api C extension depends on exports defined in the _kernel extension
###############################################################################
# The fully-qualified package path allows it to be found with path manipulation
from mantid.kernel import dlopen as _pydlopen
import os as _os
clib = _os.path.join(_os.path.dirname(__file__), '_api.so')
flags = _pydlopen.setup_dlopen(clib, ['libMantidKernel', 'libMantidGeometry', 'libMantidAPI']) # Ensure the library is open with the correct flags
from mantid.kernel import _kernel
from _api import *
_pydlopen.restore_flags(flags)
###############################################################################

###############################################################################
# Make aliases accessible in this namespace
###############################################################################
from _aliases import *

###############################################################################
# Add importAll member to ADS 
###############################################################################
import _adsimports

###############################################################################
# Attach operators to workspaces 
###############################################################################
import _workspaceops
