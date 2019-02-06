# -*- coding: utf-8 -*-
# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory UKRI,
#     NScD Oak Ridge National Laboratory, European Spallation Source
#     & Institut Laue - Langevin
# SPDX - License - Identifier: GPL - 3.0 +

from __future__ import (absolute_import, division, print_function)

from mantid.api import (AlgorithmFactory, AnalysisDataService, DataProcessorAlgorithm,
                        PropertyMode, WorkspaceGroup, WorkspaceProperty)

from mantid.simpleapi import (AddSampleLog, LoadEventNexus, LoadISISNexus, Plus)

from mantid.kernel import (CompositeValidator, Direction, IntBoundedValidator,
                           Property, StringArrayLengthValidator,
                           StringArrayMandatoryValidator, StringArrayProperty)


class Prop:
    RUNS = 'InputRunList'
    FIRST_TRANS_RUNS = 'FirstTransmissionRunList'
    SECOND_TRANS_RUNS = 'SecondTransmissionRunList'
    SLICE = 'SliceWorkspace'
    NUMBER_OF_SLICES = 'NumberOfSlices'
    OUTPUT_WS='OutputWorkspace'
    OUTPUT_WS_BINNED='OutputWorkspaceBinned'
    OUTPUT_WS_LAM='OutputWorkspaceWavelength'


class ReflectometryISISLoadAndProcess(DataProcessorAlgorithm):

    def __init__(self):
        """Initialize an instance of the algorithm."""
        DataProcessorAlgorithm.__init__(self)
        self._tofPrefix = "TOF_"
        self._transPrefix = "TRANS_"

    def category(self):
        """Return the categories of the algrithm."""
        return 'ISIS\\Reflectometry;Workflow\\Reflectometry'

    def name(self):
        """Return the name of the algorithm."""
        return 'ReflectometryISISLoadAndProcess'

    def summary(self):
        """Return a summary of the algorithm."""
        return "Reduce ISIS reflectometry data, including optional loading and summing/slicing of the input runs."

    def seeAlso(self):
        """Return a list of related algorithm names."""
        return ['ReflectometrySliceEventWorkspace', 'ReflectometryReductionOneAuto']

    def version(self):
        """Return the version of the algorithm."""
        return 1

    def PyInit(self):
        """Initialize the input and output properties of the algorithm."""
        mandatoryInputRuns = CompositeValidator()
        mandatoryInputRuns.add(StringArrayMandatoryValidator())
        lenValidator = StringArrayLengthValidator()
        lenValidator.setLengthMin(1)
        mandatoryInputRuns.add(lenValidator)
        self.declareProperty(StringArrayProperty(Prop.RUNS,
                                                 values=[],
                                                 validator=mandatoryInputRuns),
                             doc='A list of run numbers or workspace names for the input runs. '
                             'Multiple runs will be summed before reduction.')
        self.declareProperty(StringArrayProperty(Prop.FIRST_TRANS_RUNS,
                                                 values=[]),
                             doc='A list of run numbers or workspace names for the first transmission run. '
                             'Multiple runs will be summed before reduction.')
        self.declareProperty(StringArrayProperty(Prop.SECOND_TRANS_RUNS,
                                                 values=[]),
                             doc='A list of run numbers or workspace names for the second transmission run. '
                             'Multiple runs will be summed before reduction.')
        self.declareProperty(WorkspaceProperty(Prop.OUTPUT_WS, '',
                                               optional=PropertyMode.Optional,
                                               direction=Direction.Output),
                             doc='The output workspace, or workspace group if sliced.')
        self.declareProperty(WorkspaceProperty(Prop.OUTPUT_WS_BINNED, '',
                                               optional=PropertyMode.Optional,
                                               direction=Direction.Output),
                             doc='The binned output workspace, or workspace group if sliced.')
        self.declareProperty(WorkspaceProperty(Prop.OUTPUT_WS_LAM, '',
                                               optional=PropertyMode.Optional,
                                               direction=Direction.Output),
                             doc='The output workspace in wavelength, or workspace group if sliced.')
        self.declareProperty('SliceWorkspace', False, doc = 'If true, slice the input workspace')
        self._declareSliceAlgorithmProperties()
        self._declareReductionAlgorithmProperties()

    def PyExec(self):
        """Execute the algorithm."""
        # Convert run numbers to real workspaces
        inputRuns = self.getProperty(Prop.RUNS).value
        inputWorkspaces = self._getInputWorkspaces(inputRuns, False)
        firstTransRuns = self.getProperty(Prop.FIRST_TRANS_RUNS).value
        firstTransWorkspaces = self._getInputWorkspaces(firstTransRuns, True)
        secondTransRuns = self.getProperty(Prop.SECOND_TRANS_RUNS).value
        secondTransWorkspaces = self._getInputWorkspaces(secondTransRuns, True)
        # Combine multiple input runs, if required
        input_workspace = self._sumWorkspaces(inputRuns, inputWorkspaces, False)
        first_trans_workspace = self._sumWorkspaces(firstTransRuns, firstTransWorkspaces, True)
        second_trans_workspace = self._sumWorkspaces(secondTransRuns, secondTransWorkspaces, True)
        # Slice the input workspace, if required
        input_workspace = self._sliceWorkspace(input_workspace)
        # Perform the reduction
        alg = self._reduce(input_workspace, first_trans_workspace, second_trans_workspace)
        self._finalize(alg)

    def validateInputs(self):
        """Return a dictionary containing issues found in properties."""
        issues = dict()
        if len(self.getProperty(Prop.RUNS).value) > 1 and self.getProperty(Prop.SLICE).value:
            issues[Prop.SLICE] = "Cannot perform slicing when summing multiple input runs"
        return issues

    def _declareSliceAlgorithmProperties(self):
        """Copy properties from the child slicing algorithm and add our own custom ones"""
        self._slice_properties = [
            'TimeInterval', 'LogName', 'LogValueInterval']
        self.copyProperties('ReflectometrySliceEventWorkspace', self._slice_properties)
        self.declareProperty(name=Prop.NUMBER_OF_SLICES,
                             defaultValue=Property.EMPTY_INT,
                             validator=IntBoundedValidator(lower=1),
                             direction=Direction.Input,
                             doc='The number of uniform-length slices to slice the input workspace into')

    def _declareReductionAlgorithmProperties(self):
        """Copy properties from the child reduction algorithm"""
        self._reduction_properties = [
            'SummationType', 'ReductionType', 'IncludePartialBins',
            'AnalysisMode', 'ProcessingInstructions', 'ThetaIn', 'ThetaLogName', 'CorrectDetectors',
            'DetectorCorrectionType', 'WavelengthMin', 'WavelengthMax', 'I0MonitorIndex',
            'MonitorBackgroundWavelengthMin', 'MonitorBackgroundWavelengthMax',
            'MonitorIntegrationWavelengthMin', 'MonitorIntegrationWavelengthMax',
            'NormalizeByIntegratedMonitors', 'Params', 'StartOverlap', 'EndOverlap',
            'CorrectionAlgorithm', 'Polynomial', 'C0', 'C1',
            'MomentumTransferMin', 'MomentumTransferStep', 'MomentumTransferMax',
            'PolarizationAnalysis', 'CPp', 'CAp', 'CRho', 'CAlpha', 'FloodCorrection',
            'FloodWorkspace', 'Debug']
        self.copyProperties('ReflectometryReductionOneAuto', self._reduction_properties)

    def _getInputWorkspaces(self, runs, isTrans):
        """Convert the given run numbers into real workspace names. Uses workspaces from
        the ADS if they exist, or loads them otherwise."""
        workspaces = list()
        for run in runs:
            ws = self._getRunFromADSOrNone(run, isTrans)
            if not ws:
                ws = self._loadRun(run, isTrans)
            if not ws:
                raise RuntimeError('Error loading run ' + run)
            workspaces.append(ws)
        return workspaces

    def _prefixedRunName(self, run, isTrans):
        """Add a prefix for TOF workspaces onto the given run name"""
        if isTrans:
            return self._transPrefix + run
        else:
            return self._tofPrefix + run

    def _workspaceExists(self, workspace_name):
        """Return true if the given workspace exists in the ADS
        and is a valid reflectometry workspace for the requested reduction."""
        if AnalysisDataService.doesExist(workspace_name):
            self.log().information('Workspace ' + workspace_name + ' exists')
            return True
        else:
            self.log().information('Workspace ' + workspace_name + ' does not exist')
            return False

    def _inputWorkspaceIsValid(self, workspace_name):
        """Return true if the given workspace exists in the ADS and is
        a valid reflectometry workspace for the requested reduction."""
        workspace = AnalysisDataService.retrieve(workspace_name)
        if self._slicingEnabled():
            # If slicing, check that it's an event workspace
            if workspace.id() != "EventWorkspace":
                self.log().information('Workspace ' + workspace_name +
                                       ' exists but is not an event workspace')
                return False
            # Check that the monitors workspace is also loaded
            if not AnalysisDataService.doesExist(workspace_name + '_monitors'):
                self.log().information('Workspace ' + workspace_name + ' exists but ' +
                                       workspace_name + '_monitors does not')
                return False
        else:
            # If not slicing, check that it's a Workspace2D
            if workspace.id() != "Workspace2D":
                self.log().information('Workspace ' + workspace_name +
                                       ' exists but is not a Workspace2D')
        return True

    def _workspaceExistsAndIsValid(self, workspace_name, isTrans):
        """Return true if the given workspace exists in the ADS and is valid"""
        if not self._workspaceExists(workspace_name):
            return False
        # No further validation is currently required for transmission runs
        if isTrans:
            return True
        # Do validation for input runs
        return self._inputWorkspaceIsValid(workspace_name)

    def _getRunFromADSOrNone(self, run, isTrans):
        """Given a run name, return the name of the equivalent workspace in the ADS (
        which may or may not have a prefix applied). Returns None if the workspace does
        not exist or is not valid for the requested reflectometry reduction."""
        # Try given run number
        workspace_name = run
        if self._workspaceExistsAndIsValid(workspace_name, isTrans):
            return workspace_name
        # Try with prefix
        workspace_name = self._prefixedRunName(run, isTrans)
        if self._workspaceExistsAndIsValid(workspace_name, isTrans):
            return workspace_name
        # Not found
        return None

    def _loadRun(self, run, isTrans):
        """Load a run as an event workspace if slicing is requested, or a non-event
        workspace otherwise"""
        workspace_name=self._prefixedRunName(run, isTrans)
        if self._slicingEnabled():
            LoadEventNexus(Filename=run, OutputWorkspace=workspace_name, LoadMonitors=True)
            _throwIfNotValidReflectometryEventWorkspace(workspace_name)
            self.log().information('Loaded event workspace ' + workspace_name)
        else:
            LoadISISNexus(Filename=run, OutputWorkspace=workspace_name)
            self.log().information('Loaded workspace ' + workspace_name)
        return workspace_name

    def _sumWorkspaces(self, runs, workspaces, isTrans):
        """If there are multiple input workspaces, sum them and return the result. Otherwise
        just return the single input workspace, or None if the list is empty."""
        if len(workspaces) < 1:
            return None
        if len(workspaces) < 2:
            return workspaces[0]
        concatenated_names = "+".join(runs)
        summed = self._prefixedRunName(concatenated_names, isTrans)
        self.log().information('Summing workspaces' + " ".join(workspaces) + ' into ' + summed)
        lhs = workspaces[0]
        for rhs in workspaces[1:]:
            Plus(LHSWorkspace=lhs, RHSWorkspace=rhs, OutputWorkspace = summed)
            lhs = summed
        # The reduction algorithm sets the output workspace names from the run number,
        # which by default is just the first run. Set it to the concatenated name,
        # e.g. 13461+13462
        _setRunNumberForWorkspace(summed, concatenated_names)
        return summed

    def _slicingEnabled(self):
        return self.getProperty(Prop.SLICE).value

    def _setUniformNumberOfSlices(self, alg, workspace_name):
        """If slicing by a specified number of slices is requested, find the time
        interval to use to give this number of even time slices and set the relevant
        property on the given slicing algorithm"""
        if self.getProperty(Prop.NUMBER_OF_SLICES).isDefault:
            return
        number_of_slices = self.getProperty(Prop.NUMBER_OF_SLICES).value
        run=AnalysisDataService.retrieve(workspace_name).run()
        total_duration = (run.endTime() - run.startTime()).total_seconds()
        slice_duration = total_duration / number_of_slices
        alg.setProperty("TimeInterval", slice_duration)

    def _setSliceStartStopTimes(self, alg, workspace_name):
        """Set the start/stop time for the slicing algorithm based on the
        run start/end times if the time interval is specified, otherwise
        we can end up with more slices than we expect"""
        if alg.getProperty("TimeInterval").isDefault:
            return
        run=AnalysisDataService.retrieve(workspace_name).run()
        alg.setProperty("StartTime", str(run.startTime()))
        alg.setProperty("StopTime", str(run.endTime()))

    def _runSliceAlgorithm(self, input_workspace, output_workspace):
        """Run the child algorithm to perform the slicing"""
        self.log().information('Running ReflectometrySliceEventWorkspace')
        alg = self.createChildAlgorithm("ReflectometrySliceEventWorkspace")
        for property in self._slice_properties:
            alg.setProperty(property, self.getPropertyValue(property))
        alg.setProperty("OutputWorkspace", output_workspace)
        alg.setProperty("InputWorkspace", input_workspace)
        alg.setProperty("MonitorWorkspace", _monitorWorkspace(input_workspace))
        self._setUniformNumberOfSlices(alg, input_workspace)
        self._setSliceStartStopTimes(alg, input_workspace)
        alg.execute()

    def _sliceWorkspace(self, workspace):
        """If slicing has been requested, slice the input workspace, otherwise
        return it unchanged"""
        if not self._slicingEnabled():
            return workspace
        sliced_workspace_name = self._getSlicedWorkspaceGroupName(workspace)
        self.log().information('Slicing workspace ' + workspace + ' into ' + sliced_workspace_name)
        self._runSliceAlgorithm(workspace, sliced_workspace_name)
        return sliced_workspace_name

    def _getSlicedWorkspaceGroupName(self, workspace):
        return workspace + '_sliced'

    def _setChildAlgorithmPropertyIfProvided(self, alg, property_name):
        """Set the given property on the given algorithm if it is set in our
        inputs. Leave it unset otherwise."""
        if not self.getProperty(property_name).isDefault:
            alg.setProperty(property_name, self.getPropertyValue(property_name))

    def _reduce(self, input_workspace, first_trans_workspace, second_trans_workspace):
        """Run the child algorithm to do the reduction. Return the child algorithm."""
        self.log().information('Running ReflectometryReductionOneAuto on ' + input_workspace)
        alg = self.createChildAlgorithm("ReflectometryReductionOneAuto")
        for property in self._reduction_properties:
            alg.setProperty(property, self.getPropertyValue(property))
        self._setChildAlgorithmPropertyIfProvided(alg, Prop.OUTPUT_WS)
        self._setChildAlgorithmPropertyIfProvided(alg, Prop.OUTPUT_WS_BINNED)
        self._setChildAlgorithmPropertyIfProvided(alg, Prop.OUTPUT_WS_LAM)
        alg.setProperty("InputWorkspace", input_workspace)
        alg.setProperty("FirstTransmissionRun", first_trans_workspace)
        alg.setProperty("SecondTransmissionRun", second_trans_workspace)
        alg.execute()
        return alg

    def _removePrefix(self, workspace):
        """Remove the TOF prefix from the given workspace name"""
        prefix_len = len(self._tofPrefix)
        name_start = workspace[:prefix_len]
        if len(workspace) > prefix_len and name_start == self._tofPrefix:
            return workspace[len(self._tofPrefix):]
        else:
            return workspace

    def _finalize(self, child_alg):
        """Set our output properties from the results in the given child algorithm"""
        self._setOutputWorkspace(Prop.OUTPUT_WS, child_alg)
        self._setOutputWorkspace(Prop.OUTPUT_WS_BINNED, child_alg)
        self._setOutputWorkspace(Prop.OUTPUT_WS_LAM, child_alg)

    def _setOutputWorkspace(self, property_name, child_alg):
        """Set the given output property from the result in the given child algorithm,
        if it exists"""
        value = child_alg.getPropertyValue(property_name)
        if value:
            self.setPropertyValue(property_name, value)
            self.setProperty(property_name, child_alg.getProperty(property_name).value)


def _setRunNumberForWorkspace(workspace_name, run_number):
    """Set the run number in the sample log for the given workspace"""
    AddSampleLog(Workspace=workspace_name, LogName='run_number', LogText=str(run_number),
                 LogType='String')


def _throwIfNotValidReflectometryEventWorkspace(workspace_name):
    workspace=AnalysisDataService.retrieve(workspace_name)
    if isinstance(workspace, WorkspaceGroup):
        raise RuntimeError('Slicing workspace groups is not supported')
    if not workspace.run().hasProperty('proton_charge'):
        raise RuntimeError('Cannot slice workspace: run must contain proton_charge')


def _monitorWorkspace(workspace):
    """Return the associated monitor workspace name for the given workspace"""
    return workspace + '_monitors'


AlgorithmFactory.subscribe(ReflectometryISISLoadAndProcess)
