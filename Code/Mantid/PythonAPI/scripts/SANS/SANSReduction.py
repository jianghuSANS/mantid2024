###########################################################################
# This is the template analysis script for reducing the SANS data.        #
# It has place holders so that the appropriate data can be filled in at   #
# run time by the SANS GUI                                                #
#                                                                         #
#  Authors: Russell Taylor, Tessella plc (Original script)                #
#           Martyn Gigg, Tessella plc (Adapted to GUI use)                #
#           with major input from Richard Heenan                          #
###########################################################################
#
#
# README: 
# This script contains a list of function definitions to perform the SANS data reduction.
# It was primarily designed as a back end to the GUI. When being used from the GUI, MantidPlot
# automatically replaces all of the information at the top of this module
#  

#
# Set up for cycle 09/02 for which the spectra are now by rows, right to left 
# from top right, and start at 9 after 8 monitors !
#
import SANSUtility
import math
from mantidsimple import *

# ---------------------------- CORRECTION INPUT -----------------------------------------
# The information between this line and the other '-----' delimiter needs to be provided
# for the script to function. From the GUI, the tags will be replaced by the appropriate
# information. 

# The tags get replaced by input from the GUI
# The workspaces
SCATTER_SAMPLE = None
SCATTER_CAN = ''
TRANS_SAMPLE = ''
TRANS_CAN = ''
DIRECT_SAMPLE = ''
DIRECT_CAN = ''

# Now the mask string (can be empty)
SPECMASKSTRING = ''
TIMEMASKSTRING = ''

# Instrument information
INSTR_DIR = mtd.getConfigProperty('instrumentDefinition.directory')
INSTR_NAME = 'SANS2D'
# Beam centre in mm
XBEAM_CENTRE = None
YBEAM_CENTRE = None

# Analysis tab values
RMIN = None
RMAX = None
DEF_RMIN = None
DEF_RMAX = None
WAV1 = None
WAV2 = None
DWAV = None
Q1 = None
Q2 = None
DQ = None
QXY2 = None
DQXY = None
DIRECT_BEAM_FILE = None
GRAVITY = False
# This indicates whether a 1D or a 2D analysis is performed
CORRECTION_TYPE = '1D'
# Component positions
SAMPLE_Z_CORR = 0.0

# Scaling values
RESCALE = 1.0*100.0
SAMPLE_GEOM = 3
SAMPLE_WIDTH = 1.0
SAMPLE_HEIGHT = 1.0
SAMPLE_THICKNESS = 1.0 

# These values are used for the start and end bins for FlatBackground removal.
###############################################################################################
# RICHARD'S NOTE FOR SANS2D: these may need to vary with chopper phase and detector distance !
# !TASK! Put the values in the mask file if they need to be different ?????
##############################################################################################
# The GUI will replace these with default values of
# LOQ: 31000 -> 39000
# S2D: 85000 -> 100000
BACKMON_START = None
BACKMON_END = None

# The detector bank to look at. The GUI has an options box to select the detector to analyse. 
# The spectrum numbers are deduced from the name within the rear-detector tag. Names are from the 
# instrument definition file
# LOQ: HAB or main-detector-bank
# S2D: front-detector or rear-detector 
DETBANK = None

# The monitor spectrum taken from the GUI. Is this still necessary?? or can I just deduce
# it from the instrument name 
MONITORSPECTRUM = None

# Detector position information for SANS2D
FRONT_DET_RADIUS = 306.0
FRONT_DET_DEFAULT_SD_M = 4.0
FRONT_DET_DEFAULT_X_M = 1.1
REAR_DET_DEFAULT_SD_M = 4.0

# LOG files for SANS2D will have these encoder readings  
FRONT_DET_Z = 0.0
FRONT_DET_X = 0.0
FRONT_DET_ROT = 0.0
REAR_DET_Z = 0.0
# Rear_Det_X  Will Be Needed To Calc Relative X Translation Of Front Detector 
REAR_DET_X = 0.0

# MASK file stuff ==========================================================
# correction terms to SANS2d encoders - store in MASK file ?
FRONT_DET_Z_CORR = 0.0
FRONT_DET_Y_CORR = 0.0 
FRONT_DET_X_CORR = 0.0 
FRONT_DET_ROT_CORR = 0.0
REAR_DET_Z_CORR = 0.0 
REAR_DET_X_CORR = 0.0

#------------------------------- End of input section -----------------------------------------

# Transmission variables for SANS2D. The CalculateTransmission algorithm contains the defaults
# for LOQ so these are not used for LOQ
TRANS_WAV1 = 2.0
TRANS_WAV2 = 14.0
TRANS_UDET_MON = 2
TRANS_UDET_DET = 3

###################################################################################################################
#
#                              Interface functions (to be called from scripts or the GUI)
#
###################################################################################################################
# "Enumerations"
DefaultTrans = True
NewTrans = False

# Print a message and log it if the 
def _printMessage(msg, log = True):
    print msg
    if log == True:
        mantid.sendLogMessage(msg)

# Warn the user
def _issueWarning(msg):
    print 'WARNING: ' + msg

# Fatal error
def _fatalError(msg):
    exit(msg)

DATA_PATH = ''
# Set the data directory
def DataPath(directory):
    if os.path.exists(directory) == False:
        _issueWarning("Data directory does not exist")
        return
    global DATA_PATH
    DATA_PATH = directory

USER_PATH = ''
# Set the user directory
def UserPath(directory):
    if os.path.exists(directory) == False:
        _issueWarning("Data directory does not exist")
        return
    global USER_PATH
    USER_PATH = directory


########################### 
# Instrument
########################### 
def SANS2D():
    global INSTR_NAME, BACKMON_START, BACKMON_END, MONITORSPECTRUM
    INSTR_NAME = 'SANS2D'
    BACKMON_START = 85000
    BACKMON_END = 100000
    Detector('rear-detector')
    MONITORSPECTRUM = 2

def LOQ():
    global INSTR_NAME, BACKMON_START, BACKMON_END, MONITORSPECTRUM
    INSTR_NAME = 'LOQ'
    BACKMON_START = 31000
    BACKMON_END = 39000
    Detector('main-detector-bank')
    MONITORSPECTRUM = 2

def Detector(det_name):
    if INSTR_NAME == 'SANS2D' and (det_name == 'rear-detector' or det_name == 'front_detector') or \
            INSTR_NAME == 'LOQ' and (det_name == 'main-detector-bank' or det_name == 'HAB'):
        global DETBANK
        DETBANK = det_name
    else:
        _fatalError('Attempting to set invalid detector name: "' + det_name + '"')

def Set1D():
    global CORRECTION_TYPE
    CORRECTION_TYPE = '1D'

def Set2D():
    global CORRECTION_TYPE
    CORRECTION_TYPE = '2D'

########################### 
# Set the scattering sample raw workspace
########################### 
_SAMPLE_SETUP = None
_SAMPLE_RUN = ''
def AssignSample(sample_run):
    global SCATTER_SAMPLE, _SAMPLE_SETUP, _SAMPLE_RUN
    _SAMPLE_RUN = sample_run
    SCATTER_SAMPLE = _assignHelper(sample_run, False)
    _SAMPLE_SETUP = None
    if (INSTR_NAME == 'SANS2D' and sample_run != ''):
        logvalues = _loadDetectorLogs(sample_run)
    else:
        return
        
    global FRONT_DET_Z, FRONT_DET_X, FRONT_DET_ROT, REAR_DET_Z, REAR_DET_X
    FRONT_DET_Z = logvalues['Front_Det_Z']
    FRONT_DET_X = logvalues['Front_Det_X']
    FRONT_DET_ROT = logvalues['Front_Det_Rot']
    REAR_DET_Z = logvalues['Rear_Det_Z']
    REAR_DET_X = logvalues['Rear_Det_X']

########################### 
# Set the scattering can raw workspace
########################### 
_CAN_SETUP = None
_CAN_RUN = ''
def AssignCan(can_run):
    global SCATTER_CAN, _CAN_SETUP, _CAN_RUN
    _CAN_RUN = can_run
    SCATTER_CAN = _assignHelper(can_run, False)
    _CAN_SETUP  = None
    if (INSTR_NAME == 'SANS2D' and can_run != ''):
        logvalues = _loadDetectorLogs(can_run)
    else:
        return
    # Check against sample values and warn if they are not the same but still continue reduction
    can_values = []
    can_values.append(logvalues['Front_Det_Z'] + FRONT_DET_Z_CORR)
    can_values.append(logvalues['Front_Det_X'] + FRONT_DET_X_CORR)
    can_values.append(logvalues['Front_Det_Rot'] + FRONT_DET_ROT_CORR)
    can_values.append(logvalues['Rear_Det_Z'] + REAR_DET_Z_CORR)
    can_values.append(logvalues['Rear_Det_X'] + REAR_DET_X_CORR)

    smp_values = []
    smp_values.append(FRONT_DET_Z + FRONT_DET_Z_CORR)
    smp_values.append(FRONT_DET_X + FRONT_DET_X_CORR)
    smp_values.append(FRONT_DET_ROT + FRONT_DET_ROT_CORR)
    smp_values.append(REAR_DET_Z + REAR_DET_Z_CORR)
    smp_values.append(REAR_DET_X + REAR_DET_X_CORR)

    det_names = ['Front_Det_Z', 'Front_Det_X','Front_Det_Rot', 'Rear_Det_Z', 'Rear_Det_X']
    for i in range(0, 5):
        if math.fabs(smp_values[i] - can_values[i]) > 5e-03:
            _issueWarning(det_names[i] + " values differ between sample and can runs. Sample = " + str(smp_values[i]) + \
                              ' , Can = ' + str(can_values[i]))

########################### 
# Set the trans sample and measured raw workspaces
########################### 
def TransmissionSample(sample, direct):
    global TRANS_SAMPLE, DIRECT_SAMPLE
    TRANS_SAMPLE = _assignHelper(sample, True)
    DIRECT_SAMPLE = _assignHelper(direct, True)

########################## 
# Set the trans sample and measured raw workspaces
########################## 
def TransmissionCan(can, direct):
    global TRANS_CAN, DIRECT_CAN
    TRANS_CAN = _assignHelper(can, True)
    DIRECT_CAN = _assignHelper(direct, True)

# Helper function
def _assignHelper(run_string, is_trans):
    if run_string == '':
        return ''
    pieces = run_string.split('.')
    if len(pieces) != 2 :
         _fatalError("Invalid run specified: " + run_string + ". Please use RUNNUMBER.EXT format")
    else:
        run_no = pieces[0]
        ext = pieces[1]
    if is_trans:
        wkspname =  run_no + '_trans_' + ext.lower()
    else:
        wkspname =  run_no + '_sans_' + ext.lower()

    if INSTR_NAME == 'LOQ':
        field_width = 5
    else:
        field_width = 8
        
    basename = INSTR_NAME + run_no.rjust(field_width, '0')
    if basename == INSTR_NAME + ''.rjust(field_width, '0'):
        return ''
    
    filename = os.path.join(DATA_PATH,basename)
    if is_trans:
        _loadRawData(filename, wkspname, ext, spec_max = 8)
    else:
        _loadRawData(filename, wkspname, ext)
    return wkspname

##########################
# Loader function
##########################
def _loadRawData(filename, workspace, ext, spec_max = None):
    if ext.lower() == 'raw':
        if spec_max == None:
            LoadRaw(filename + '.' + ext, workspace)
        else:
            LoadRaw(filename + '.' + ext, workspace, SpectrumMax=spec_max)
        LoadSampleDetailsFromRaw(workspace, filename + '.' + ext)
    else:
        if spec_max == None:
            LoadNexus(filename + '.' + ext, workspace)
        else:
            LoadNexus(filename + '.' + ext, workspace, SpectrumMax=spec_max)

    sample_details = mtd.getMatrixWorkspace(workspace).getSampleDetails()
    SampleGeometry(sample_details.getGeometryFlag())
    SampleThickness(sample_details.getThickness())
    SampleHeight(sample_details.getHeight())
    SampleWidth(sample_details.getWidth())


# Load the detector logs
def _loadDetectorLogs(run_string):
    run_no = run_string[:run_string.rfind('.')]
    logname = INSTR_NAME + run_no.rjust(8, '0')
    # Adding runs produces a 1000nnnn or 2000nnnn. For less copying, of log files doctor the filename
    logname = logname[0:6] + '0' + logname[7:]
    filename = os.path.join(DATA_PATH, logname + '.log')

    # Build a dictionary of log data 
    logvalues = {}
    logvalues['Rear_Det_X'] = 0.0
    logvalues['Rear_Det_Z'] = 0.0
    logvalues['Front_Det_X'] = 0.0
    logvalues['Front_Det_Z'] = 0.0
    logvalues['Front_Det_Rot'] = 0.0
    file_handle = open(filename, 'r')
    for line in file_handle:
        parts = line.split()
        if len(parts) != 3:
            _issueWarning('Incorrect structure detected in logfile "' + filename + '" for line \n"' + line + '"\nEntry skipped')
        component = parts[1]
        if component in logvalues.keys():
            logvalues[component] = float(parts[2])
    
    file_handle.close()
    return logvalues

#########################
# Limits 
def LimitsR(rmin, rmax):
    _readLimitValues('L/R ' + str(rmin) + ' ' + str(rmax) + ' 1')

def LimitsWav(lmin, lmax, step, type):
    _readLimitValues('L/WAV ' + str(lmin) + ' ' + str(lmax) + ' ' + str(step) + '/'  + type)

def LimitsQ(qmin, qmax, step, type):
    _readLimitValues('L/Q ' + str(qmin) + ' ' + str(qmax) + ' ' + str(step) + '/'  + type)

def LimitsQXY(qmin, qmax, step, type):
    _readLimitValues('L/QXY ' + str(qmin) + ' ' + str(qmax) + ' ' + str(step) + '/'  + type)
    
def Gravity(flag):
    if isinstance(flag, bool) or elif isinstance(flag, int):
        global GRAVITY
        GRAVITY = flag
    else:
        _warnUser("Invalid GRAVITY flag passed, try True/False. Setting kept as " + str(GRAVITY))

######################### 
# Sample geometry flag
######################### 
def SampleGeometry(geom_id):
    if geom_id > 3 or geom_id < 1:
        _fatalError("Invalid geometry type for sample: " + str(geom_id))
    global SAMPLE_GEOM
    SAMPLE_GEOM = geom_id

######################### 
# Sample width
######################### 
def SampleWidth(width):
    if SAMPLE_GEOM == None:
        _fatalError('Attempting to set width without setting geometry flag. Please set geometry type first')
    global SAMPLE_WIDTH
    SAMPLE_WIDTH = width
    # For a disk the height=width
    if SAMPLE_GEOM == 3:
        global SAMPLE_HEIGHT
        SAMPLE_HEIGHT = width

######################### 
# Sample height
######################### 
def SampleHeight(height):
    if SAMPLE_GEOM == None:
        _fatalError('Attempting to set height without setting geometry flag. Please set geometry type first')
    global SAMPLE_HEIGHT
    SAMPLE_HEIGHT = height
    # For a disk the height=width
    if SAMPLE_GEOM == 3:
        global SAMPLE_WIDTH
        SAMPLE_WIDTH = height

######################### 
# Sample thickness
#########################
def SampleThickness(thickness):
    global SAMPLE_THICKNESS
    SAMPLE_THICKNESS = thickness

#############################
# Print sample geometry
###########################
def displayGeometry():
    print 'Beam centre: [' + str(XBEAM_CENTRE) + ',' + str(YBEAM_CENTRE) + ']'
    print '-- Sample Geometry --\n' + \
        '    ID: ' + str(SAMPLE_GEOM) + '\n' + \
        '    Width: ' + str(SAMPLE_WIDTH) + '\n' + \
        '    Height: ' + str(SAMPLE_HEIGHT) + '\n' + \
        '    Thickness: ' + str(SAMPLE_THICKNESS) + '\n'


######################################
# Set the centre in mm
####################################
def SetCentre(XVAL, YVAL):
    global XBEAM_CENTRE, YBEAM_CENTRE
    XBEAM_CENTRE = XVAL/1000.
    YBEAM_CENTRE = YVAL/1000.

####################################
# Add a mask to the correct string
###################################
def Mask(details):
    details = details.lstrip()
    details_compare = details.upper()
    global TIMEMASKSTRING, SPECMASKSTRING
    if details_compare.startswith('/CLEAR/TIME'):
        TIMEMASKSTRING = ''
    elif details_compare.startswith('/CLEAR'):
        SPECMASKSTRING = ''
    elif details_compare.startswith('/T'):
        TIMEMASKSTRING += ';' + details[2:].lstrip()
    elif ( details_compare.startswith('S') or details_compare.startswith('H') or details_compare.startswith('V') ):
        SPECMASKSTRING += ',' + details.lstrip()
    else:
        pass

#############################
# Read a mask file
#############################
def MaskFile(filename):
    if os.path.isabs(filename) == False:
        filename = os.path.join(USER_PATH, filename)

    if os.path.exists(filename) == False:
        _fatalError("Cannot read mask file '" + filename + "', path does not exist.")

    file_handle = open(filename, 'r')
    for line in file_handle:
        if line.startswith('!'):
            continue
        # This is so that I can be sure all EOL characters have been removed
        line = line.rstrip()
        upper_line = line.upper()
        if upper_line.startswith('L/'):
            _readLimitValues(line)
        elif upper_line.startswith('MON/'):
            details = line[4:]
            if details.upper().startswith('LENGTH'):
                global MONITORSPECTRUM
                MONITORSPECTRUM = int(details.partition(' ')[2])
            else:
                filepath = details[7:].rstrip()
                if '[' in filepath:
                    idx = filepath.rfind(']')
                    filepath = filepath[idx + 1:]
                if not os.path.isabs(filepath):
                    filepath = os.path.join(USER_PATH, filepath)

                type = details[0:6]
                if type.upper() == 'DIRECT':
                    global DIRECT_BEAM_FILE
                    DIRECT_BEAM_FILE = filepath
        elif upper_line.startswith('MASK'):
            Mask(upper_line[4:])
        elif upper_line.startswith('SET CENTRE'):
            values = upper_line.split()
            SetCentre(float(values[2]), float(values[3]))
        elif upper_line.startswith('SET SCALES'):
            values = upper_line.split()
            global RESCALE
            RESCALE = float(values[2])*100.0
        elif upper_line.startswith('SAMPLE/OFFSET'):
            values = upper_line.split()
            global SAMPLE_Z_CORR
            SAMPLE_Z_CORR = float(values[1])/1000.
        elif upper_line.startswith('DET/CORR'):
            _readDetectorCorrections(upper_line[8:])
        else:
            continue

    # Close the handle
    file_handle.close()

# Read a limit line of a mask file
def _readLimitValues(limit_line):
    limits = limit_line.partition('L/')[2]
    # Split with no arguments defaults to any whitespace character and in particular
    # multiple spaces are include
    elements = limits.split()
    type, minval, maxval = elements[0], elements[1], elements[2]
    if len(elements) == 4:
        step = elements[3]
        step_details = step.split('/')
        if len(step_details) == 2:
            step_size = step_details[0]
            step_type = step_details[1]
            if step_type.upper() == 'LIN':
                step_type = ''
            else:
                step_type = '-'
        else:
            step_size = step_details[0]

    if type.upper() == 'WAV':
        global WAV1, WAV2, DWAV
        WAV1 = float(minval)
        WAV2 = float(maxval)
        DWAV = float(step_type + step_size)
    elif type.upper() == 'Q':
        global Q1, Q2, DQ
        Q1 = float(minval)
        Q2 = float(maxval)
        DQ = float(step_type + step_size)
    elif type.upper() == 'QXY':
        global QXY2, DQXY
        QXY2 = float(maxval)
        DQXY = float(step_type + step_size)
    elif type.upper() == 'R':
        global RMIN, RMAX, DEF_RMIN, DEF_RMAX
        RMIN = float(minval)/1000.
        RMAX = float(maxval)/1000.
        DEF_RMIN = RMIN
        DEF_RMAX = RMAX
    else:
        pass

def _readDetectorCorrections(details):
    values = details.split()
    det_name = values[0]
    det_axis = values[1]
    shift = float(values[2])

    if det_name == 'REAR':
        if det_axis == 'X':
            global REAR_DET_X_CORR
            REAR_DET_X_CORR = shift
        elif det_axis == 'Z':
            global REAR_DET_Z_CORR
            REAR_DET_Z_CORR = shift
        else:
            pass
    else:
        if det_axis == 'X':
            global FRONT_DET_X_CORR
            FRONT_DET_X_CORR = shift
        elif det_axis == 'Y':
            global FRONT_DET_Y_CORR
            FRONT_DET_Y_CORR = shift
        elif det_axis == 'Z':
            global FRONT_DET_Z_CORR
            FRONT_DET_Z_CORR = shift
        elif det_axis == 'ROT':
            global FRONT_DET_ROT_CORR
            FRONT_DET_ROT_CORR = shift
        else:
            pass    

def displayMaskFile():
    print '-- Mask file defaults --'
    print '    Wavelength range: ',WAV1, WAV2, DWAV
    print '    Q range: ',Q1, Q2, DQ
    print '    QXY range: ', QXY2, DQXY
    print '    radius', RMIN, RMAX
    print '    direct beam file:', DIRECT_BEAM_FILE
    print '    spectrum mask: ', SPECMASKSTRING
    print '    time mask: ', TIMEMASKSTRING

# ---------------------------------------------------------------------------------------

##
# Set up the sample and can detectors and calculate the transmission if available
##
def _initReduction(xcentre = None, ycentre = None):
    # *** Sample setup first ***
    if SCATTER_SAMPLE == None:
        exit('Error: No sample run has been set')

    if xcentre == None or ycentre == None:
        xcentre = XBEAM_CENTRE
        ycentre = YBEAM_CENTRE

    global _SAMPLE_SETUP	
    if _SAMPLE_SETUP == None:
        _SAMPLE_SETUP = _init_run(SCATTER_SAMPLE, [xcentre, ycentre], False)
    
    global _CAN_SETUP
    if SCATTER_CAN != '' and _CAN_SETUP == None:
        _CAN_SETUP = _init_run(SCATTER_CAN, [xcentre, ycentre], True)

    # Instrument specific information using function in utility file
    global DIMENSION, SPECMIN, SPECMAX, BACKMON_START, BACKMON_END
    DIMENSION, SPECMIN, SPECMAX, BACKMON_START, BACKMON_END = SANSUtility.GetInstrumentDetails(INSTR_NAME, DETBANK)

    return _SAMPLE_SETUP, _CAN_SETUP

##
# Run the reduction for a given wavelength range
##
def WavRangeReduction(wav_start, wav_end, use_def_trans = DefaultTrans, finding_centre = False):
    if finding_centre == False:
        _printMessage("Running reduction for wavelength range " + str(wav_start) + '-' + str(wav_end))
    # This only performs the init if it needs to
    sample_setup, can_setup = _initReduction(XBEAM_CENTRE, YBEAM_CENTRE)

    wsname_cache = sample_setup.getReducedWorkspace()
    # Run correction function
    if finding_centre == True:
        final_workspace = wsname_cache.split('_')[0] + '_quadrants'
    else:
        final_workspace = wsname_cache + '_' + str(wav_start) + '_' + str(wav_end)
    sample_setup.setReducedWorkspace(final_workspace)
    # Perform correction
    Correct(sample_setup, wav_start, wav_end, use_def_trans, finding_centre)

    if can_setup != None:
        tmp_smp = final_workspace+"_sam_tmp"
        RenameWorkspace(final_workspace, tmp_smp)
        # Run correction function
        # was  Correct(SCATTER_CAN, can_setup[0], can_setup[1], wav_start, wav_end, can_setup[2], can_setup[3], finding_centre)
        tmp_can = final_workspace+"_can_tmp"
        can_setup.setReducedWorkspace(tmp_can)
        # Can correction
        Correct(can_setup, wav_start, wav_end, use_def_trans, finding_centre)
        Minus(tmp_smp, tmp_can, final_workspace)

        # Due to rounding errors, small shifts in detector encoders and poor stats in highest Q bins need "minus" the
        # workspaces before removing nan & trailing zeros thus, beware,  _sc,  _sam_tmp and _can_tmp may NOT have same Q bins
        if finding_centre == False:
            ReplaceSpecialValues(InputWorkspace = tmp_smp,OutputWorkspace = tmp_smp, NaNValue="0", InfinityValue="0")
            ReplaceSpecialValues(InputWorkspace = tmp_can,OutputWorkspace = tmp_can, NaNValue="0", InfinityValue="0")
            if CORRECTION_TYPE == '1D':
                SANSUtility.StripEndZeroes(tmp_smp)
                SANSUtility.StripEndZeroes(tmp_can)
        else:
            mantid.deleteWorkspace(tmp_smp)
            mantid.deleteWorkspace(tmp_can)
                
    # Crop Workspace to remove leading and trailing zeroes
    if finding_centre == False:
        # Replaces NANs with zeroes
        ReplaceSpecialValues(InputWorkspace = final_workspace, OutputWorkspace = final_workspace, NaNValue="0", InfinityValue="0")
        if CORRECTION_TYPE == '1D':
            SANSUtility.StripEndZeroes(final_workspace)
    else:
        RenameWorkspace(final_workspace + '_1', 'Left')
        RenameWorkspace(final_workspace + '_2', 'Right')
        RenameWorkspace(final_workspace + '_3', 'Up')
        RenameWorkspace(final_workspace + '_4', 'Down')
        UnGroupWorkspace(final_workspace)

    # Revert the name change so that future calls with different wavelengths get the correct name
    sample_setup.setReducedWorkspace(wsname_cache)                                
    return final_workspace

##
# Init helper
##
def _init_run(raw_ws, beamcoords, emptycell):
    if raw_ws == '':
        return None

    if emptycell:
        _printMessage('Initializing can workspace to [' + str(beamcoords[0]) + ',' + str(beamcoords[1]) + ']' )
    else:
        _printMessage('Initializing sample workspace to [' + str(beamcoords[0]) + ',' + str(beamcoords[1]) + ']' )

    if emptycell == True:
        final_ws = "can_temp_workspace"
    else:
        final_ws = raw_ws.split('_')[0]
        if DETBANK == 'front-detector':
            final_ws += 'front'
        elif DETBANK == 'rear-detector':
            final_ws += 'rear'
        elif DETBANK == 'main-detector-bank':
            final_ws += 'main'
        else:
            final_ws += 'HAB'
            final_ws += '_' + CORRECTION_TYPE

    # Put the components in the correct positions
    maskpt_rmin, maskpt_rmax = SetupComponentPositions(DETBANK, raw_ws, beamcoords[0], beamcoords[1])
    
    # Create a run details object
    if emptycell == True:
        return SANSUtility.RunDetails(raw_ws, final_ws, TRANS_CAN, DIRECT_CAN, maskpt_rmin, maskpt_rmax, 'can')
    else:
        return SANSUtility.RunDetails(raw_ws, final_ws, TRANS_SAMPLE, DIRECT_SAMPLE, maskpt_rmin, maskpt_rmax, 'sample')

##
# Setup the transmission workspace
##
def CalculateTransmissionCorrection(run_setup, lambdamin, lambdamax, use_def_trans):
    trans_raw = run_setup.getTransRaw()
    direct_raw = run_setup.getDirectRaw()
    if trans_raw == '' or direct_raw == '':
        return None

    if use_def_trans == True:
        if INSTR_NAME == 'SANS2D':
            fulltransws = trans_raw.split('_')[0] + '_trans_' + run_setup.getSuffix() + '_' + str(TRANS_WAV1) + '_' + str(TRANS_WAV2)
            wavbin = str(TRANS_WAV1) + ',' + str(DWAV) + ',' + str(TRANS_WAV2)
        else:
            fulltransws = trans_raw.split('_')[0] + '_trans_' + run_setup.getSuffix() + '_2.2_10'
            wavbin = str(2.2) + ',' + str(DWAV) + ',' + str(10.0)
    else:
        fulltransws = trans_raw.split('_')[0] + '_trans_' + run_setup.getSuffix() + '_' + str(lambdamin) + '_' + str(lambdamax)
        wavbin = str(lambdamin) + ',' + str(DWAV) + ',' + str(lambdamax)

    if mtd.workspaceExists(fulltransws) == False or use_def_trans == False:
        if INSTR_NAME == 'LOQ':
            # Change the instrument definition to the correct one in the LOQ case
            LoadInstrument(trans_raw, INSTR_DIR + "/LOQ_trans_Definition.xml")
            LoadInstrument(direct_raw, INSTR_DIR + "/LOQ_trans_Definition.xml")
            trans_tmp_out = SANSUtility.SetupTransmissionWorkspace(trans_raw, '1,2', BACKMON_START, BACKMON_END, wavbin, True)
            direct_tmp_out = SANSUtility.SetupTransmissionWorkspace(direct_raw, '1,2', BACKMON_START, BACKMON_END, wavbin, True)
            CalculateTransmission(trans_tmp_out,direct_tmp_out, fulltransws, OutputUnfittedData=True)
        else:
            trans_tmp_out = SANSUtility.SetupTransmissionWorkspace(trans_raw, '1,2', BACKMON_START, BACKMON_END, wavbin, False) 
            direct_tmp_out = SANSUtility.SetupTransmissionWorkspace(direct_raw, '1,2', BACKMON_START, BACKMON_END, wavbin, False)
            CalculateTransmission(trans_tmp_out,direct_tmp_out, fulltransws, TRANS_UDET_MON, TRANS_UDET_DET, TRANS_WAV1, TRANS_WAV2, OutputUnfittedData=True)
        # Remove temopraries
        mantid.deleteWorkspace(trans_tmp_out)
        mantid.deleteWorkspace(direct_tmp_out)

    if use_def_trans == True:
        tmp_ws = 'trans_' + run_setup.getSuffix() + '_' + str(lambdamin) + '_' + str(lambdamax)
        CropWorkspace(fulltransws, tmp_ws, XMin = str(lambdamin), XMax = str(lambdamax))
        return tmp_ws
    else: 
        return fulltransws

##
# Setup component positions
##
def SetupComponentPositions(detector, dataws, xbeam, ybeam):
    # Put the components in the correct place
    # The sample holder
    MoveInstrumentComponent(dataws, 'some-sample-holder', Z = SAMPLE_Z_CORR, RelativePosition="1")
    
    # The detector
    if INSTR_NAME == 'LOQ':
        xshift = (317.5/1000.) - xbeam
        yshift = (317.5/1000.) - ybeam
        MoveInstrumentComponent(dataws, detector, X = xshift, Y = yshift, RelativePosition="1")
        # LOQ instrument description has detector at 0.0, 0.0
        return [xshift, yshift], [xshift, yshift] 
    else:
        if detector == 'front-detector':
            rotateDet = (-FRONT_DET_ROT - FRONT_DET_ROT_CORR)
            RotateInstrumentComponent(dataws, detector,X="0.",Y="1.0",Z="0.",Angle=rotateDet)
            RotRadians = math.pi*(FRONT_DET_ROT + FRONT_DET_ROT_CORR)/180.
            xshift = (REAR_DET_X + REAR_DET_X_CORR - FRONT_DET_X - FRONT_DET_X_CORR + FRONT_DET_RADIUS*math.sin(RotRadians ) )/1000. - FRONT_DET_DEFAULT_X_M - xbeam
            yshift = (FRONT_DET_X_CORR /1000.  - ybeam)
            # default in instrument description is 23.281m - 4.000m from sample at 19,281m !
            # need to add ~58mm to det1 to get to centre of detector, before it is rotated.
            zshift = (FRONT_DET_Z + FRONT_DET_Z_CORR + FRONT_DET_RADIUS*(1 - math.cos(RotRadians)) )/1000. - FRONT_DET_DEFAULT_SD_M
            MoveInstrumentComponent(dataws, detector, X = xshift, Y = yshift, Z = zshift, RelativePosition="1")
            return [0.0, 0.0], [0.0, 0.0]
        else:
            xshift = -xbeam
            yshift = -ybeam
            zshift = (REAR_DET_Z + REAR_DET_Z_CORR)/1000. - REAR_DET_DEFAULT_SD_M
            mantid.sendLogMessage("::SANS:: Setup move "+str(xshift*1000.)+" "+str(yshift*1000.))              
            MoveInstrumentComponent(dataws, detector, X = xshift, Y = yshift, Z = zshift, RelativePosition="1")
            return [0.0,0.0], [xshift, yshift]

#----------------------------------------------------------------------------------------------------------------------------
##
# Main correction routine
##
def Correct(run_setup, wav_start, wav_end, use_def_trans, finding_centre = False):
    '''Performs the data reduction steps'''
    global MONITORSPECTRUM, SPECMIN, SPECMAX
    sample_raw = run_setup.getRawWorkspace()
    if INSTR_NAME == "SANS2D":
        sample_run = sample_raw.split('_')[0]
        if int(sample_run) < 568:
            MONITORSPECTRUM = 73730
            monstart = 0
            if DETBANK == 'rear-detector':
                SPECMIN = DIMENSION*DIMENSION + 1 + monstart
                SPECMAX = DIMENSION*DIMENSION*2 + monstart
            else:
                SPECMIN = 1 + monstart
                SPECMAX = DIMENSION*DIMENSION + monstart                        
                                
    ############################# Setup workspaces ######################################
    monitorWS = "Monitor"
    # Get the monitor ( StartWorkspaceIndex is off by one with cropworkspace)
    CropWorkspace(sample_raw, monitorWS, StartWorkspaceIndex = str(MONITORSPECTRUM - 1), EndWorkspaceIndex = str(MONITORSPECTRUM - 1))
    if INSTR_NAME == 'LOQ':
        RemoveBins(monitorWS, monitorWS, '19900', '20500', Interpolation="Linear")
    
    # Remove flat background
    FlatBackground(monitorWS, monitorWS, '0', BACKMON_START, BACKMON_END)

    # Get the bank we are looking at
    final_result = run_setup.getReducedWorkspace()
    CropWorkspace(sample_raw, final_result, StartWorkspaceIndex = (SPECMIN - 1), EndWorkspaceIndex = str(SPECMAX - 1))
    #####################################################################################
        
    ########################## Masking  ################################################
    # Mask the corners and beam stop if radius parameters are given
    maskpt_rmin = run_setup.getMaskPtMin()
    maskpt_rmax = run_setup.getMaskPtMax()	
    if finding_centre == True:
        if RMIN > 0.0: 
            SANSUtility.MaskInsideCylinder(final_result, RMIN, maskpt_rmin[0], maskpt_rmin[1])
        if RMAX > 0.0:
            SANSUtility.MaskOutsideCylinder(final_result, RMAX, maskpt_rmin[0], maskpt_rmin[1])
    else:
        if RMIN > 0.0: 
            SANSUtility.MaskInsideCylinder(final_result, RMIN, maskpt_rmin[0], maskpt_rmin[1])
        if RMAX > 0.0:
            SANSUtility.MaskOutsideCylinder(final_result, RMAX, maskpt_rmax[0], maskpt_rmax[1])
            
    # Mask other requested spectra that are given in the GUI
    speclist = SANSUtility.ConvertToSpecList(SPECMASKSTRING, SPECMIN, DIMENSION)
    # Spectrum mask
    SANSUtility.MaskBySpecNumber(final_result, speclist)
    # Time mask
    SANSUtility.MaskByBinRange(final_result, TIMEMASKSTRING)
    ####################################################################################
        
    ######################## Unit change and rebin #####################################
    # Convert all of the files to wavelength and rebin
    # ConvertUnits does have a rebin option, but it's crude. In particular it rebins on linear scale.
    ConvertUnits(monitorWS, monitorWS, "Wavelength")
    wavbin =  str(wav_start) + "," + str(DWAV) + "," + str(wav_end)
    Rebin(monitorWS, monitorWS,wavbin)
    ConvertUnits(final_result,final_result,"Wavelength")
    Rebin(final_result,final_result,wavbin)
    ####################################################################################

    ####################### Correct by incident beam monitor ###########################
    # At this point need to fork off workspace name to keep a workspace containing raw counts
    tmpWS = "reduce_temp_workspace"
    Divide(final_result, monitorWS, tmpWS)
    mantid.deleteWorkspace(monitorWS)
    ###################################################################################

    ############################ Transmission correction ##############################
    trans_ws = CalculateTransmissionCorrection(run_setup, wav_start, wav_end, use_def_trans)
    if trans_ws != None:
        Divide(tmpWS, trans_ws, tmpWS)
    ##################################################################################   
        
    ############################ Efficiency correction ################################
    CorrectToFile(tmpWS, DIRECT_BEAM_FILE, tmpWS, "Wavelength", "Divide")
    ###################################################################################
        
    ############################# Scale by volume #####################################
    SANSUtility.ScaleByVolume(tmpWS, RESCALE, SAMPLE_GEOM, SAMPLE_WIDTH, SAMPLE_HEIGHT, SAMPLE_THICKNESS)
    ################################################## ################################
        
    ################################ Correction in Q space ############################
    # 1D
    if CORRECTION_TYPE == '1D':
        q_bins = str(Q1) + "," + str(DQ) + "," + str(Q2)
        if finding_centre == True:
            GroupIntoQuadrants(tmpWS, final_result, maskpt_rmin[0], maskpt_rmin[1], q_bins)
            return
        else:
            Q1D(tmpWS,final_result,final_result,q_bins, AccountForGravity=GRAVITY)
    # 2D    
    else:
        # Run 2D algorithm
        Qxy(tmpWS, final_result, QXY2, DQXY)

    mantid.deleteWorkspace(tmpWS)
    return
############################# End of Correct function ###################################################

############################ Centre finding functions ###################################################

# These variables keep track of the centre coordinates that have been used so that we can calculate a relative shift of the
# detector
XVAR_PREV = 0.0
YVAR_PREV = 0.0
ITER_NUM = 0
RESIDUE_GRAPH = None
                
# Create a workspace with a quadrant value in it 
def CreateQuadrant(reduced_ws, rawcount_ws, quadrant, xcentre, ycentre, q_bins, output):
    # Need to create a copy because we're going to mask 3/4 out and that's a one-way trip
    CloneWorkspace(reduced_ws,output)
    objxml = SANSUtility.QuadrantXML([xcentre, ycentre, 0.0], RMIN, RMAX, quadrant)
    # Mask out everything outside the quadrant of interest
    MaskDetectorsInShape(output,objxml)
    # Q1D ignores masked spectra/detectors. This is on the InputWorkspace, so we don't need masking of the InputForErrors workspace
    Q1D(output,rawcount_ws,output,q_bins,AccountForGravity=GRAVITY)

    flag_value = -10.0
    ReplaceSpecialValues(InputWorkspace=output,OutputWorkspace=output,NaNValue=flag_value,InfinityValue=flag_value)
    if CORRECTION_TYPE == '1D':
        SANSUtility.StripEndZeroes(output, flag_value)

# Create 4 quadrants for the centre finding algorithm and return their names
def GroupIntoQuadrants(reduced_ws, final_result, xcentre, ycentre, q_bins):
    tmp = 'quad_temp_holder'
    pieces = ['Left', 'Right', 'Up', 'Down']
    to_group = ''
    counter = 0
    for q in pieces:
        counter += 1
        to_group += final_result + '_' + str(counter) + ','
        CreateQuadrant(reduced_ws, final_result, q, xcentre, ycentre, q_bins, final_result + '_' + str(counter))

    # We don't need these now 
    mantid.deleteWorkspace(final_result)                    
    mantid.deleteWorkspace(reduced_ws)
    GroupWorkspaces(final_result, to_group.strip(','))

# Calcluate the sum squared difference of the given workspaces. This assumes that a workspace with
# one spectrum for each of the quadrants. The order should be L,R,U,D.
def CalculateResidue():
    global XVAR_PREV, YVAR_PREV, RESIDUE_GRAPH
    yvalsA = mtd.getMatrixWorkspace('Left').readY(0)
    yvalsB = mtd.getMatrixWorkspace('Right').readY(0)
    qvalsA = mtd.getMatrixWorkspace('Left').readX(0)
    qvalsB = mtd.getMatrixWorkspace('Right').readX(0)
    qrange = [len(yvalsA), len(yvalsB)]
    nvals = min(qrange)
    residueX = 0
    indexB = 0
    for indexA in range(0, nvals):
        if qvalsA[indexA] < qvalsB[indexB]:
            mantid.sendLogMessage("::SANS::LR1 "+str(indexA)+" "+str(indexB))
            continue
        elif qvalsA[indexA] > qvalsB[indexB]:
            while qvalsA[indexA] > qvalsB[indexB]:
                mantid.sendLogMessage("::SANS::LR2 "+str(indexA)+" "+str(indexB))
                indexB += 1
        if indexA > nvals - 1 or indexB > nvals - 1:
            break
        residueX += pow(yvalsA[indexA] - yvalsB[indexB], 2)
        indexB += 1

    yvalsA = mtd.getMatrixWorkspace('Up').readY(0)
    yvalsB = mtd.getMatrixWorkspace('Down').readY(0)
    qvalsA = mtd.getMatrixWorkspace('Up').readX(0)
    qvalsB = mtd.getMatrixWorkspace('Down').readX(0)
    qrange = [len(yvalsA), len(yvalsB)]
    nvals = min(qrange)
    residueY = 0
    indexB = 0
    for indexA in range(0, nvals):
        if qvalsA[indexA] < qvalsB[indexB]:
            mantid.sendLogMessage("::SANS::UD1 "+str(indexA)+" "+str(indexB))
            continue
        elif qvalsA[indexA] > qvalsB[indexB]:
            while qvalsA[indexA] > qvalsB[indexB]:
                mantid.sendLogMessage("::SANS::UD2 "+str(indexA)+" "+str(indexB))
                indexB += 1
        if indexA > nvals - 1 or indexB > nvals - 1:
            break
        residueY += pow(yvalsA[indexA] - yvalsB[indexB], 2)
        indexB += 1
                        
    if RESIDUE_GRAPH == None:
        RESIDUE_GRAPH = plotSpectrum('Left', 0)
        mergePlots(RESIDUE_GRAPH, plotSpectrum('Right', 0))
        mergePlots(RESIDUE_GRAPH, plotSpectrum('Up', 0))
        mergePlots(RESIDUE_GRAPH, plotSpectrum('Down', 0))
        RESIDUE_GRAPH.activeLayer().setCurveTitle(0, 'Left')
        RESIDUE_GRAPH.activeLayer().setCurveTitle(1, 'Right')
        RESIDUE_GRAPH.activeLayer().setCurveTitle(3, 'Up')
        RESIDUE_GRAPH.activeLayer().setCurveTitle(2, 'Down')
	
    RESIDUE_GRAPH.activeLayer().setTitle("Itr " + str(ITER_NUM)+" "+str(XVAR_PREV*1000.)+","+str(YVAR_PREV*1000.)+" SX "+str(residueX)+" SY "+str(residueY))
    mantid.sendLogMessage("::SANS::Itr: "+str(ITER_NUM)+" "+str(XVAR_PREV*1000.)+","+str(YVAR_PREV*1000.)+" SX "+str(residueX)+" SY "+str(residueY))              
    return residueX, residueY
	
def RunReduction(coords):
    '''Compute the value of (L-R)^2+(U-D)^2 a circle split into four quadrants'''
    global XVAR_PREV, YVAR_PREV, RESIDUE_GRAPH
    xcentre = coords[0]
    ycentre= coords[1]
    
    xshift = -xcentre + XVAR_PREV
    yshift = -ycentre + YVAR_PREV
    XVAR_PREV = xcentre
    YVAR_PREV = ycentre

    # Do the correction
    if xshift != 0.0 or yshift != 0.0:
        MoveInstrumentComponent(SCATTER_SAMPLE, ComponentName = DETBANK, X = str(xshift), Y = str(yshift), RelativePosition="1")
        if SCATTER_CAN != '':
            MoveInstrumentComponent(SCATTER_CAN, ComponentName = DETBANK, X = str(xshift), Y = str(yshift), RelativePosition="1")
			
    _SAMPLE_SETUP.setMaskPtMin([0.0,0.0])
    _SAMPLE_SETUP.setMaskPtMax([xcentre, ycentre])
    if _CAN_SETUP != None:
        _CAN_SETUP.setMaskPtMin([0.0, 0.0])
        _CAN_SETUP.setMaskPtMax([xcentre, ycentre])

    WavRangeReduction(WAV1, WAV2, DefaultTrans, finding_centre = True)
    return CalculateResidue()

def FindBeamCentre(rlow, rupp, MaxIter = 10, xstart = None, ystart = None):
    global XVAR_PREV, YVAR_PREV, ITER_NUM, RMIN, RMAX, XBEAM_CENTRE, YBEAM_CENTRE
    RMIN = float(rlow)/1000.
    RMAX = float(rupp)/1000.
	
    if xstart == None or ystart == None:
        XVAR_PREV = XBEAM_CENTRE
        YVAR_PREV = YBEAM_CENTRE
    else:
        XVAR_PREV = xstart
        YVAR_PREV = ystart

    mantid.sendLogMessage("::SANS:: xstart,ystart="+str(XVAR_PREV*1000.)+" "+str(YVAR_PREV*1000.)) 
    _printMessage("Starting centre finding routine ...")
    # Initialize the workspace with the starting coordinates. (Note that this moves the detector to -x,-y)
    _initReduction(XVAR_PREV, YVAR_PREV)
    ITER_NUM = 0
    # Run reduction, returning the X and Y sum-squared difference values 
    _printMessage("Running initial reduction: " + str(XVAR_PREV*1000.)+ "  "+ str(YVAR_PREV*1000.))
    oldX2,oldY2 = RunReduction([XVAR_PREV, YVAR_PREV])
    XSTEP = 5.0/1000.
    YSTEP = 5.0/1000.
    # take first trial step
    XNEW = XVAR_PREV + XSTEP
    YNEW = YVAR_PREV + YSTEP
    for ITER_NUM in range(1, MaxIter+1):
        _printMessage("Iteration " + str(ITER_NUM) + ": " + str(XNEW*1000.)+ "  "+ str(YNEW*1000.))
        newX2,newY2 = RunReduction([XNEW, YNEW])
        if newX2 > oldX2:
            XSTEP = -XSTEP/2.
        if newY2 > oldY2:
            YSTEP = -YSTEP/2.
        if abs(XSTEP) < 0.1251/1000. and abs(YSTEP) < 0.1251/1000. :
            _printMessage("::SANS:: Converged - check if stuck in local minimum!")
            break
        oldX2 = newX2
        oldY2 = newY2
        XNEW += XSTEP
        YNEW += YSTEP
	
    if ITER_NUM == MaxIter:
        _printMessage("::SANS:: Out of iterations, new coordinates may not be the best!")
        XNEW -= XSTEP
        YNEW -= YSTEP

    
    XBEAM_CENTRE = XNEW
    YBEAM_CENTRE = YNEW
    _printMessage("Centre coordinates updated: [" + str(XBEAM_CENTRE*1000.)+ ","+ str(YBEAM_CENTRE*1000.) + ']')
    
    # Reload the sample and can and reset the radius range
    global _SAMPLE_SETUP
    _assignHelper(_SAMPLE_RUN, False)
    _SAMPLE_SETUP = None
    if _CAN_RUN != '':
        _assignHelper(_CAN_RUN, False)
        global _CAN_SETUP
        _CAN_SETUP = None
    
    RMIN = DEF_RMIN
    RMAX = DEF_RMAX    
############################################################################################################################

# These are to work around for the moment
def plotSpectrum(name, spec):
    return qti.app.mantidUI.pyPlotSpectraList([name],[spec])

def mergePlots(g1, g2):
    return qti.app.mantidUI.mergePlots(g1,g2)
