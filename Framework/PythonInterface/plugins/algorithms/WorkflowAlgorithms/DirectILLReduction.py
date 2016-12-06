# -*- coding: utf-8 -*-

import collections
import glob
from mantid.api import AlgorithmFactory, AnalysisDataServiceImpl, DataProcessorAlgorithm, FileAction, FileProperty, ITableWorkspaceProperty, MatrixWorkspaceProperty, mtd, PropertyMode,  WorkspaceProperty
from mantid.kernel import Direct, Direction, IntArrayProperty, StringListValidator, StringMandatoryValidator, UnitConversion
from mantid.simpleapi import AddSampleLog, CalculateFlatBackground,\
                             CloneWorkspace, ComputeCalibrationCoefVan,\
                             ConvertUnits, CorrectKiKf, CreateSingleValuedWorkspace, CreateWorkspace, DeleteWorkspace, DetectorEfficiencyCorUser, Divide, ExtractMonitors, ExtractSpectra, \
                             FindDetectorsOutsideLimits, FindEPP, GetEiMonDet, GroupWorkspaces, Integration, Load,\
                             MaskDetectors, MedianDetectorTest, MergeRuns, Minus, Multiply, NormaliseToMonitor, Plus, Rebin, Scale
import numpy
from os import path

CLEANUP_DELETE = 'DeleteIntermediateWorkspaces'
CLEANUP_KEEP   = 'KeepIntermediateWorkspaces'

DIAGNOSTICS_YES   = 'DiagnoseDetectors'
DIAGNOSTICS_NO    = 'OmitDetectorDiagnostics'

INCIDENT_ENERGY_CALIBRATION_NO  = 'OmitIncidentEnergyCalibration'
INCIDENT_ENERGY_CALIBRATION_YES = 'CalibrateIncidentEnergy'

INDEX_TYPE_DETECTOR_ID     = 'DetectorID'
INDEX_TYPE_SPECTRUM_NUMBER = 'SpectrumNumber'
INDEX_TYPE_WORKSPACE_INDEX = 'WorkspaceIndex'

NORM_METHOD_MONITOR = 'Monitor'
NORM_METHOD_TIME    = 'AcquisitionTime'

PROP_BINNING_Q                        = 'QBinning'
PROP_BINNING_W                        = 'WBinning'
PROP_CD_WORKSPACE                     = 'CadmiumWorkspace'
PROP_CLEANUP_MODE                     = 'Cleanup'
PROP_DIAGNOSTICS_WORKSPACE            = 'DiagnosticsWorkspace'
PROP_DETECTORS_FOR_EI_CALIBRATION     = 'IncidentEnergyCalibrationDetectors'
PROP_DETECTOR_DIAGNOSTICS             = 'Diagnostics'
PROP_EC_WORKSPACE                     = 'EmptyCanWorkspace'
PROP_EPP_WORKSPACE                    = 'EPPWorkspace'
PROP_FLAT_BACKGROUND_SCALING          = 'FlatBackgroundScaling'
PROP_FLAT_BACKGROUND_WINDOW           = 'FlatBackgroundAveragingWindow'
PROP_FLAT_BACKGROUND_WORKSPACE        = 'FlatBackgroundWorkspace'
PROP_INCIDENT_ENERGY_CALIBRATION      = 'IncidentEnergyCalibration'
PROP_INCIDENT_ENERGY_WORKSPACE        = 'IncidentEnergyWorkspace'
PROP_INDEX_TYPE                       = 'IndexType'
PROP_INITIAL_ELASTIC_PEAK_REFERENCE   = 'InitialElasticPeakReference'
PROP_INPUT_FILE                       = 'InputFile'
PROP_INPUT_WORKSPACE                  = 'InputWorkspace'
PROP_MONITOR_EPP_WORKSPACE            = 'MonitorEPPWorkspace'
PROP_MONITOR_INDEX                    = 'Monitor'
PROP_NORMALISATION                    = 'Normalisation'
PROP_OUTPUT_DIAGNOSTICS_WORKSPACE     = 'OutputDiagnosticsWorkspace'
PROP_OUTPUT_DETECTOR_EPP_WORKSPACE             = 'OutputEPPWorkspace'
PROP_OUTPUT_FLAT_BACKGROUND_WORKSPACE = 'OutputFlatBackgroundWorkspace'
PROP_OUTPUT_INCIDENT_ENERGY_WORKSPACE = 'OutputIncidentEnergyWorkspace'
PROP_OUTPUT_MONITOR_EPP_WORKSPACE     = 'OutputMonitorEPPWorkspace'
PROP_OUTPUT_WORKSPACE                 = 'OutputWorkspace'
PROP_REDUCTION_TYPE                   = 'ReductionType'
PROP_TRANSMISSION                     = 'Transmission'
PROP_USER_MASK                        = 'MaskedDetectors'
PROP_VANADIUM_WORKSPACE               = 'VanadiumWorkspace'

REDUCTION_TYPE_CD = 'Empty can/cadmium'
REDUCTION_TYPE_EC = REDUCTION_TYPE_CD
REDUCTION_TYPE_SAMPLE = 'Sample'
REDUCTION_TYPE_VANADIUM = 'Vanadium'

WS_CONTENT_DETECTORS = 0
WS_CONTENT_MONITORS = 1

class IntermediateWsCleanup:
    '''
    Manages intermediate workspace cleanup.
    '''
    def __init__(self, cleanupMode):
        self._doDelete = cleanupMode == CLEANUP_DELETE
        self._protected = set()
        self._toBeDeleted = set()

    def cleanup(self, *args):
        '''
        Deletes the given workspaces.
        '''
        for ws in args:
            self._delete(ws)

    def cleanupLater(self, *args):
        '''
        Marks the given workspaces to be cleaned up later.
        '''
        map(self._toBeDeleted.add, map(str, args))

    def finalCleanup(self):
        '''
        Deletes all workspaces marked to be cleaned up later.
        ''' 
        for ws in self._toBeDeleted:
            if mtd.doesExist(ws):
                DeleteWorkspace(Workspace=ws)

    def protect(self, *args):
        '''
        Marks the given workspaces to be never deleted.
        '''
        map(self._protected.add, map(str, args))

    def _delete(self, ws):
        '''
        Deletes the given workspace if it is not protected, and
        deletion is actually turned on.
        '''
        if not self._doDelete:
            return
        ws = str(ws)
        if not ws in self._protected and mtd.doesExist(ws):
            DeleteWorkspace(Workspace=ws)

class NameSource:

    def __init__(self, prefix, cleanupMode):
        self._names = set()
        self._prefix = prefix
        if cleanupMode == CLEANUP_DELETE:
            self._prefix = '__' + prefix

    def withSuffix(self, suffix):
        return self._prefix + '_' + suffix

def setAsBad(ws, index):
    ws.dataY(index)[0] += 1

def _loadFiles(inputFilename, eppReference, wsNames, wsCleanup, log):
    '''
    Loads files specified by filenames, merging them into a single
    workspace.
    '''
    # TODO Stop this epp reference madness when LoadILLTOF v.2 is available.
    # TODO Explore ways of loading data file-by-file and merging pairwise.
    #      Should save some memory (IN5!).
    rawWSName = wsNames.withSuffix('raw')
    if eppReference:
        if mtd.doesExist(str(eppReference)):
            ws = Load(Filename=inputFilename,
                      OutputWorkspace=rawWSName,
                      WorkspaceVanadium=eppReference)
        else:
            ws = Load(Filename=inputFilename,
                      OutputWorkspace=rawWSName,
                      FilenameVanadium=eppReference)
    else:
        # For multiple files, we need an EPP reference workspace
        # anyway, so we'll use the first file the user wants to
        # load.
        head, tail = path.split(inputFilename)
        # Loop over known MultipleFileProperty file list
        # specifiers.
        for separator in [':', ',', '-', '+']:
            referenceFilename, sep, rest = tail.partition(separator)
            if referenceFilename:
                break;
        if referenceFilename:
            referenceFilename = path.join(head, referenceFilename)
            log.information('Using ' + referenceFilename + ' as initial EPP reference.')
            eppReferenceWSName = wsNames.withSuffix('initial_epp_reference')
            Load(Filename=referenceFilename,
                 OutputWorkspace=eppReferenceWSName)
            ws = Load(Filename=inputFilename,
                      OutputWorkspace=rawWSName,
                      WorkspaceVanadium=eppReferenceWSName)
            wsCleanup.cleanup(eppReferenceWSName)
        else:
            # Single file, no EPP reference.
            ws = Load(Filename=inputFilename,
                      OutputWorkspace=rawWSName)
    # Now merge the loaded files
    mergedWSName = wsNames.withSuffix('merged')
    ws = MergeRuns(InputWorkspaces=ws,
                   OutputWorkspace=mergedWSName)
    wsCleanup.cleanup(rawWSName)
    return ws

def _extractMonitorWs(ws, wsNames):
    '''
    Separates detector and monitor histograms from a workspace.
    '''
    detWSName = wsNames.withSuffix('extracted_detectors')
    monWSName = wsNames.withSuffix('extracted_monitors')
    detectorWS, monitorWS = ExtractMonitors(InputWorkspace=ws,
                                            DetectorWorkspace=detWSName,
                                            MonitorWorkspace=monWSName)
    return detectorWS, monitorWS

def _createFlatBackground(ws, windowWidth, wsNames):
    '''
    Returns a flat background workspace.
    '''
    bkgWSName = wsNames.withSuffix('flat_background')
    bkgWS = CalculateFlatBackground(InputWorkspace=ws,
                                    OutputWorkspace=bkgWSName,
                                    Mode='Moving Average',
                                    OutputMode='Return Background',
                                    SkipMonitors=False,
                                    NullifyNegativeValues=False,
                                    AveragingWindowWidth=windowWidth)
    return bkgWS

def _subtractFlatBackground(ws, bkgWorkspace, bkgScaling, wsNames, wsCleanup):
    '''
    Subtracts a scaled flat background from a workspace.
    '''
    subtractedWSName = wsNames.withSuffix('background_subtracted')
    scaledBkgWSName = wsNames.withSuffix('flat_background_scaled')
    Scale(InputWorkspace = bkgWorkspace,
          OutputWorkspace = scaledBkgWSName,
          Factor = bkgScaling)
    subtractedWS = Minus(LHSWorkspace=ws,
                      RHSWorkspace=scaledBkgWSName,
                      OutputWorkspace=subtractedWSName)
    wsCleanup.cleanup(scaledBkgWSName)
    return subtractedWS

def _findEPP(ws, wsType, wsNames):
    '''
    Returns EPP table for a workspace.
    '''
    if wsType == WS_CONTENT_DETECTORS:
        eppWSName = wsNames.withSuffix('epp_detectors')
    else:
        eppWSName = wsNames.withSuffix('epp_monitors')
    eppWS = FindEPP(InputWorkspace = ws,
                    OutputWorkspace = eppWSName)
    return eppWS

def _diagnoseDetectors(ws, bkgWS, wsNames, wsCleanup):
    '''
    Returns a diagnostics workspace.
    '''
    # 1. Detectors with zero counts.
    zeroCountWSName = wsNames.withSuffix('diagnostics_zero_counts')
    zeroCountDiagnostics, nFailures = FindDetectorsOutsideLimits(InputWorkspace=ws,
                                                                 OutputWorkspace=zeroCountWSName)
    # 2. Detectors with high background.
    noisyBkgWSName = wsNames.withSuffix('diagnostics_noisy_background')
    noisyBkgDiagnostics, nFailures = MedianDetectorTest(InputWorkspace=bkgWS,
                                                        OutputWorkspace=noisyBkgWSName,
                                                        LowThreshold=0.0,
                                                        HighThreshold=10,
                                                        LowOutlier=0.0)
    combinedDiagnosticsWSName = wsNames.withSuffix('diagnostics')
    diagnosticsWS = Plus(LHSWorkspace=zeroCountDiagnostics,
                         RHSWorkspace=noisyBkgDiagnostics,
                         OutputWorkspace=combinedDiagnosticsWSName)
    wsCleanup.cleanup(zeroCountWSName)
    wsCleanup.cleanup(noisyBkgWSName)
    return diagnosticsWS

def _maskDiagnosedDetectors(ws, diagnosticsWS, wsNames):
    '''
    Mask detectors according to diagnostics.
    '''
    maskedWSName = wsNames.withSuffix('diagnostics_applied')
    maskedWS = CloneWorkspace(InputWorkspace=ws,
                              OutputWorkspace=maskedWSName)
    MaskDetectors(Workspace=maskedWS,
                  MaskedWorkspace=diagnosticsWS)
    return maskedWS

def _applyUserMask(ws, mask, indexType, wsNames):
    '''
    Applies mask.
    '''
    maskedWSName = wsNames.withSuffix('masked')
    maskedWorkspace = CloneWorkspace(InputWorkspace = ws,
                                     OutputWorkspace = maskedWSName)
    if indexType == INDEX_TYPE_DETECTOR_ID:
        MaskDetectors(Workspace=maskedWorkspace,
                      DetectorList=mask)
    elif indexType == INDEX_TYPE_SPECTRUM_NUMBER:
        MaskDetectors(Workspace=maskedWorkspace,
                      SpectraList=mask)
    elif indexType == INDEX_TYPE_WORKSPACE_INDEX:
        MaskDetectors(Workspace=maskedWorkspace,
                      WorkspaceIndexList=mask)
    else:
        raise RuntimeError('Unknown ' + PROP_INDEX_TYPE)
    return maskedWorkspace

def _calibratedIncidentEnergy(detWorkspace, detEPPWorkspace, monWorkspace, monEPPWorkspace, indexType, eiCalibrationDets, eiCalibrationMon, wsNames, log):
    '''
    Returns the calibrated incident energy.
    '''
    instrument = detWorkspace.getInstrument().getName()
    eiWorkspace = None
    if instrument in ['IN4', 'IN6']:
        pulseInterval = detWorkspace.getRun().getLogData('pulse_interval').value
        energy = GetEiMonDet(DetectorWorkspace=detWorkspace,
                             DetectorEPPTable=detEPPWorkspace,
                             IndexType=indexType,
                             Detectors=eiCalibrationDets,
                             MonitorWorkspace=monWorkspace,
                             MonitorEppTable=monEPPWorkspace,
                             Monitor=eiCalibrationMon,
                             PulseInterval=pulseInterval)
        eiWSName = wsNames.withSuffix('incident_energy')
        eiWorkspace = CreateSingleValuedWorkspace(OutputWorkspace=eiWSName,
                                                  DataValue=energy)
    else:
        log.error('Instrument ' + instrument + ' not supported for incident energy calibration')
    return eiWorkspace

def _applyIncidentEnergyCalibration(ws, wsType, eiWS, wsNames):
    '''
    Updates incident energy and wavelength in the sample logs.
    '''
    energy = eiWS.readY(0)[0]
    if wsType == WS_CONTENT_DETECTORS:
        calibratedWSName = wsNames.withSuffix('incident_energy_calibrated_detectors')
    elif wsType == WS_CONTENT_MONITORS:
        calibratedWSName = wsNames.withSuffix('incident_energy_calibrated_monitors')
    else:
        raise RuntimeError('Unknown workspace content type')
    calibratedWS = CloneWorkspace(InputWorkspace=ws,
                                  OutputWorkspace=calibratedWSName)
    AddSampleLog(Workspace=calibratedWS,
                 LogName='Ei',
                 LogText=str(energy),
                 LogType='Number',
                 NumberType='Double',
                 LogUnit='meV')
    wavelength = UnitConversion.run('Energy', 'Wavelength', energy, 0, 0, 0, Direct, 5)
    AddSampleLog(Workspace=calibratedWS,
                 Logname='wavelength',
                 LogText=str(wavelength),
                 LogType='Number',
                 NumberType='Double',
                 LogUnit='Ångström')
    return calibratedWS

def _normalizeToMonitor(ws, monWS, monEPPWS, monIndex, wsNames, wsCleanup):
    '''
    Normalizes to monitor counts.
    '''
    normalizedWSName = wsNames.withSuffix('normalized_to_monitor')
    normalizationFactorWsName = wsNames.withSuffix('normalization_factor_monitor')
    eppRow = monEPPWS.row(monIndex)
    sigma = eppRow['Sigma']
    centre = eppRow['PeakCentre']
    begin = centre - 3 * sigma
    end = centre + 3 * sigma
    normalizedWS, normalizationFactorWS = NormaliseToMonitor(InputWorkspace=ws,
                                                             OutputWorkspace=normalizedWSName,
                                                             MonitorWorkspace=monWS,
                                                             MonitorWorkspaceIndex=monIndex,
                                                             IntegrationRangeMin=begin,
                                                             IntegrationRangeMax=end,
                                                             NormFactorWS=normalizationFactorWsName)
    wsCleanup.cleanup(normalizationFactorWS)
    return normalizedWS

def _normalizeToTime(ws, wsNames, wsCleanup):
    '''
    Normalizes to 'actual_time' sample log.
    '''
    normalizedWSName = wsNames.withSuffix('normalized_to_time')
    normalizationFactorWsName = wsNames.withSuffix('normalization_factor_time')
    time = inWs.getLogData('actual_time').value
    normalizationFactorWS = CreateSingleValuedWorkspace(OutputWorkspace=normalizationFactorWsName,
                                                        DataValue = time)
    normalizedWS = Divide(LHSWorkspace=ws,
                          RHSWorkspace=normalizationFactorWS,
                          OutputWorkspace=normalizedWSName)
    wsCleanup.cleanup(normalizationFactorWS)
    return normalizedWS

def _subtractECWithCd(ws, ecWS, cdWS, transmission, wsNames, wsCleanup):
    '''
    Subtracts cadmium corrected emtpy can.
    '''
    # out = (in - Cd) / transmission - (EC - Cd)
    transmissionWSName = wsNames.withSuffix('transmission')
    transmissionWS = CreateSingleValuedWorkspace(OutputWorkspace=transmissionWSName,
                                                 DataValue=transmission)
    cdSubtractedECWSName = wsNames.withSuffix('Cd_subtracted_EC')
    cdSubtractedECWS = Minus(LHSWorkspace=ecWS,
                             RHSWorkspace=cdWS,
                             OutputWorkspace=cdSubtractedECWSName)
    cdSubtractedWSName = wsNames.withSuffix('Cd_subtracted')
    cdSubtractedWS = Minus(LHSWorkspace=ws,
                           RHSWorkspace=cdWS,
                           OutputWorkspace=cdSubtractedWSName)
    correctedCdSubtractedWsName = wsNames.withSuffix('transmission_corrected_Cd_subtracted')
    correctedCdSubtractedWS = Divide(LHSWorkspace=cdSubtractedWS,
                                     RHSWorkspace=transmissionWS,
                                     OutputWorkspace=correctedCdSubtractedWSName)
    ecSubtractedWSName = wsNames.withSuffix('EC_subtracted')
    ecSubtractedWS = Minus(LHSWorkspace=correctedCdSubtractedWS,
                           RHSWorkspace=cdSubtractedECWS,
                           OutputWorkspace=ecSubtractedWSName)
    wsCleanup.cleanup(cdSubtractedECWS,
                      cdSubtractedWS,
                      correctedCdSubtractedWS)
    return ecSubtractedWS


def _subtractEC(ws, ecWS, transmission, wsNames, wsCleanup):
    '''
    Subtracts empty can.
    '''
    # out = in - transmission * EC
    transmissionWSName = wsNames.withSuffix('transmission')
    transmissionWS = CreateSingleValuedWorkspace(OutputWorkspace=transmissionWSName,
                                                 DataValue=transmission)
    correctedECWSName = wsNames.withSuffix('transmission_corrected_EC')
    correctedECWS = Multiply(LHSWorkspace=ecWS,
                             RHSWorkspace=transmissionWS,
                             OutputWorkspace=correctedECWSName)
    ecSubtractedWSName = wsNames.withSuffix('EC_subtracted')
    ecSubtractedWS = Minus(LHSWorkspace=ws,
                           RHSWorkspace=correctedECWS,
                           OutputWorkspace=ecSubtractedWSName)
    wsCleanup.cleanup(transmissionWS)
    wsCleanup.cleanup(correctedECWS)
    return ecSubtractedWS

def _normalizeToVanadium(ws, vanaWS, wsNames):
    '''
    Normalizes to vanadium workspace.
    '''
    vanaNormalizedWSName = wsNames.withSuffix('vanadium_normalized')
    vanaNormalizedWS = Divide(LHSWorkspace=ws,
                              RHSWorkspace=vanaWS,
                              OutputWorkspace=vanaNormalizedWSName)
    return vanaNormalizedWS

def _convertTOFToDeltaE(ws, wsNames):
    '''
    Converts the X units from time-of-flight to energy transfer.
    '''
    energyConvertedWSName = wsNames.withSuffix('energy_converted')
    energyConvertedWS = ConvertUnits(InputWorkspace = ws,
                                     OutputWorkspace = energyConvertedWSName,
                                     Target = 'DeltaE',
                                     EMode = 'Direct')
    return energyConvertedWS

def _rebin(ws, params, wsNames):
    '''
    Rebins a workspace.
    '''
    rebinnedWSName = wsNames.withSuffix('rebinned')
    rebinnedWS = Rebin(InputWorkspace = ws,
                       OutputWorkspace = rebinnedWSName,
                       Params = params)
    return rebinnedWS

def _correctByKiKf(ws, wsNames):
    '''
    Applies the k_i / k_f correction.
    '''
    correctedWSName = wsNames.withSuffix('kikf')
    correctedWS = CorrectKiKf(InputWorkspace = ws,
                              OutputWorkspace = correctedWSName)
    return correctedWS

def _correctByDetectorEfficiency(ws, wsNames):
    correctedWSName = wsNames.withSuffix('detector_efficiency_corrected')
    correctedWS = DetectorEfficiencyCorUser(InputWorkspace = ws,
                                            OutputWorkspace = correctedWSName)
    return correctedWS


class DirectILLReduction(DataProcessorAlgorithm):

    def __init__(self):
        DataProcessorAlgorithm.__init__(self)

    def category(self):
        return 'Workflow\\Inelastic'

    def name(self):
        return 'DirectILLReduction'

    def summary(self):
        return 'Data reduction workflow for the direct geometry time-of-flight spectrometers at ILL'

    def version(self):
        return 1

    def PyExec(self):
        reductionType = self.getProperty(PROP_REDUCTION_TYPE).value
        wsNamePrefix = self.getProperty(PROP_OUTPUT_WORKSPACE).valueAsStr
        cleanupMode = self.getProperty(PROP_CLEANUP_MODE).value
        wsNames = NameSource(wsNamePrefix, cleanupMode)
        wsCleanup = IntermediateWsCleanup(cleanupMode)
        indexType = self.getProperty(PROP_INDEX_TYPE).value

        # The variable 'mainWS' shall hold the current main data
        # throughout the algorithm.

        # Init workspace.
        inputFile = self.getProperty(PROP_INPUT_FILE).value
        if inputFile:
            eppReference = self.getProperty(PROP_INITIAL_ELASTIC_PEAK_REFERENCE).value
            mainWS = _loadFiles(inputFile,
                                   eppReference,
                                   wsNames,
                                   wsCleanup,
                                   self.log())
        elif self.getProperty(PROP_INPUT_WORKSPACE).value:
            mainWS = self.getProperty(PROP_INPUT_WORKSPACE).value

        # Extract monitors to a separate workspace
        detectorWorkspace, monitorWorkspace = _extractMonitorWs(mainWS,
                                                                wsNames)
        monitorIndex = self.getProperty(PROP_MONITOR_INDEX).value
        monitorIndex = self._convertToWorkspaceIndex(monitorIndex,
                                                     monitorWorkspace)
        wsCleanup.cleanup(mainWS)
        mainWS = detectorWorkspace
        del(detectorWorkspace)
        wsCleanup.cleanupLater(monitorWorkspace)

        # Time-independent background
        # ATM monitor background is ignored
        bkgInWS = self.getProperty(PROP_FLAT_BACKGROUND_WORKSPACE).value
        if not bkgInWS:
            windowWidth = self.getProperty(PROP_FLAT_BACKGROUND_WINDOW).value
            bkgWorkspace = _createFlatBackground(mainWS, 
                                                 windowWidth,
                                                 wsNames)
        else:
            bkgWorkspace = bkgInWS
            wsCleanup.protect(bkgWorkspace)
        if not self.getProperty(PROP_OUTPUT_FLAT_BACKGROUND_WORKSPACE).isDefault:
            self.setProperty(PROP_OUTPUT_FLAT_BACKGROUND_WORKSPACE, bkgWorkspace)
        # Subtract the time-independent background.
        bkgScaling = self.getProperty(PROP_FLAT_BACKGROUND_SCALING).value
        bkgSubtractedWorkspace = _subtractFlatBackground(mainWS,
                                                         bkgWorkspace,
                                                         bkgScaling,
                                                         wsNames,
                                                         wsCleanup)
        wsCleanup.cleanup(mainWS)
        mainWS = bkgSubtractedWorkspace
        del(bkgSubtractedWorkspace)
        wsCleanup.cleanupLater(bkgWorkspace)

        # Find elastic peak positions for detectors.
        detectorEPPInWS = self.getProperty(PROP_EPP_WORKSPACE).value
        if not detectorEPPInWS:
            detectorEPPWS = _findEPP(mainWS,
                                     WS_CONTENT_DETECTORS,
                                     wsNames)
        else:
            detectorEPPWS = detectorInWS
            wsCleanup.protect(detectorEPPWS)
        if not self.getProperty(PROP_OUTPUT_DETECTOR_EPP_WORKSPACE).isDefault:
            self.setProperty(PROP_OUTPUT_DETECTOR_EPP_WORKSPACE,
                             detectorEPPWS)
        # Elastic peaks for monitors
        monitorEPPInWS = self.getProperty(PROP_MONITOR_EPP_WORKSPACE).value
        if not monitorEPPInWS:
            monitorEPPWS = _findEPP(mainWS,
                                    WS_CONTENT_MONITORS,
                                    wsNames)
        else:
            monitorEPPWS = monitorInWS
            wsCleanup.protect(monitorEPPWS)
        if not self.getProperty(PROP_OUTPUT_MONITOR_EPP_WORKSPACE).isDefault:
            self.setProperty(PROP_OUTPUT_MONITOR_EPP_WORKSPACE,
                             monitorEPPWS)
        wsCleanup.cleanupLater(detectorEPPWS, monitorEPPWS)

        # Detector diagnostics, if requested.
        if self.getProperty(PROP_DETECTOR_DIAGNOSTICS).value == DIAGNOSTICS_YES:
            diagnosticsInWS = self.getProperty(PROP_DIAGNOSTICS_WORKSPACE).value
            if not diagnosticsInWS:
                diagnosticsWS = _diagnoseDetectors(mainWS, bkgWorkspace, wsNames, wsCleanup)
            else:
                diagnosticsWS = diagnosticsInWS
                wsCleanup.protect(diagnosticsWS)
            if not self.getProperty(PROP_OUTPUT_DIAGNOSTICS_WORKSPACE).isDefault:
                self.setProperty(PROP_OUTPUT_DIAGNOSTICS_WORKSPACE, diagnosticsWS)
            diagnosedWorkspace = _maskDiagnosedDetectors(mainWS,
                                                         diagnosticsWS,
                                                         wsNames)
            wsCleanup.cleanup(diagnosticsWS)
            del(diagnosticsWS)
            wsCleanup.cleanup(mainWS)
            mainWS = diagnosedWorkspace
            del(diagnosedWorkspace)
        # Apply user mask.
        userMask = self.getProperty(PROP_USER_MASK).value
        maskedWorkspace = _applyUserMask(mainWS, 
                                         userMask,
                                         indexType,
                                         wsNames)
        wsCleanup.cleanup(mainWS)
        mainWS = maskedWorkspace
        del(maskedWorkspace)

        # Get calibrated incident energy
        eiCalibration = self.getProperty(PROP_INCIDENT_ENERGY_CALIBRATION).value
        if eiCalibration == INCIDENT_ENERGY_CALIBRATION_YES:
            eiInWS = self.getProperty(PROP_INCIDENT_ENERGY_WORKSPACE).value
            if not eiInWS:
                eiCalibrationDets = self.getProperty(PROP_DETECTORS_FOR_EI_CALIBRATION).value
                eiCalibrationWS = _calibratedIncidentEnergy(mainWS,
                                                            detectorEPPWS,
                                                            monitorWorkspace,
                                                            monitorEPPWS,
                                                            indexType,
                                                            eiCalibrationDets,
                                                            monitorIndex,
                                                            wsNames,
                                                            self.log())
            else:
                eiCalibrationWS = eiInWS
                wsCleanup.protect(eiCalibrationWS)
            if eiCalibrationWS:
                eiCalibratedDetWS = _applyIncidentEnergyCalibration(mainWS,
                                                                    WS_CONTENT_DETECTORS,
                                                                    eiCalibrationWS,
                                                                    wsNames)
                wsCleanup.cleanup(mainWS)
                mainWS = eiCalibratedDetWS
                del(eiCalibratedDetWS)
                eiCalibratedMonWS = _applyIncidentEnergyCalibration(monitorWorkspace,
                                                                    WS_CONTENT_MONITORS,
                                                                    eiCalibrationWS,
                                                                    wsNames)
                wsCleanup.cleanup(monitorWorkspace)
                monitorWorkspace = eiCalibratedMonWS
                del(eiCalibratedMonWS)
            if not self.getProperty(PROP_OUTPUT_INCIDENT_ENERGY_WORKSPACE).isDefault:
                self.setProperty(PROP_OUTPUT_INCIDENT_ENERGY_WORKSPACE, eiCalibrationWS)
            wsCleanup.cleanup(eiCalibrationWS)
            del(eiCalibrationWS)

        # Normalisation to monitor/time
        normalisationMethod = self.getProperty(PROP_NORMALISATION).value
        if normalisationMethod:
            if normalisationMethod == NORM_METHOD_MONITOR:
                normalizedWS = _normalizeToMonitor(mainWS,
                                                   monitorWorkspace,
                                                   monitorEPPWS,
                                                   monitorIndex,
                                                   wsNames,
                                                   wsCleanup)
            elif normalisationMethod == NORM_METHOD_TIME:
                normalizedWS = _normalizeToTime(mainWS,
                                                wsNames,
                                                wsCleanup)
            else:
                raise RuntimeError('Unknonwn normalisation method ' + normalisationMethod)
            wsCleanup.cleanup(mainWS)
            mainWS = normalizedWS
            del(normalizedWS)

        # Reduction for empty can and cadmium ends here.
        if reductionType == REDUCTION_TYPE_CD or reductionType == REDUCTION_TYPE_EC:
            self._finalize(mainWS, wsCleanup)
            return

        # Continuing with vanadium and sample reductions.

        # Empty can subtraction
        ecInWS = self.getProperty(PROP_EC_WORKSPACE).value
        if ecInWS:
            cdInWS = self.getProperty(PROP_CD_WORKSPACE).value
            transmission = self.getProperty(PROP_TRANSMISSION).value
            if cdInWS:
                ecSubtractedWS = _subtractECWithCd(mainWS,
                                                   ecInWS,
                                                   cdInWS,
                                                   transmission,
                                                   wsNames,
                                                   wsCleanup)
            else:
                ecSubtractedWS = _subtractEC(mainWS,
                                             ecInWS,
                                             transmission,
                                             wsNames,
                                             wsCleanup)
            wsCleanup.cleanup(mainWS)
            mainWS = ecSubtractedWS
            del(ecSubtractedWS)

        # Reduction for vanadium ends here.
        if reductionType == REDUCTION_TYPE_VANADIUM:
            # We output an integrated vanadium, ready to be used for
            # normalization.
            outWS = self.getPropertyValue(PROP_OUTPUT_WORKSPACE)
            # TODO For the time being, we may just want to integrate
            # the vanadium data as `ComputeCalibrationCoef` does not do
            # the best possible Debye-Waller correction.
            mainWS = ComputeCalibrationCoefVan(VanadiumWorkspace=mainWS,
                                                  EPPTable=detectorEPPWS,
                                                  OutputWorkspace=outWS)
            self._finalize(mainWS, wsCleanup)
            return

        # Continuing with sample reduction.

        # Vanadium normalization.
        # TODO Absolute normalization.
        vanaWS = self.getProperty(PROP_VANADIUM_WORKSPACE).value
        if vanaWS:
            vanaNormalizedWS = _normalizeToVanadium(mainWS,
                                                    vanaWS,
                                                    wsNames)
            wsCleanup.cleanup(mainWS)
            mainWS = vanaNormalizedWS
            del(vanaNormalizedWS)

        # Convert units from TOF to energy
        energyConvertedWS = _convertTOFToDeltaE(mainWS, wsNames)
        wsCleanup.cleanup(mainWS)
        mainWS = energyConvertedWS
        del(energyConvertedWS)

        # KiKf conversion
        kikfCorrectedWS = _correctByKiKf(mainWS, wsNames)
        wsCleanup.cleanup(mainWS)
        mainWS = kikfCorrectedWS
        del(kikfCorrectedWS)

        # Rebinning
        # TODO automatize binning in w. Do we need rebinning in q as well?
        params = self.getProperty(PROP_BINNING_W).value
        if params:
            rebinnedWS = _rebin(mainWS, params, wsNames)
            wsCleanup.cleanup(mainWS)
            mainWS = rebinnedWS
            del(rebinnedWS)

        # Detector efficiency correction
        efficiencyCorrectedWS = _correctByDetectorEfficiency(mainWS,
                                                             wsNames)
        wsCleanup.cleanup(mainWS)
        mainWS = efficiencyCorrectedWS
        del(efficiencyCorrectedWS)

        # TODO Self-shielding corrections

        self._finalize(mainWS, wsCleanup)

    def PyInit(self):
        # TODO Property validation.
        # Inputs
        self.declareProperty(FileProperty(PROP_INPUT_FILE,
                                          '',
                                          action=FileAction.OptionalLoad,
                                          extensions=['nxs']))
        self.declareProperty(MatrixWorkspaceProperty(PROP_INPUT_WORKSPACE,
                                                     '',
                                                     optional=PropertyMode.Optional,
                                                     direction=Direction.Input))
        self.declareProperty(WorkspaceProperty(PROP_OUTPUT_WORKSPACE,
                             '',
                             direction=Direction.Output),
                             doc='The output of the algorithm')
        self.declareProperty(PROP_REDUCTION_TYPE,
                             REDUCTION_TYPE_SAMPLE,
                             validator=StringListValidator([REDUCTION_TYPE_SAMPLE, REDUCTION_TYPE_VANADIUM, REDUCTION_TYPE_CD, REDUCTION_TYPE_EC]),
                             direction=Direction.Input,
                             doc='Type of the reduction workflow and output')
        self.declareProperty(PROP_CLEANUP_MODE,
                             CLEANUP_DELETE,
                             validator=StringListValidator([CLEANUP_DELETE, CLEANUP_KEEP]),
                             direction=Direction.Input,
                             doc='What to do with intermediate workspaces')
        self.declareProperty(PROP_INITIAL_ELASTIC_PEAK_REFERENCE,
                             '',
                             direction=Direction.Input,
                             doc="Reference file or workspace for initial 'EPP' sample log entry")
        self.declareProperty(MatrixWorkspaceProperty(PROP_VANADIUM_WORKSPACE,
                                                     '',
                                                     Direction.Input,
                                                     PropertyMode.Optional),
                             doc='Reduced vanadium workspace')
        self.declareProperty(MatrixWorkspaceProperty(PROP_EC_WORKSPACE,
                                                     '',
                                                     Direction.Input,
                                                     PropertyMode.Optional),
                             doc='Reduced empty can workspace')
        self.declareProperty(MatrixWorkspaceProperty(PROP_CD_WORKSPACE,
                                                     '',
                                                     Direction.Input,
                                                     PropertyMode.Optional),
                             doc='Reduced cadmium workspace')
        self.declareProperty(ITableWorkspaceProperty(PROP_EPP_WORKSPACE,
                                                     '',
                                                     Direction.Input,
                                                     PropertyMode.Optional),
                             doc='Table workspace containing results from the FindEPP algorithm')
        self.declareProperty(ITableWorkspaceProperty(PROP_MONITOR_EPP_WORKSPACE,
                                                     '',
                                                     Direction.Input,
                                                     PropertyMode.Optional),
                             doc='Table workspace containing results from the FindEPP algorithm for the monitor workspace')
        self.declareProperty(PROP_INDEX_TYPE,
                             INDEX_TYPE_WORKSPACE_INDEX,
                             direction=Direction.Input,
                             doc='Type of numbers in ' + PROP_MONITOR_INDEX + ' and ' + PROP_DETECTORS_FOR_EI_CALIBRATION + ' properties')
        self.declareProperty(PROP_MONITOR_INDEX,
                             0,
                             direction=Direction.Input,
                             doc='Index of the main monitor')
        self.declareProperty(PROP_INCIDENT_ENERGY_CALIBRATION,
                             INCIDENT_ENERGY_CALIBRATION_YES,
                             validator=StringListValidator([INCIDENT_ENERGY_CALIBRATION_YES, INCIDENT_ENERGY_CALIBRATION_YES]),
                             direction=Direction.Input,
                             doc='Enable or disable incident energy calibration on IN4 and IN6')
        self.declareProperty(PROP_DETECTORS_FOR_EI_CALIBRATION,
                             '',
                             direction=Direction.Input,
                             doc='List of detectors used for the incident energy calibration')
        self.declareProperty(MatrixWorkspaceProperty(PROP_INCIDENT_ENERGY_WORKSPACE,
                                                     '',
                                                     Direction.Input,
                                                     PropertyMode.Optional),
                             doc='A single-valued workspace holding the calibrated incident energy')
        self.declareProperty(PROP_FLAT_BACKGROUND_SCALING,
                             1.0,
                             direction=Direction.Input,
                             doc='Flat background scaling constant')
        self.declareProperty(PROP_FLAT_BACKGROUND_WINDOW,
                             30,
                             direction=Direction.Input,
                             doc='Running average window width (in bins) for flat background')
        self.declareProperty(MatrixWorkspaceProperty(PROP_FLAT_BACKGROUND_WORKSPACE,
                                                     '',
                                                     Direction.Input,
                                                     PropertyMode.Optional),
                             doc='Workspace from which to get flat background data')
        self.declareProperty(IntArrayProperty(PROP_USER_MASK,
                                              '',
                                              direction=Direction.Input),
                             doc='List of spectra to mask')
        self.declareProperty(PROP_DETECTOR_DIAGNOSTICS,
                             DIAGNOSTICS_YES,
                             validator=StringListValidator([DIAGNOSTICS_YES, DIAGNOSTICS_NO]),
                             direction=Direction.Input,
                             doc='If true, run detector diagnostics or apply ' + PROP_DIAGNOSTICS_WORKSPACE)
        self.declareProperty(MatrixWorkspaceProperty(PROP_DIAGNOSTICS_WORKSPACE,
                                                     '',
                                                     Direction.Input,
                                                     PropertyMode.Optional),
                             doc='Detector diagnostics workspace obtained from another reduction run.')
        self.declareProperty(PROP_NORMALISATION,
                             NORM_METHOD_MONITOR,
                             validator=StringListValidator([NORM_METHOD_MONITOR, NORM_METHOD_TIME]),
                             direction=Direction.Input,
                             doc='Normalisation method')
        self.declareProperty(PROP_TRANSMISSION,
                             1.0,
                             direction=Direction.Input,
                             doc='Sample transmission for empty can subtraction')
        self.declareProperty(PROP_BINNING_Q,
                             '',
                             direction=Direction.Input,
                             doc='Rebinning in q')
        self.declareProperty(PROP_BINNING_W,
                             '',
                             direction=Direction.Input,
                             doc='Rebinning in w')
        # Rest of the output properties.
        self.declareProperty(ITableWorkspaceProperty(PROP_OUTPUT_DETECTOR_EPP_WORKSPACE,
                             '',
                             direction=Direction.Output,
                             optional=PropertyMode.Optional),
                             doc='Output workspace for elastic peak positions')
        self.declareProperty(WorkspaceProperty(PROP_OUTPUT_INCIDENT_ENERGY_WORKSPACE,
                                               '',
                                               direction=Direction.Output,
                                               optional=PropertyMode.Optional),
                             doc='Output workspace for calibrated inciden energy')
        self.declareProperty(ITableWorkspaceProperty(PROP_OUTPUT_MONITOR_EPP_WORKSPACE,
                             '',
                             direction=Direction.Output,
                             optional=PropertyMode.Optional),
                             doc='Output workspace for elastic peak positions')
        self.declareProperty(WorkspaceProperty(PROP_OUTPUT_FLAT_BACKGROUND_WORKSPACE,
                             '',
                             direction=Direction.Output,
                             optional=PropertyMode.Optional),
                             doc='Output workspace for flat background')
        self.declareProperty(WorkspaceProperty(PROP_OUTPUT_DIAGNOSTICS_WORKSPACE,
                                               '',
                                               direction=Direction.Output,
                                               optional=PropertyMode.Optional),
                             doc='Output workspace for detector diagnostics')

    def validateInputs(self):
        """
        Checks for issues with user input.
        """
        issues = dict()

        fileGiven = not self.getProperty(PROP_INPUT_FILE).isDefault
        wsGiven = not self.getProperty(PROP_INPUT_WORKSPACE).isDefault
        # Validate an input exists
        if fileGiven == wsGiven:
            issues[PROP_INPUT_FILE] = 'Must give either an input file or an input workspace.'

        return issues

    def _convertToWorkspaceIndex(self, i, ws):
        indexType = self.getProperty(PROP_INDEX_TYPE).value
        if indexType == INDEX_TYPE_WORKSPACE_INDEX:
            return i
        elif indexType == INDEX_TYPE_SPECTRUM_NUMBER:
            return ws.getIndexFromSpectrumNumber(i)
        else: # INDEX_TYPE_DETECTOR_ID
            for j in range(ws.getNumberHistograms()):
                if ws.getSpectrum(j).hasDetectorID(i):
                    return j
            raise RuntimeError('No workspace index found for detector id {0}'.format(i))

    def _finalize(self, outWS, wsCleanup):
        self.setProperty(PROP_OUTPUT_WORKSPACE, outWS)
        wsCleanup.finalCleanup()

AlgorithmFactory.subscribe(DirectILLReduction)
