# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +
"""A collection of functions to share creating models for tests
"""
# std imports
from unittest.mock import MagicMock, create_autospec

# 3rd party imports
from mantid.api import SpecialCoordinateSystem
from mantid.dataobjects import PeaksWorkspace

# local imports
from mantidqt.widgets.sliceviewer.peaksviewer.model import PeaksViewerModel
from mantidqt.widgets.sliceviewer.peaksviewer.representation.painter import MplPainter


def draw_peaks(centers, fg_color, slice_value, slice_width, frame=SpecialCoordinateSystem.QLab):
    model = create_peaks_viewer_model(centers, fg_color)
    slice_info = create_slice_info(centers, slice_value, slice_width, frame)
    mock_painter = MagicMock(spec=MplPainter)
    mock_painter._axes = MagicMock()

    model.draw_peaks(slice_info, mock_painter)

    return model, mock_painter


def create_peaks_viewer_model(centers, fg_color, name=None):
    peaks = [create_mock_peak(center) for center in centers]

    def get_peak(index):
        return peaks[index]

    model = PeaksViewerModel(create_autospec(PeaksWorkspace), fg_color, 'unused')
    if name is not None:
        model.ws.name.return_value = name
    model.ws.__iter__.return_value = peaks
    model.ws.getPeak.side_effect = get_peak
    return model


def create_mock_peak(center):
    peak = MagicMock()
    # set all 3 methods to return the same thing. Check appropriate method called in test
    peak.getQLabFrame.return_value = center
    peak.getQSampleFrame.return_value = center
    peak.getHKL.return_value = center
    shape = MagicMock()
    shape.shapeName.return_value = 'none'
    peak.getPeakShape.return_value = shape
    return peak


def create_slice_info(transform_side_effect,
                      slice_value,
                      slice_width,
                      frame=SpecialCoordinateSystem.QLab):
    slice_info = MagicMock()
    slice_info.frame = frame
    slice_info.transform.side_effect = transform_side_effect
    slice_info.value = slice_value
    slice_info.width = slice_width
    return slice_info
