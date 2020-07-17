// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "MantidAPI/Axis.h"
#include "MantidAPI/FileFinder.h"
#include "MantidAPI/FrameworkManager.h"
#include "MantidAPI/Sample.h"
#include "MantidAPI/SpectrumInfo.h"
#include "MantidAlgorithms/ConvertUnits.h"
#include "MantidAlgorithms/MonteCarloAbsorption.h"
#include "MantidAlgorithms/SampleCorrections/IBeamProfile.h"
#include "MantidAlgorithms/SampleCorrections/IMCInteractionVolume.h"
#include "MantidAlgorithms/SampleCorrections/MCInteractionStatistics.h"
#include "MantidDataHandling/LoadBinaryStl.h"
#include "MantidGeometry/Instrument/SampleEnvironment.h"
#include "MantidGeometry/Objects/ShapeFactory.h"
#include "MantidKernel/Material.h"
#include "MantidKernel/PhysicalConstants.h"
#include "MantidKernel/PseudoRandomNumberGenerator.h"
#include "MantidKernel/UnitFactory.h"
#include "MantidKernel/WarningSuppressions.h"

#include <cxxtest/TestSuite.h>
#include <gmock/gmock.h>

#include "MantidTestHelpers/ComponentCreationHelper.h"
#include "MantidTestHelpers/WorkspaceCreationHelper.h"

namespace {
enum class Environment {
  SampleOnly,
  SamplePlusContainer,
  UserBeamSize,
  MeshSamplePlusContainer
};

struct TestWorkspaceDescriptor {
  int nspectra;
  int nbins;
  Environment sampleEnviron;
  unsigned int emode;
  double beamWidth;
  double beamHeight;
};

void addSample(const Mantid::API::MatrixWorkspace_sptr &ws,
               const Environment environment, double beamWidth = 0.,
               double beamHeight = 0.) {
  using namespace Mantid::API;
  using namespace Mantid::Geometry;
  using namespace Mantid::Kernel;
  using namespace Mantid::DataHandling;
  namespace PhysicalConstants = Mantid::PhysicalConstants;

  if (environment == Environment::MeshSamplePlusContainer) {
    std::string samplePath =
        Mantid::API::FileFinder::Instance().getFullPath("PearlSample.stl");
    ScaleUnits scaleType = ScaleUnits::millimetres;
    ReadMaterial::MaterialParameters params;
    params.chemicalSymbol = "V";
    auto binaryStlReader = LoadBinaryStl(samplePath, scaleType, params);
    std::shared_ptr<MeshObject> shape = binaryStlReader.readStl();
    ws->mutableSample().setShape(shape);

    std::string envPath =
        Mantid::API::FileFinder::Instance().getFullPath("PearlEnvironment.stl");
    // set up a uniform material for whole environment here to give simple case
    params.chemicalSymbol = "Ti-Zr";
    params.massDensity = 5.23;
    auto binaryStlReaderEnv = LoadBinaryStl(envPath, scaleType, params);
    std::shared_ptr<MeshObject> environmentShape = binaryStlReaderEnv.readStl();

    auto can = std::make_shared<Container>(environmentShape);
    std::unique_ptr<SampleEnvironment> environment =
        std::make_unique<SampleEnvironment>("PearlEnvironment", can);

    ws->mutableSample().setEnvironment(std::move(environment));
  } else {
    // Define a sample shape
    constexpr double sampleRadius{0.006};
    constexpr double sampleHeight{0.04};
    const V3D sampleBaseCentre{0., -sampleHeight / 2., 0.};
    const V3D yAxis{0., 1., 0.};
    auto sampleShape = ComponentCreationHelper::createCappedCylinder(
        sampleRadius, sampleHeight, sampleBaseCentre, yAxis, "sample-cylinder");
    // And a material assuming it's a CSG Object
    sampleShape->setMaterial(
        Material("Vanadium", PhysicalConstants::getNeutronAtom(23, 0), 0.072));
    ws->mutableSample().setShape(sampleShape);

    if (environment == Environment::SamplePlusContainer) {
      constexpr double containerWallThickness{0.002};
      constexpr double containerInnerRadius{1.2 * sampleHeight};
      constexpr double containerOuterRadius{containerInnerRadius +
                                            containerWallThickness};

      auto canShape = ComponentCreationHelper::createHollowShell(
          containerInnerRadius, containerOuterRadius);
      // Set material assuming it's a CSG Object
      canShape->setMaterial(Material(
          "CanMaterial", PhysicalConstants::getNeutronAtom(26, 0), 0.01));
      auto can = std::make_shared<Container>(canShape);
      ws->mutableSample().setEnvironment(
          std::make_unique<SampleEnvironment>("can", can));
    } else if (environment == Environment::UserBeamSize) {
      auto inst = ws->getInstrument();
      auto &pmap = ws->instrumentParameters();
      auto source = inst->getSource();
      pmap.addDouble(source->getComponentID(), "beam-width", beamWidth);
      pmap.addDouble(source->getComponentID(), "beam-height", beamHeight);
    }
  }
}

Mantid::API::MatrixWorkspace_sptr
setUpWS(const TestWorkspaceDescriptor &wsProps) {
  using namespace Mantid::Kernel;

  auto space = WorkspaceCreationHelper::create2DWorkspaceWithFullInstrument(
      wsProps.nspectra, wsProps.nbins);
  // Needs to have units of wavelength
  space->getAxis(0)->unit() = UnitFactory::Instance().create("Wavelength");
  auto inst = space->getInstrument();
  auto &pmap = space->instrumentParameters();

  if (wsProps.emode == DeltaEMode::Direct) {
    pmap.addString(inst.get(), "deltaE-mode", "Direct");
    const double efixed(12.0);
    space->mutableRun().addProperty<double>("Ei", efixed);
  } else if (wsProps.emode == DeltaEMode::Indirect) {
    const double efixed(1.845);
    pmap.addString(inst.get(), "deltaE-mode", "Indirect");
    pmap.addDouble(inst.get(), "Efixed", efixed);
  }

  addSample(space, wsProps.sampleEnviron, wsProps.beamWidth,
            wsProps.beamHeight);
  return space;
}
} // namespace

class MonteCarloAbsorptionTest : public CxxTest::TestSuite {
public:
  //---------------------------------------------------------------------------
  // Success cases
  //---------------------------------------------------------------------------
  void test_Workspace_With_Just_Sample_For_Elastic() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        5, 10, Environment::SampleOnly, DeltaEMode::Elastic, -1, -1};
    auto outputWS = runAlgorithm(wsProps);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    const size_t middle_index(4);

    TS_ASSERT_DELTA(0.6243, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.2829, outputWS->y(0)[middle_index], delta);
    TS_ASSERT_DELTA(0.1110, outputWS->y(0).back(), delta);
    TS_ASSERT_DELTA(0.6280, outputWS->y(2).front(), delta);
    TS_ASSERT_DELTA(0.2892, outputWS->y(2)[middle_index], delta);
    TS_ASSERT_DELTA(0.1178, outputWS->y(2).back(), delta);
    TS_ASSERT_DELTA(0.6265, outputWS->y(4).front(), delta);
    TS_ASSERT_DELTA(0.2864, outputWS->y(4)[middle_index], delta);
    TS_ASSERT_DELTA(0.1143, outputWS->y(4).back(), delta);
  }

  void test_Workspace_With_Just_Sample_For_Direct() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::SampleOnly, DeltaEMode::Direct, -1, -1};
    auto outputWS = runAlgorithm(wsProps);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    const size_t middle_index(4);

    TS_ASSERT_DELTA(0.5061, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.3434, outputWS->y(0)[middle_index], delta);
    TS_ASSERT_DELTA(0.2292, outputWS->y(0).back(), delta);
  }

  void test_Workspace_With_Just_Sample_For_Indirect() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::SampleOnly, DeltaEMode::Indirect, -1, -1};
    auto outputWS = runAlgorithm(wsProps);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    const size_t middle_index(4);

    TS_ASSERT_DELTA(0.3652, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.2326, outputWS->y(0)[middle_index], delta);
    TS_ASSERT_DELTA(0.1448, outputWS->y(0).back(), delta);
  }

  void test_Workspace_With_Sample_And_Container() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::SamplePlusContainer, DeltaEMode::Elastic, -1, -1};
    auto outputWS = runAlgorithm(wsProps);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    const size_t middle_index(4);

    TS_ASSERT_DELTA(0.5995, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.2713, outputWS->y(0)[middle_index], delta);
    TS_ASSERT_DELTA(0.1072, outputWS->y(0).back(), delta);
  }

  void test_Workspace_Beam_Size_Set() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::UserBeamSize, DeltaEMode::Elastic, 0.018, 0.015};
    auto outputWS = runAlgorithm(wsProps);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    const size_t middle_index(4);
    TS_ASSERT_DELTA(0.6243, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.2829, outputWS->y(0)[middle_index], delta);
    TS_ASSERT_DELTA(0.1110, outputWS->y(0).back(), delta);
  }

  void test_Linear_Wavelength_Interpolation() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::SampleOnly, DeltaEMode::Elastic, -1, -1};
    const int nlambda(5);
    const std::string interpolation("Linear");
    auto outputWS = runAlgorithm(wsProps, true, nlambda, interpolation);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    TS_ASSERT_DELTA(0.6221, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.3455, outputWS->y(0)[3], delta);
    TS_ASSERT_DELTA(0.2725, outputWS->y(0)[4], delta);
    TS_ASSERT_DELTA(0.1121, outputWS->y(0).back(), delta);
  }

  void test_CSpline_Wavelength_Interpolation() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::SampleOnly, DeltaEMode::Elastic, -1, -1};
    const int nlambda(5);
    const std::string interpolation("CSpline");
    auto outputWS = runAlgorithm(wsProps, true, nlambda, interpolation);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    TS_ASSERT_DELTA(0.6221, outputWS->y(0).front(), delta);
    // Interpolation gives some negative value due to test setup
    TS_ASSERT_DELTA(0.3373, outputWS->y(0)[3], delta);
    TS_ASSERT_DELTA(0.2725, outputWS->y(0)[4], delta);
    TS_ASSERT_DELTA(0.1121, outputWS->y(0).back(), delta);
  }

  void test_Workspace_With_Different_Lambda_Ranges() {
    using namespace Mantid::API;

    // create an instrument including some monitors so that there's a good
    // variation in the wavelength range of the spectra when convert from TOF to
    // wavelength
    MatrixWorkspace_sptr testWS =
        WorkspaceCreationHelper::create2DWorkspaceWithFullInstrument(10, 100,
                                                                     true);
    testWS->getAxis(0)->unit() =
        Mantid::Kernel::UnitFactory::Instance().create("TOF");

    Mantid::Algorithms::ConvertUnits convert;
    convert.initialize();
    convert.setChild(true);
    convert.setProperty("InputWorkspace", testWS);
    convert.setProperty("Target", "Wavelength");
    convert.setProperty("OutputWorkspace", "dummy");
    convert.execute();
    testWS = convert.getProperty("OutputWorkspace");

    auto mcAbsorb = createAlgorithm();
    addSample(testWS, Environment::SampleOnly);
    TS_ASSERT_THROWS_NOTHING(mcAbsorb->setProperty("InputWorkspace", testWS));
    TS_ASSERT_THROWS_NOTHING(mcAbsorb->execute());
  }

  void test_ignore_masked_spectra() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        5, 10, Environment::SampleOnly, DeltaEMode::Elastic, -1, -1};
    auto testWS = setUpWS(wsProps);
    testWS->mutableSpectrumInfo().setMasked(0, true);
    auto mcAbsorb = createAlgorithm();
    TS_ASSERT_THROWS_NOTHING(mcAbsorb->setProperty("InputWorkspace", testWS));
    TS_ASSERT_THROWS_NOTHING(mcAbsorb->execute());
    auto outputWS = getOutputWorkspace(mcAbsorb);
    // should still output a spectra but it should also be masked and equal to
    // zero
    TS_ASSERT_EQUALS(outputWS->getNumberHistograms(), 5);
    TS_ASSERT_EQUALS(outputWS->spectrumInfo().isMasked(0), true);
    auto yData = outputWS->getSpectrum(0).dataY();
    bool allZero = std::all_of(yData.begin(), yData.end(),
                               [](double i) { return i == 0; });
    TS_ASSERT_EQUALS(allZero, true);
  }

  //---------------------------------------------------------------------------
  // Failure cases
  //---------------------------------------------------------------------------
  void test_Workspace_With_No_Instrument_Is_Not_Accepted() {
    using namespace Mantid::API;

    auto mcAbsorb = createAlgorithm();
    // Create a simple test workspace that has no instrument
    auto testWS = WorkspaceCreationHelper::create2DWorkspace(1, 1);

    TS_ASSERT_THROWS(mcAbsorb->setProperty("InputWorkspace", testWS),
                     const std::invalid_argument &);
  }

  void test_Workspace_With_An_Invalid_Sample_Shape_Is_Not_Accepted() {
    using namespace Mantid::API;

    auto testWS =
        WorkspaceCreationHelper::create2DWorkspaceWithFullInstrument(1, 1);
    // Needs to have units of wavelength
    testWS->getAxis(0)->unit() =
        Mantid::Kernel::UnitFactory::Instance().create("Wavelength");
    auto mcabs = createAlgorithm();
    TS_ASSERT_THROWS_NOTHING(mcabs->setProperty("InputWorkspace", testWS));
    TS_ASSERT_THROWS(mcabs->execute(), const std::invalid_argument &);
  }

  void test_Lower_Limit_for_Number_of_Wavelengths() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::SampleOnly, DeltaEMode::Direct, -1, -1};
    int nlambda{1};
    TS_ASSERT_THROWS(runAlgorithm(wsProps, true, nlambda, "Linear"),
                     const std::runtime_error &)
    nlambda = 2;
    TS_ASSERT_THROWS(runAlgorithm(wsProps, true, nlambda, "CSpline"),
                     const std::runtime_error &)
  }

  void test_event_workspace() {
    auto inputWS =
        WorkspaceCreationHelper::createEventWorkspaceWithFullInstrument(5, 2,
                                                                        true);
    inputWS->getAxis(0)->unit() =
        Mantid::Kernel::UnitFactory::Instance().create("Wavelength");
    addSample(inputWS, Environment::SampleOnly);

    auto mcabs = createAlgorithm();
    TS_ASSERT_THROWS_NOTHING(mcabs->setProperty("InputWorkspace", inputWS));
    TS_ASSERT(mcabs->execute());
    // only checking that it can successfully execute
  }

  void test_Sparse_Instrument_For_Elastic() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        5, 10, Environment::SampleOnly, DeltaEMode::Elastic, -1, -1};
    auto outputWS = runAlgorithm(wsProps, false, -1, "Linear", true, 3, 3);

    verifyDimensions(wsProps, outputWS);
    const double delta{1e-04};
    const size_t middle_index{4};
    TS_ASSERT_DELTA(0.6239, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.2823, outputWS->y(0)[middle_index], delta);
    TS_ASSERT_DELTA(0.1105, outputWS->y(0).back(), delta);
    TS_ASSERT_DELTA(0.6264, outputWS->y(2).front(), delta);
    TS_ASSERT_DELTA(0.2864, outputWS->y(2)[middle_index], delta);
    TS_ASSERT_DELTA(0.1147, outputWS->y(2).back(), delta);
    TS_ASSERT_DELTA(0.6259, outputWS->y(4).front(), delta);
    TS_ASSERT_DELTA(0.2853, outputWS->y(4)[middle_index], delta);
    TS_ASSERT_DELTA(0.1132, outputWS->y(4).back(), delta);
  }

  void test_Sparse_Instrument_For_Direct() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::SampleOnly, DeltaEMode::Direct, -1, -1};
    auto outputWS = runAlgorithm(wsProps, false, -1, "Linear", true, 3, 3);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    const size_t middle_index(4);

    TS_ASSERT_DELTA(0.5056, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.3429, outputWS->y(0)[middle_index], delta);
    TS_ASSERT_DELTA(0.2286, outputWS->y(0).back(), delta);
  }

  void test_Sparse_Instrument_For_Indirect() {
    using Mantid::Kernel::DeltaEMode;
    TestWorkspaceDescriptor wsProps = {
        1, 10, Environment::SampleOnly, DeltaEMode::Indirect, -1, -1};
    auto outputWS = runAlgorithm(wsProps, false, -1, "Linear", true, 3, 3);

    verifyDimensions(wsProps, outputWS);
    const double delta(1e-04);
    const size_t middle_index(4);

    TS_ASSERT_DELTA(0.3646, outputWS->y(0).front(), delta);
    TS_ASSERT_DELTA(0.2321, outputWS->y(0)[middle_index], delta);
    TS_ASSERT_DELTA(0.1443, outputWS->y(0).back(), delta);
  }

private:
  class MockMCAbsorptionStrategy final
      : public Mantid::Algorithms::MCAbsorptionStrategy {
  public:
    MockMCAbsorptionStrategy(
        Mantid::Algorithms::IMCInteractionVolume &interactionVolume,
        const Mantid::Algorithms::IBeamProfile &beamProfile,
        Mantid::Kernel::DeltaEMode::Type EMode, const size_t nevents,
        const size_t maxScatterPtAttempts,
        const bool regenerateTracksForEachLambda)
        : MCAbsorptionStrategy(interactionVolume, beamProfile, EMode, nevents,
                               maxScatterPtAttempts,
                               regenerateTracksForEachLambda){};

  public:
    GNU_DIAG_OFF_SUGGEST_OVERRIDE
    MOCK_METHOD7(calculate,
                 void(Mantid::Kernel::PseudoRandomNumberGenerator &rng,
                      const Mantid::Kernel::V3D &finalPos,
                      const std::vector<double> &lambdas,
                      const double lambdaFixed,
                      std::vector<double> &attenuationFactors,
                      std::vector<double> &attFactorErrors,
                      Mantid::Algorithms::MCInteractionStatistics &stats));
    GNU_DIAG_ON_SUGGEST_OVERRIDE
  };
  class MockSparseWorkspace final : public Mantid::Algorithms::SparseWorkspace {
  public:
    MockSparseWorkspace(const Mantid::API::MatrixWorkspace &modelWS,
                        const size_t wavelengthPoints, const size_t rows,
                        const size_t columns)
        : SparseWorkspace(modelWS, wavelengthPoints, rows, columns){};

  public:
    GNU_DIAG_OFF_SUGGEST_OVERRIDE
    MOCK_CONST_METHOD2(interpolateFromDetectorGrid,
                       Mantid::HistogramData::Histogram(const double lat,
                                                        const double lon));
    GNU_DIAG_ON_SUGGEST_OVERRIDE
  };
  class TestMonteCarloAbsorption final
      : public Mantid::Algorithms::MonteCarloAbsorption {
    std::unique_ptr<Mantid::Algorithms::MCAbsorptionStrategy> createStrategy(
        const Mantid::Algorithms::IBeamProfile &beamProfile,
        const Mantid::API::Sample &sample,
        Mantid::Kernel::DeltaEMode::Type EMode, const size_t nevents,
        const size_t maxScatterPtAttempts,
        const bool regenerateTracksForEachLambda,
        const Mantid::Algorithms::MCInteractionVolume::ScatteringPointVicinity
            pointsIn) override {
      Mantid::Algorithms::MCInteractionVolume interactionVol(
          sample, beamProfile.defineActiveRegion(sample), maxScatterPtAttempts,
          pointsIn);
      return std::make_unique<MockMCAbsorptionStrategy>(
          interactionVol, beamProfile, EMode, nevents, maxScatterPtAttempts,
          regenerateTracksForEachLambda);
    }
    std::unique_ptr<Mantid::Algorithms::SparseWorkspace>
    createSparseWorkspace(const Mantid::API::MatrixWorkspace &modelWS,
                          const size_t wavelengthPoints, const size_t rows,
                          const size_t columns) {
      return std::make_unique<MockSparseWorkspace>(modelWS, wavelengthPoints,
                                                   rows, columns);
    }
  };
  Mantid::API::MatrixWorkspace_const_sptr
  runAlgorithm(const TestWorkspaceDescriptor &wsProps,
               const bool resimulateTracksForDiffWavelengths = false,
               int nlambda = -1, const std::string &interpolate = "",
               const bool sparseInstrument = false, const int sparseRows = 2,
               const int sparseColumns = 2) {
    auto inputWS = setUpWS(wsProps);
    auto mcabs = createAlgorithm();
    TS_ASSERT_THROWS_NOTHING(mcabs->setProperty("InputWorkspace", inputWS));
    if (resimulateTracksForDiffWavelengths) {
      TS_ASSERT_THROWS_NOTHING(
          mcabs->setProperty("ResimulateTracksForDifferentWavelengths", true));
      if (nlambda > 0) {
        TS_ASSERT_THROWS_NOTHING(
            mcabs->setProperty("NumberOfWavelengthPoints", nlambda));
      }
    }
    if (!interpolate.empty()) {
      TS_ASSERT_THROWS_NOTHING(
          mcabs->setProperty("Interpolation", interpolate));
    }
    if (sparseInstrument) {
      mcabs->setProperty("SparseInstrument", true);
      mcabs->setProperty("NumberOfDetectorRows", sparseRows);
      mcabs->setProperty("NumberOfDetectorColumns", sparseColumns);
    }
    mcabs->execute();
    return getOutputWorkspace(mcabs);
  }

  Mantid::API::IAlgorithm_sptr createAlgorithm() {
    using Mantid::Algorithms::MonteCarloAbsorption;
    using Mantid::API::IAlgorithm;
    auto alg = std::make_shared<MonteCarloAbsorption>();
    alg->initialize();
    alg->setRethrows(true);
    alg->setChild(true);
    alg->setProperty("EventsPerPoint", 300);
    alg->setPropertyValue("OutputWorkspace", "__unused_on_child");
    return alg;
  }

  Mantid::API::MatrixWorkspace_const_sptr
  getOutputWorkspace(const Mantid::API::IAlgorithm_sptr &alg) {
    using Mantid::API::MatrixWorkspace_sptr;
    MatrixWorkspace_sptr output = alg->getProperty("OutputWorkspace");
    TS_ASSERT(output);
    if (!output)
      TS_FAIL("Algorithm has not set an output workspace");
    return output;
  }

  void
  verifyDimensions(TestWorkspaceDescriptor wsProps,
                   const Mantid::API::MatrixWorkspace_const_sptr &outputWS) {
    TS_ASSERT_EQUALS(wsProps.nspectra, outputWS->getNumberHistograms());
    TS_ASSERT_EQUALS(wsProps.nbins, outputWS->blocksize());
  }
};

class MonteCarloAbsorptionTestPerformance : public CxxTest::TestSuite {
public:
  static MonteCarloAbsorptionTestPerformance *createSuite() {
    return new MonteCarloAbsorptionTestPerformance();
  }

  static void destroySuite(MonteCarloAbsorptionTestPerformance *suite) {
    delete suite;
  }

  MonteCarloAbsorptionTestPerformance() {
    TestWorkspaceDescriptor wsProps = {
        10, 700, Environment::SampleOnly, Mantid::Kernel::DeltaEMode::Elastic,
        -1, -1};

    inputElastic = setUpWS(wsProps);

    wsProps.emode = Mantid::Kernel::DeltaEMode::Direct;
    inputDirect = setUpWS(wsProps);

    wsProps.emode = Mantid::Kernel::DeltaEMode::Indirect;
    inputIndirect = setUpWS(wsProps);

    wsProps.sampleEnviron = Environment::MeshSamplePlusContainer;
    wsProps.emode = Mantid::Kernel::DeltaEMode::Elastic;
    inputElasticMesh = setUpWS(wsProps);
  }

  void test_exec_sample_elastic() {
    Mantid::Algorithms::MonteCarloAbsorption alg;
    alg.initialize();
    alg.setProperty("InputWorkspace", inputElastic);
    alg.setProperty("EventsPerPoint", 300);
    alg.setPropertyValue("OutputWorkspace", "__unused_on_child");
    alg.execute();
  }

  void test_exec_sample_direct() {
    Mantid::Algorithms::MonteCarloAbsorption alg;
    alg.initialize();
    alg.setProperty("InputWorkspace", inputDirect);
    alg.setProperty("EventsPerPoint", 300);
    alg.setPropertyValue("OutputWorkspace", "__unused_on_child");
    alg.execute();
  }

  void test_exec_sample_indirect() {
    Mantid::Algorithms::MonteCarloAbsorption alg;
    alg.initialize();
    alg.setProperty("InputWorkspace", inputIndirect);
    alg.setProperty("EventsPerPoint", 300);
    alg.setPropertyValue("OutputWorkspace", "__unused_on_child");
    alg.execute();
  }

  void test_exec_sample_elastic_mesh() {
    Mantid::Algorithms::MonteCarloAbsorption alg;
    alg.initialize();
    alg.setProperty("InputWorkspace", inputElasticMesh);
    alg.setProperty("EventsPerPoint", 100);
    alg.setPropertyValue("OutputWorkspace", "__unused_on_child");
    alg.execute();
  }

private:
  Mantid::API::Workspace_sptr inputElastic;
  Mantid::API::Workspace_sptr inputDirect;
  Mantid::API::Workspace_sptr inputIndirect;
  Mantid::API::Workspace_sptr inputElasticMesh;
};
