#pylint: disable=no-init
from mantid.simpleapi import *
from mantid.api import DataProcessorAlgorithm, AlgorithmFactory, MatrixWorkspaceProperty, PropertyMode, Progress, WorkspaceGroupProperty
from mantid.kernel import StringMandatoryValidator, Direction, logger, IntBoundedValidator, FloatBoundedValidator, MaterialBuilder

#pylint: disable=too-many-instance-attributes
class IndirectAnnulusAbsorption(DataProcessorAlgorithm):
    _can_inner_radius = 0.0
    _can_outer_radius = 0.0
    _output_ws = None
    _ass_ws = None
    _can_ws_name = ''
    _use_can_mass_density = None
    _can_number_density = None
    _can_mass_density = None
    _can_chemical_formula = ''
    _sample_outer_radius = 0.
    _abs_ws = None
    _events = 0
    _use_can_corrections = False
    _sample_ws_name = ''
    _can_scale = 0.
    _sample_chemical_formula = ''
    _acc_ws = None
    _use_sample_mass_density = None
    _sample_number_density = None
    _sample_mass_density = None
    _sample_inner_radius = 0.

    def category(self):
        return "Workflow\\Inelastic;CorrectionFunctions\\AbsorptionCorrections;Workflow\\MIDAS"


    def summary(self):
        return "Calculates indirect absorption corrections for an annulus sample shape."


    def PyInit(self):
        # Sample options
        self.declareProperty(MatrixWorkspaceProperty('SampleWorkspace', '', direction=Direction.Input),
                             doc='Sample workspace.')

        self.declareProperty(name='SampleChemicalFormula', defaultValue='', validator=StringMandatoryValidator(),
                             doc='Sample chemical formula')
        self.declareProperty(name='UseSampleMassDensity', defaultValue=False,
                             doc='Use Sample Mass Density (True) or Sample Number Density (False)')
        self.declareProperty(name='SampleNumberDensity', defaultValue=0.1,
                             validator=FloatBoundedValidator(0.0),
                             doc='Sample number density in atoms/Angstrom3')
        self.declareProperty(name='SampleMassDensity', defaultValue=1.0,
                             validator=FloatBoundedValidator(0.0),
                             doc='Sample mass density in g/cm3')
        self.declareProperty(name='SampleInnerRadius', defaultValue=0.2,
                             validator=FloatBoundedValidator(0.0),
                             doc='Sample radius')
        self.declareProperty(name='SampleOuterRadius', defaultValue=0.25,
                             validator=FloatBoundedValidator(0.0),
                             doc='Sample radius')

        # Container options
        self.declareProperty(MatrixWorkspaceProperty('CanWorkspace', '', optional=PropertyMode.Optional,
                                                     direction=Direction.Input),
                             doc='Container workspace.')
        self.declareProperty(name='UseCanCorrections', defaultValue=False,
                             doc='Use can corrections in subtraction')
        self.declareProperty(name='CanChemicalFormula', defaultValue='',
                             doc='Chemical formula for the can')
        self.declareProperty(name='UseCanMassDensity', defaultValue=False,
                             doc='Use Container Mass Density (True) or Container Number Density (False).')
        self.declareProperty(name='CanNumberDensity', defaultValue=0.1,
                             validator=FloatBoundedValidator(0.0),
                             doc='Container number density in atoms/Angstrom3')
        self.declareProperty(name='CanMassDensity', defaultValue=1.0,
                             validator=FloatBoundedValidator(0.0),
                             doc='Container number density in g/cm3')
        self.declareProperty(name='CanInnerRadius', defaultValue=0.19,
                             validator=FloatBoundedValidator(0.0),
                             doc='Sample radius')
        self.declareProperty(name='CanOuterRadius', defaultValue=0.26,
                             validator=FloatBoundedValidator(0.0),
                             doc='Sample radius')
        self.declareProperty(name='CanScaleFactor', defaultValue=1.0,
                             validator=FloatBoundedValidator(0.0),
                             doc='Scale factor to multiply can data')

        # General options
        self.declareProperty(name='Events', defaultValue=5000,
                             validator=IntBoundedValidator(0),
                             doc='Number of neutron events')

        # Output options
        self.declareProperty(MatrixWorkspaceProperty('OutputWorkspace', '', direction=Direction.Output),
                             doc='The output corrected workspace.')

        self.declareProperty(WorkspaceGroupProperty('CorrectionsWorkspace', '', direction=Direction.Output,
                                                    optional=PropertyMode.Optional),
                             doc='The corrections workspace for scattering and absorptions in sample.')

    #pylint: disable=too-many-branches
    def PyExec(self):
        from IndirectCommon import getEfixed

        self._setup()

        # Set up progress reporting
        n_prog_reports = 2
        if self._can_ws_name is not None:
            n_prog_reports += 1
        prog = Progress(self, 0.0, 1.0, n_prog_reports)

        efixed = getEfixed(self._sample_ws_name)

        sample_wave_ws = '__sam_wave'
        ConvertUnits(InputWorkspace=self._sample_ws_name, OutputWorkspace=sample_wave_ws,
                     Target='Wavelength', EMode='Indirect', EFixed=efixed)

        sample_thickness = self._sample_outer_radius - self._sample_inner_radius
        logger.information('Sample thickness: ' + str(sample_thickness))

        prog.report('Calculating sample corrections')
        if self._use_sample_mass_density:
            builder = MaterialBuilder()
            mat = builder.setFormula(self._sample_chemical_formula).setMassDensity(self._sample_mass_density).build()
            self._sample_number_density = mat.numberDensity
        AnnularRingAbsorption(InputWorkspace=sample_wave_ws,
                              OutputWorkspace=self._ass_ws,
                              SampleHeight=3.0,
                              SampleThickness=sample_thickness,
                              CanInnerRadius=self._can_inner_radius,
                              CanOuterRadius=self._can_outer_radius,
                              SampleChemicalFormula=self._sample_chemical_formula,
                              SampleNumberDensity=self._sample_number_density,
                              NumberOfWavelengthPoints=10,
                              EventsPerPoint=self._events)

        group = self._ass_ws

        if self._can_ws_name is not None:
            can1_wave_ws = '__can1_wave'
            can2_wave_ws = '__can2_wave'
            ConvertUnits(InputWorkspace=self._can_ws_name, OutputWorkspace=can1_wave_ws,
                         Target='Wavelength', EMode='Indirect', EFixed=efixed)
            if self._can_scale != 1.0:
                logger.information('Scaling container by: ' + str(self._can_scale))
                Scale(InputWorkspace=can1_wave_ws, OutputWorkspace=can1_wave_ws, Factor=self._can_scale, Operation='Multiply')
            CloneWorkspace(InputWorkspace=can1_wave_ws, OutputWorkspace=can2_wave_ws)

            can_thickness_1 = self._sample_inner_radius - self._can_inner_radius
            can_thickness_2 = self._can_outer_radius - self._sample_outer_radius
            logger.information('Container thickness: %f & %f' % (can_thickness_1, can_thickness_2))

            if self._use_can_corrections:
                prog.report('Calculating container corrections')
                Divide(LHSWorkspace=sample_wave_ws, RHSWorkspace=self._ass_ws, OutputWorkspace=sample_wave_ws)

                if self._use_can_mass_density:
                    builder = MaterialBuilder()
                    mat = builder.setFormula(self._can_chemical_formula).setMassDensity(self._can_mass_density).build()
                    self._can_number_density = mat.numberDensity
                    SetSampleMaterial(can1_wave_ws, ChemicalFormula=self._can_chemical_formula, SampleNumberDensity=self._can_number_density, SampleMassDensity = self._can_mass_density)
                else:
                    SetSampleMaterial(can1_wave_ws, ChemicalFormula=self._can_chemical_formula, SampleNumberDensity=self._can_number_density)
                AnnularRingAbsorption(InputWorkspace=can1_wave_ws,
                                      OutputWorkspace='__Acc1',
                                      SampleHeight=3.0,
                                      SampleThickness=can_thickness_1,
                                      CanInnerRadius=self._can_inner_radius,
                                      CanOuterRadius=self._sample_outer_radius,
                                      SampleChemicalFormula=self._can_chemical_formula,
                                      SampleNumberDensity=self._can_number_density,
                                      NumberOfWavelengthPoints=10,
                                      EventsPerPoint=self._events)

                SetSampleMaterial(can2_wave_ws, ChemicalFormula=self._can_chemical_formula, SampleNumberDensity=self._can_number_density)
                AnnularRingAbsorption(InputWorkspace=can2_wave_ws,
                                      OutputWorkspace='__Acc2',
                                      SampleHeight=3.0,
                                      SampleThickness=can_thickness_2,
                                      CanInnerRadius=self._sample_inner_radius,
                                      CanOuterRadius=self._can_outer_radius,
                                      SampleChemicalFormula=self._can_chemical_formula,
                                      SampleNumberDensity=self._can_number_density,
                                      NumberOfWavelengthPoints=10,
                                      EventsPerPoint=self._events)

                Multiply(LHSWorkspace='__Acc1', RHSWorkspace='__Acc2', OutputWorkspace=self._acc_ws)
                DeleteWorkspace('__Acc1')
                DeleteWorkspace('__Acc2')

                Divide(LHSWorkspace=can1_wave_ws, RHSWorkspace=self._acc_ws, OutputWorkspace=can1_wave_ws)
                Minus(LHSWorkspace=sample_wave_ws, RHSWorkspace=can1_wave_ws, OutputWorkspace=sample_wave_ws)
                group += ',' + self._acc_ws

            else:
                prog.report('Calculating can scaling')
                Minus(LHSWorkspace=sample_wave_ws, RHSWorkspace=can1_wave_ws, OutputWorkspace=sample_wave_ws)
                Divide(LHSWorkspace=sample_wave_ws, RHSWorkspace=self._ass_ws, OutputWorkspace=sample_wave_ws)

            DeleteWorkspace(can1_wave_ws)
            DeleteWorkspace(can2_wave_ws)

        else:
            Divide(LHSWorkspace=sample_wave_ws,
                   RHSWorkspace=self._ass_ws,
                   OutputWorkspace=sample_wave_ws)

        ConvertUnits(InputWorkspace=sample_wave_ws,
                     OutputWorkspace=self._output_ws,
                     Target='DeltaE',
                     EMode='Indirect',
                     EFixed=efixed)
        DeleteWorkspace(sample_wave_ws)

        prog.report('Recording sample logs')
        sample_log_workspaces = [self._output_ws, self._ass_ws]
        sample_logs = [('sample_shape', 'annulus'),
                       ('sample_filename', self._sample_ws_name),
                       ('sample_inner', self._sample_inner_radius),
                       ('sample_outer', self._sample_outer_radius),
                       ('can_inner', self._can_inner_radius),
                       ('can_outer', self._can_outer_radius)]

        if self._can_ws_name is not None:
            sample_logs.append(('container_filename', self._can_ws_name))
            sample_logs.append(('container_scale', self._can_scale))
            if self._use_can_corrections:
                sample_log_workspaces.append(self._acc_ws)
                sample_logs.append(('container_thickness_1', can_thickness_1))
                sample_logs.append(('container_thickness_2', can_thickness_2))

        log_names = [item[0] for item in sample_logs]
        log_values = [item[1] for item in sample_logs]

        for ws_name in sample_log_workspaces:
            AddSampleLogMultiple(Workspace=ws_name, LogNames=log_names, LogValues=log_values)

        self.setProperty('OutputWorkspace', self._output_ws)

        # Output the Ass workspace if it is wanted, delete if not
        if self._abs_ws == '':
            DeleteWorkspace(self._ass_ws)
            if self._can_ws_name is not None and self._use_can_corrections:
                DeleteWorkspace(self._acc_ws)
        else:
            GroupWorkspaces(InputWorkspaces=group, OutputWorkspace=self._abs_ws)
            self.setProperty('CorrectionsWorkspace', self._abs_ws)

    def _setup(self):
        """
        Get algorithm properties.
        """

        self._sample_ws_name = self.getPropertyValue('SampleWorkspace')
        self._sample_chemical_formula = self.getPropertyValue('SampleChemicalFormula')
        self._use_sample_mass_density = self.getProperty('UseSampleMassDensity').value
        self._sample_number_density = self.getProperty('SampleNumberDensity').value
        self._sample_mass_density = self.getProperty('SampleMassDensity').value
        self._sample_inner_radius = self.getProperty('SampleInnerRadius').value
        self._sample_outer_radius = self.getProperty('SampleOuterRadius').value

        self._can_ws_name = self.getPropertyValue('CanWorkspace')
        if self._can_ws_name == '':
            self._can_ws_name = None
        self._use_can_corrections = self.getProperty('UseCanCorrections').value
        self._can_chemical_formula = self.getPropertyValue('CanChemicalFormula')
        self._use_can_mass_density = self.getProperty('UseCanMassDensity').value
        self._can_number_density = self.getProperty('CanNumberDensity').value
        self._can_mass_density = self.getProperty('CanMassDensity').value
        self._can_inner_radius = self.getProperty('CanInnerRadius').value
        self._can_outer_radius = self.getProperty('CanOuterRadius').value
        self._can_scale = self.getProperty('CanScaleFactor').value

        self._events = self.getProperty('Events').value
        self._output_ws = self.getPropertyValue('OutputWorkspace')

        self._abs_ws = self.getPropertyValue('CorrectionsWorkspace')
        if self._abs_ws == '':
            self._ass_ws = '__ass'
            self._acc_ws = '__acc'
        else:
            self._ass_ws = self._abs_ws + '_ass'
            self._acc_ws = self._abs_ws + '_acc'


    def validateInputs(self):
        """
        Validate algorithm options.
        """

        self._setup()
        issues = dict()

        if self._use_can_corrections and self._can_chemical_formula == '':
            issues['CanChemicalFormula'] = 'Must be set to use can corrections'

        if self._use_can_corrections and self._can_ws_name is None:
            issues['UseCanCorrections'] = 'Must specify a can workspace to use can corections'

        # Geometry validation: can inner < sample inner < sample outer < can outer
        if self._sample_inner_radius < self._can_inner_radius:
            issues['SampleInnerRadius'] = 'Must be greater than CanInnerRadius'

        if self._sample_outer_radius < self._sample_inner_radius:
            issues['SampleOuterRadius'] = 'Must be greater than SampleInnerRadius'

        if self._can_outer_radius < self._sample_outer_radius:
            issues['CanOuterRadius'] = 'Must be greater than SampleOuterRadius'

        return issues


# Register algorithm with Mantid
AlgorithmFactory.subscribe(IndirectAnnulusAbsorption)
