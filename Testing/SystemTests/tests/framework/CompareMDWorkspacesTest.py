# Mantid Repository : https://github.com/mantidproject/mantid
#
# Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
#   NScD Oak Ridge National Laboratory, European Spallation Source,
#   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
# SPDX - License - Identifier: GPL - 3.0 +
#pylint: disable=no-init,invalid-name
"""
System test for ARCS reduction
"""

import os
import systemtesting
from mantid.simpleapi import *


class CompareMDWorkspacesTest(systemtesting.MantidSystemTest):

    compare_result_1 = ''
    compare_result_2 = ''
    compare_result_3 = ''

    def requiredFiles(self):
        return []

    def requiredMemoryMB(self):
        return 4000

    def cleanup(self):

        for ws_name in ['md_1', 'md_2', 'ev_ws_1', 'ev_ws_2']:
            if mtd.doesExist(ws_name):
                mtd.remove(ws_name)

        return True

    def runTest(self):
        # create saple workspace 1
        ev_ws_1 = CreateSampleWorkspace(WorkspaceType='Event',
                                        Function='Flat background',
                                        XUnit='TOF',
                                        XMin=-9,
                                        XMax=9,
                                        BinWidth=1,
                                        NumEvents=54,
                                        NumBanks=1,
                                        BankPixelWidth=1)

        md_1 = ConvertToMD(InputWorkspace=ev_ws_1,
                           dEAnalysisMode='Elastic',
                           MinValues='-10',
                           MaxValues='10',
                           SplitInto='1',
                           MaxRecursionDepth=1)

        # create saple workspace 1
        ev_ws_2 = ChangeBinOffset(InputWorkspace=ev_ws_1, Offset=0.1)

        md_2 = ConvertToMD(InputWorkspace=ev_ws_2,
                           dEAnalysisMode='Elastic',
                           MinValues='-10',
                           MaxValues='10',
                           SplitInto='1',
                           MaxRecursionDepth=1)
        print(md_2)

        # compare md1 and md2
        self.compare_result_1 = CompareMDWorkspaces(Workspace1='md_1', Workspace2='md_2',
                                                    Tolerance=0.000001, CheckEvents=True,
                                                    IgnoreBoxID=False)

        # merge some MD workspaces
        merged_1 = MergeMD(InputWorkspaces='md_1, md_2', SplitInto=1, MaxRecursionDepth=1)
        merged_2 = MergeMD(InputWorkspaces='md_2, md_1', SplitInto=1, MaxRecursionDepth=1)

        # compare
        self.compare_result_2 = CompareMDWorkspaces(Workspace1=merged_1, Workspace2=merged_2, CheckEvents=True)

        # create merge3
        merged_3 = md_1 + md_1
        # compare
        self.compare_reuslt_3 = CompareMDWorkspaces(Workspace1=merged_1, Workspace2=merged_3, CheckEvents=True,
                                                    Tolerance=1E-7)

    def validate(self):
        self.assertTrue(self.compare_result_1 is not None)
        self.assertTrue(self.compare_result_1.Equals)


class ARCSReductionTest(systemtesting.MantidSystemTest):

    vanFile1=''
    vanFile0=''
    nxspeFile=''

    def requiredFiles(self):
        return ["ARCS_23961_event.nxs","WBARCS.nxs"]

    def requiredMemoryMB(self):
        return 4000

    def cleanup(self):
        if os.path.exists(self.nxspeFile):
            os.remove(self.nxspeFile)
        if os.path.exists(self.vanFile1):
            os.remove(self.vanFile1)
        if os.path.exists(self.vanFile0):
            os.remove(self.vanFile0)
        return True

    def runTest(self):
        self.vanFile1=os.path.join(config.getString('defaultsave.directory'),'ARCSvan_1.nxs')
        self.vanFile0=os.path.join(config.getString('defaultsave.directory'),'ARCSvan_0.nxs')
        self.nxspeFile=os.path.join(config.getString('defaultsave.directory'),'ARCSsystemtest.nxspe')
        config['default.facility']="SNS"
        DgsReduction(   SampleInputFile="ARCS_23961_event.nxs",
                        OutputWorkspace="reduced",
                        IncidentBeamNormalisation="ByCurrent",
                        DetectorVanadiumInputFile="WBARCS.nxs",
                        UseBoundsForDetVan=True,
                        DetVanIntRangeLow=0.35,
                        DetVanIntRangeHigh=0.75,
                        DetVanIntRangeUnits="Wavelength",
                        SaveProcessedDetVan=True,
                        SaveProcDetVanFilename=self.vanFile0)
        DgsReduction(   SampleInputFile="ARCS_23961_event.nxs",
                        OutputWorkspace="reduced",
                        IncidentBeamNormalisation="ByCurrent",
                        DetectorVanadiumInputFile="WBARCS.nxs",
                        UseBoundsForDetVan=True,
                        DetVanIntRangeLow=0.35,
                        DetVanIntRangeHigh=0.75,
                        DetVanIntRangeUnits="Wavelength",
                        MedianTestLevelsUp=1.,
                        SaveProcessedDetVan=True,
                        SaveProcDetVanFilename=self.vanFile1)

        Ei=mtd["reduced"].run().get("Ei").value
        SaveNXSPE(InputWorkspace="reduced",Filename=self.nxspeFile,Efixed=Ei,psi=0,KiOverKfScaling=True)

    def validate(self):
    #test vanadium file
        self.assertTrue(os.path.exists(self.vanFile0))
        self.assertTrue(os.path.exists(self.vanFile1))
        van0=Load(self.vanFile0)
        van1=Load(self.vanFile1)
        m0=ExtractMask(van0)
        m1=ExtractMask(van1)
        self.assertGreaterThan(len(m0[1]),len(m1[1])) #levelsUp=1 should have less pixels masked
        DeleteWorkspace("m0")
        DeleteWorkspace("m1")
        DeleteWorkspace(van0)
        DeleteWorkspace(van1)
        self.assertTrue(os.path.exists(self.nxspeFile))
        LoadNXSPE(self.nxspeFile,OutputWorkspace='nxspe')
        self.disableChecking.append('Instrument')

        return 'nxspe','ARCSsystemtest.nxs'
