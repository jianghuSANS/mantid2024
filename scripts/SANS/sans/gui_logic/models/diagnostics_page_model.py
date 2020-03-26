# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
from mantid import AnalysisDataService
from mantid.api import AlgorithmPropertyWithValue
from mantid.simpleapi import SumSpectra, ConvertAxesToRealSpace
from sans.algorithm_detail.batch_execution import provide_loaded_data, create_unmanaged_algorithm, add_to_group
from sans.algorithm_detail.crop_helper import get_component_name
from sans.algorithm_detail.mask_sans_workspace import mask_workspace
from sans.common.constants import EMPTY_NAME
from sans.common.enums import IntegralEnum, DetectorType, SANSDataType
from sans.common.file_information import get_instrument_paths_for_sans_file
from sans.common.general_functions import parse_diagnostic_settings
from sans.common.xml_parsing import get_named_elements_from_ipf_file
from sans.gui_logic.models.RowEntries import RowEntries
from sans.gui_logic.plotting import get_plotting_module
from sans.gui_logic.presenter.gui_state_director import GuiStateDirector


def run_integral(integral_ranges, mask, integral, detector, state):
    ranges = parse_range(integral_ranges)
    input_workspaces = load_workspace(state)

    is_multi_range = len (ranges) > 1

    output_workspaces = []
    for input_workspace in input_workspaces:
        input_workspace_name = input_workspace.name()
        if is_multi_range:
            AnalysisDataService.remove(input_workspace_name + '_ranges')
        input_workspace = crop_workspace(detector.value, input_workspace)

        if mask:
            input_workspace = apply_mask(state, input_workspace, detector.value)

        x_dim, y_dim = get_detector_size_from_sans_file(state, detector)

        output_workspace = integrate_ranges(ranges, integral, mask, detector, input_workspace_name, input_workspace, x_dim, y_dim,
                                            is_multi_range)
        plot_graph(output_workspace)

        output_workspaces.append(output_workspace)

    return output_workspaces


def integrate_ranges(ranges, integral, mask, detector, input_workspace_name, input_workspace, x_dim, y_dim, is_multi_range):
    for integral_range in ranges:
        output_workspace = generate_output_workspace_name(integral_range, integral, mask, detector, input_workspace_name)
        output_workspace = run_algorithm(input_workspace, integral_range, integral, output_workspace, x_dim, y_dim)

        if is_multi_range:
            add_to_group(output_workspace, input_workspace_name + '_ranges')

    if is_multi_range:
        return AnalysisDataService.retrieve(input_workspace_name + '_ranges')
    else:
        return output_workspace


def parse_range(range):
    if range:
        return parse_diagnostic_settings(range)
    else:
        return [[0, AlgorithmPropertyWithValue.EMPTY_INT]]


def load_workspace(state):
    workspace_to_name = {SANSDataType.SAMPLE_SCATTER: "SampleScatterWorkspace",
                         SANSDataType.SAMPLE_TRANSMISSION: "SampleTransmissionWorkspace",
                         SANSDataType.SAMPLE_DIRECT: "SampleDirectWorkspace",
                         SANSDataType.CAN_SCATTER: "CanScatterWorkspace",
                         SANSDataType.CAN_TRANSMISSION: "CanTransmissionWorkspace",
                         SANSDataType.CAN_DIRECT: "CanDirectWorkspace"}

    workspace_to_monitor = {SANSDataType.SAMPLE_SCATTER: "SampleScatterMonitorWorkspace",
                            SANSDataType.CAN_SCATTER: "CanScatterMonitorWorkspace"}

    workspaces, monitors = provide_loaded_data(state, False, workspace_to_name, workspace_to_monitor)

    return workspaces[SANSDataType.SAMPLE_SCATTER]


def crop_workspace(component, workspace):
    crop_name = "CropToComponent"
    component_to_crop = DetectorType(component)
    component_to_crop = get_component_name(workspace, component_to_crop)
    crop_options = {"InputWorkspace": workspace,
                    "OutputWorkspace": EMPTY_NAME,
                    "ComponentNames": component_to_crop}

    crop_alg = create_unmanaged_algorithm(crop_name, **crop_options)
    crop_alg.execute()
    output_workspace = crop_alg.getProperty("OutputWorkspace").value

    return output_workspace


def run_algorithm(input_workspace, range, integral, output_workspace, x_dim, y_dim):
    hv_min = range[0]
    hv_max = range[1]

    if integral == IntegralEnum.Horizontal:
        output_workspace = ConvertAxesToRealSpace(InputWorkspace=input_workspace, OutputWorkspace=output_workspace, VerticalAxis='x',
                                                  HorizontalAxis='y', NumberVerticalBins=int(x_dim), NumberHorizontalBins=int(y_dim))
        output_workspace = SumSpectra(InputWorkspace=output_workspace, OutputWorkspace=output_workspace, StartWorkspaceIndex=hv_min,
                                      EndWorkspaceIndex=hv_max)
    elif integral == IntegralEnum.Vertical:
        output_workspace = ConvertAxesToRealSpace(InputWorkspace=input_workspace, OutputWorkspace=output_workspace, VerticalAxis='y',
                                                  HorizontalAxis='x', NumberVerticalBins=int(x_dim), NumberHorizontalBins=int(y_dim))
        output_workspace = SumSpectra(InputWorkspace=output_workspace, OutputWorkspace=output_workspace, StartWorkspaceIndex=hv_min,
                                      EndWorkspaceIndex=hv_max)
    elif integral == IntegralEnum.Time:
        output_workspace = SumSpectra(InputWorkspace=input_workspace, OutputWorkspace=output_workspace,
                                      StartWorkspaceIndex=hv_min, EndWorkspaceIndex=hv_max)

    return output_workspace


def generate_output_workspace_name(range, integral, mask, detector, input_workspace_name):
    integral_string = integral.value
    detector_string = detector.value

    return 'Run:{}, Range:{}, Direction:{}, Detector:{}, Mask:{}'.format(input_workspace_name, range,
                                                                         integral_string,
                                                                         detector_string, mask)


def plot_graph(workspace):
    plotting_module = get_plotting_module()
    if hasattr(plotting_module, 'plotSpectrum'):
        return plotting_module.plotSpectrum(workspace, 0)
    elif hasattr(plotting_module, 'plot'):
        if not isinstance(workspace, list):
            workspace = [workspace]
        plotting_module.plot(workspace, wksp_indices=[0])


def apply_mask(state, workspace, component):
    output_ws = mask_workspace(component_as_string=component, workspace=workspace, state=state)
    return output_ws


def get_detector_size_from_sans_file(state, detector):
    instrument_file = get_instrument_paths_for_sans_file(state.data.sample_scatter)

    if detector == DetectorType.HAB:
        x_dim = get_named_elements_from_ipf_file(instrument_file[1], "high-angle-detector-num-columns",
                                                 float)['high-angle-detector-num-columns']
        y_dim = get_named_elements_from_ipf_file(instrument_file[1], "high-angle-detector-num-rows",
                                                 float)['high-angle-detector-num-rows']
    else:
        x_dim = get_named_elements_from_ipf_file(instrument_file[1], "low-angle-detector-num-columns", float)[
                                                 'low-angle-detector-num-columns']
        y_dim = get_named_elements_from_ipf_file(instrument_file[1], "low-angle-detector-num-rows", float)[
                                                 'low-angle-detector-num-rows']

    return x_dim, y_dim


def create_state(state_model_with_view_update, file, period, facility):
    table_row = RowEntries(sample_scatter=file, sample_scatter_period=period)
    gui_state_director = GuiStateDirector(state_model_with_view_update, facility)

    state = gui_state_director.create_state(table_row)

    return state
