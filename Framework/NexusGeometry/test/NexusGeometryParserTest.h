// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//     NScD Oak Ridge National Laboratory, European Spallation Source
//     & Institut Laue - Langevin
// SPDX - License - Identifier: GPL - 3.0 +
#ifndef NEXUSGEOMETRYPARSERTEST_H_
#define NEXUSGEOMETRYPARSERTEST_H_

#include <cxxtest/TestSuite.h>

#include "MantidGeometry/Instrument.h"
#include "MantidGeometry/Instrument/ComponentInfo.h"
#include "MantidGeometry/Instrument/DetectorInfo.h"
#include "MantidGeometry/Objects/CSGObject.h"
#include "MantidGeometry/Objects/MeshObject.h"
#include "MantidGeometry/Objects/MeshObject2D.h"
#include "MantidGeometry/Surfaces/Cylinder.h"
#include "MantidKernel/ConfigService.h"
#include "MantidKernel/EigenConversionHelpers.h"
#include "MantidNexusGeometry/NexusGeometryParser.h"

#include "mockobjects.h"
#include <H5Cpp.h>
#include <Poco/Glob.h>
#include <chrono>
#include <gmock/gmock.h>
#include <string>

using namespace Mantid;
using namespace NexusGeometry;
namespace {
std::unique_ptr<Geometry::DetectorInfo>
extractDetectorInfo(const Mantid::Geometry::Instrument &instrument) {
  Geometry::ParameterMap pmap;
  return std::move(std::get<1>(instrument.makeBeamline(pmap)));
}

std::pair<std::unique_ptr<Geometry::ComponentInfo>,
          std::unique_ptr<Geometry::DetectorInfo>>
extractBeamline(const Mantid::Geometry::Instrument &instrument) {
  Geometry::ParameterMap pmap;
  auto beamline = instrument.makeBeamline(pmap);
  return {std::move(std::get<0>(beamline)), std::move(std::get<1>(beamline))};
}

} // namespace
class NexusGeometryParserTest : public CxxTest::TestSuite {
public:
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static NexusGeometryParserTest *createSuite() {
    return new NexusGeometryParserTest();
  }
  static void destroySuite(NexusGeometryParserTest *suite) { delete suite; }

  std::unique_ptr<const Mantid::Geometry::Instrument> makeTestInstrument() {
    H5std_string nexusFilename = "unit_testing/SMALLFAKE_example_geometry.hdf5";
    const auto fullpath = Kernel::ConfigService::Instance().getFullPath(
        nexusFilename, true, Poco::Glob::GLOB_DEFAULT);

    return NexusGeometryParser::createInstrument(
        fullpath, std::make_unique<MockLogger>());
  }

  void test_pixel_shape_as_mesh() {

    auto instrument = NexusGeometryParser::createInstrument(
        "/home/spu92482/Downloads/DETGEOM_example_1.nxs",
        std::make_unique<testing::NiceMock<MockLogger>>());
    auto beamline = extractBeamline(*instrument);
    auto &compInfo = *beamline.first;
    auto &detInfo = *beamline.second;
    TS_ASSERT_EQUALS(detInfo.size(), 4);
    auto &shape1 = compInfo.shape(0);
    auto &shape2 = compInfo.shape(1);
    auto *shape1Mesh =
        dynamic_cast<const Geometry::MeshObject2D *>(&shape1); // Test detectors
    auto *shape2Mesh = dynamic_cast<const Geometry::MeshObject2D *>(&shape2);
    TS_ASSERT(shape1Mesh);
    TS_ASSERT(shape2Mesh);
    TS_ASSERT_EQUALS(shape1Mesh, shape2Mesh); // pixel shape - all identical.
    TS_ASSERT_EQUALS(shape1Mesh->numberOfTriangles(), 2);
    TS_ASSERT_EQUALS(shape1Mesh->numberOfVertices(), 4);
  }
  void test_pixel_shape_as_cylinders() {
    auto instrument = NexusGeometryParser::createInstrument(
        "/home/spu92482/Downloads/DETGEOM_example_2.nxs",
        std::make_unique<testing::NiceMock<MockLogger>>());
    auto beamline = extractBeamline(*instrument);
    auto &compInfo = *beamline.first;
    auto &detInfo = *beamline.second;
    TS_ASSERT_EQUALS(detInfo.size(), 4);
    auto &shape1 = compInfo.shape(0);
    auto &shape2 = compInfo.shape(1);

    auto *shape1Cylinder =
        dynamic_cast<const Geometry::CSGObject *>(&shape1); // Test detectors
    auto *shape2Cylinder = dynamic_cast<const Geometry::CSGObject *>(&shape2);

    TS_ASSERT(shape1Cylinder);
    TS_ASSERT(shape2Cylinder);
    TS_ASSERT_EQUALS(shape1Cylinder->shapeInfo().radius(), 0.25);
    TS_ASSERT_EQUALS(shape1Cylinder->shapeInfo().height(), 0.5);
    TS_ASSERT_EQUALS(shape1Cylinder->shapeInfo().radius(),
                     shape2Cylinder->shapeInfo().radius());
    TS_ASSERT_EQUALS(shape1Cylinder->shapeInfo().height(),
                     shape2Cylinder->shapeInfo().height());
  }
  void test_detector_shape_as_mesh() {
    auto instrument = NexusGeometryParser::createInstrument(
        "/home/spu92482/Downloads/DETGEOM_example_3.nxs",
        std::make_unique<testing::NiceMock<MockLogger>>());
    auto beamline = extractBeamline(*instrument);
    auto &compInfo = *beamline.first;
    auto &detInfo = *beamline.second;
    TS_ASSERT_EQUALS(detInfo.size(), 4);
    auto &shape1 = compInfo.shape(0);
    auto &shape2 = compInfo.shape(1);
    auto *shape1Mesh =
        dynamic_cast<const Geometry::MeshObject2D *>(&shape1); // Test detectors
    auto *shape2Mesh = dynamic_cast<const Geometry::MeshObject2D *>(&shape2);
    TS_ASSERT(shape1Mesh);
    TS_ASSERT(shape2Mesh);
    TS_ASSERT_EQUALS(shape1Mesh, shape2Mesh); // pixel shape - all identical.
    TS_ASSERT_EQUALS(shape1Mesh->numberOfTriangles(), 1);
    TS_ASSERT_EQUALS(shape1Mesh->numberOfVertices(), 3);
    // auto componentInfo = *beamline.first;
  }
  void test_detector_shape_as_cylinders() {
    auto instrument = NexusGeometryParser::createInstrument(
        "/home/spu92482/Downloads/DETGEOM_example_4.nxs",
        std::make_unique<testing::NiceMock<MockLogger>>());
    auto beamline = extractBeamline(*instrument);

    // auto componentInfo = *beamline.first;
  }
};

class NexusGeometryParserTestPerformance : public CxxTest::TestSuite {
public:
  // This pair of boilerplate methods prevent the suite being created statically
  // This means the constructor isn't called when running other tests
  static NexusGeometryParserTestPerformance *createSuite() {
    return new NexusGeometryParserTestPerformance();
  }

  NexusGeometryParserTestPerformance() {
    m_wishHDF5DefinitionPath = Kernel::ConfigService::Instance().getFullPath(
        "WISH_Definition_10Panels.hdf5", true, Poco::Glob::GLOB_DEFAULT);
    m_sans2dHDF5DefinitionPath = Kernel::ConfigService::Instance().getFullPath(
        "SANS2D_Definition_Tubes.hdf5", true, Poco::Glob::GLOB_DEFAULT);
    m_lokiHDF5DefinitionPath = Kernel::ConfigService::Instance().getFullPath(
        "LOKI_Definition.hdf5", true, Poco::Glob::GLOB_DEFAULT);
  }
  static void destroySuite(NexusGeometryParserTestPerformance *suite) {
    delete suite;
  }

  void test_load_wish() {
    auto start = std::chrono::high_resolution_clock::now();
    auto wishInstrument = NexusGeometryParser::createInstrument(
        m_wishHDF5DefinitionPath, std::make_unique<MockLogger>());
    auto stop = std::chrono::high_resolution_clock::now();
    std::cout << "Creating WISH instrument took: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(stop -
                                                                       start)
                     .count()
              << " ms" << std::endl;
    auto detInfo = extractDetectorInfo(*wishInstrument);
    TS_ASSERT_EQUALS(detInfo->size(), 778245); // Sanity check
  }

  void test_load_sans2d() {
    auto start = std::chrono::high_resolution_clock::now();
    auto sansInstrument = NexusGeometryParser::createInstrument(
        m_sans2dHDF5DefinitionPath, std::make_unique<MockLogger>());
    auto stop = std::chrono::high_resolution_clock::now();
    std::cout << "Creating SANS2D instrument took: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(stop -
                                                                       start)
                     .count()
              << " ms" << std::endl;
    auto detInfo = extractDetectorInfo(*sansInstrument);
    TS_ASSERT_EQUALS(detInfo->size(), 122888); // Sanity check
  }

  void test_load_loki() {
    auto start = std::chrono::high_resolution_clock::now();
    auto sansInstrument = NexusGeometryParser::createInstrument(
        m_lokiHDF5DefinitionPath, std::make_unique<MockLogger>());
    auto stop = std::chrono::high_resolution_clock::now();
    std::cout << "Creating LOKI instrument took: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(stop -
                                                                       start)
                     .count()
              << " ms" << std::endl;

    auto beamline = extractBeamline(*sansInstrument);
    auto componentInfo = std::move(std::get<0>(beamline));
    auto detectorInfo = std::move(std::get<1>(beamline));
    TS_ASSERT_EQUALS(detectorInfo->size(), 8000); // Sanity check

    // Add detectors are described by a meshobject 2d
    auto &shape = componentInfo->shape(0);
    auto *match = dynamic_cast<const Mantid::Geometry::MeshObject2D *>(&shape);
    TS_ASSERT(match);
  }

private:
  std::string m_wishHDF5DefinitionPath;
  std::string m_sans2dHDF5DefinitionPath;
  std::string m_lokiHDF5DefinitionPath;
};
#endif
