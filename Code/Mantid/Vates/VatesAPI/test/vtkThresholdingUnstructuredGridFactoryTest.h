#ifndef VTK_THRESHOLDING_UNSTRUCTURED_GRID_FACTORY_TEST_H_
#define VTK_THRESHOLDING_UNSTRUCTURED_GRID_FACTORY_TEST_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <cxxtest/TestSuite.h>
#include "MantidVatesAPI/vtkThresholdingUnstructuredGridFactory.h"
#include "MantidVatesAPI/TimeStepToTimeStep.h"
#include "MantidVatesAPI/UserDefinedThresholdRange.h"
#include "MockObjects.h"
#include "MantidMDEvents/MDHistoWorkspace.h"

using namespace Mantid;
using namespace Mantid::MDEvents;

//=====================================================================================
// Functional Tests
//=====================================================================================
class vtkThresholdingUnstructuredGridFactoryTest: public CxxTest::TestSuite
{

public:

  void testThresholds()
  {
    using namespace Mantid::VATES;
    using namespace Mantid::Geometry;
    using namespace testing;

    // Workspace with value 1.0 everywhere
    MDHistoWorkspace_sptr ws_sptr = getFakeMDHistoWorkspace(1.0, 4);
    ws_sptr->setTransformFromOriginal(new NullTransform);
//    MockIMDWorkspace* pMockWs = new MockIMDWorkspace;
//    EXPECT_CALL(*pMockWs, getSignalNormalizedAt(_, _, _, _)).Times(AtLeast(1)).WillRepeatedly(Return(1));
//    EXPECT_CALL(*pMockWs, getXDimension()).Times(9).WillRepeatedly(Return(IMDDimension_const_sptr(
//      new FakeIMDDimension("x"))));
//    EXPECT_CALL(*pMockWs, getYDimension()).Times(9).WillRepeatedly(Return(IMDDimension_const_sptr(
//      new FakeIMDDimension("y"))));
//    EXPECT_CALL(*pMockWs, getZDimension()).Times(9).WillRepeatedly(Return(IMDDimension_const_sptr(
//      new FakeIMDDimension("z"))));
//    EXPECT_CALL(*pMockWs, getTDimension()).Times(AtLeast(1)).WillRepeatedly(Return(IMDDimension_const_sptr(
//      new FakeIMDDimension("t"))));
//    EXPECT_CALL(*pMockWs, getNonIntegratedDimensions()).Times(6).WillRepeatedly(Return(VecIMDDimension_const_sptr(4)));
//    Mantid::API::IMDWorkspace_sptr ws_sptr(pMockWs);

    //Set up so that only cells with signal values == 1 should not be filtered out by thresholding.

    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> inside(ThresholdRange_scptr(new UserDefinedThresholdRange(0, 2)), "signal", 0);
    inside.initialize(ws_sptr);
    vtkUnstructuredGrid* insideProduct = dynamic_cast<vtkUnstructuredGrid*>(inside.create());

    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> below(ThresholdRange_scptr(new UserDefinedThresholdRange(0, 0.5)),"signal", 0);
    below.initialize(ws_sptr);
    vtkUnstructuredGrid* belowProduct = dynamic_cast<vtkUnstructuredGrid*>(below.create());

    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> above(ThresholdRange_scptr(new UserDefinedThresholdRange(2, 3)), "signal", 0);
    above.initialize(ws_sptr);
    vtkUnstructuredGrid* aboveProduct = dynamic_cast<vtkUnstructuredGrid*>(above.create());

    TS_ASSERT_EQUALS((10*10*10), insideProduct->GetNumberOfCells());
    TS_ASSERT_EQUALS(0, belowProduct->GetNumberOfCells());
    TS_ASSERT_EQUALS(0, aboveProduct->GetNumberOfCells());
  }

  void testSignalAspects()
  {
    using namespace Mantid::VATES;
    using namespace Mantid::Geometry;
    using namespace testing;

    // Workspace with value 1.0 everywhere
    MDHistoWorkspace_sptr ws_sptr = getFakeMDHistoWorkspace(1.0, 4);
    ws_sptr->setTransformFromOriginal(new NullTransform);
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);

    //Constructional method ensures that factory is only suitable for providing mesh information.
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory =
      vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> (ThresholdRange_scptr(pRange), "signal", 0);
    factory.initialize(ws_sptr);

    vtkDataSet* product = factory.create();
    TSM_ASSERT_EQUALS("A single array should be present on the product dataset.", 1, product->GetCellData()->GetNumberOfArrays());
    vtkDataArray* signalData = product->GetCellData()->GetArray(0);
    TSM_ASSERT_EQUALS("The obtained cell data has the wrong name.", std::string("signal"), signalData->GetName());
    const int correctCellNumber = 10*10*10;
    TSM_ASSERT_EQUALS("The number of signal values generated is incorrect.", correctCellNumber, signalData->GetSize());
    product->Delete();
  }

  void testIsValidThrowsWhenNoWorkspace()
  {
    using namespace Mantid::VATES;
    using namespace Mantid::API;

    IMDWorkspace* nullWorkspace = NULL;
    Mantid::API::IMDWorkspace_sptr ws_sptr(nullWorkspace);
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory(ThresholdRange_scptr(pRange), "signal", 1);

    TSM_ASSERT_THROWS("No workspace, so should not be possible to complete initialization.", factory.initialize(ws_sptr), std::runtime_error);
  }

  void testCreateMeshOnlyThrows()
  {
    using namespace Mantid::VATES;
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory(ThresholdRange_scptr(pRange), "signal", 1);
    TS_ASSERT_THROWS(factory.createMeshOnly() , std::runtime_error);
  }

  void testCreateScalarArrayThrows()
  {
    using namespace Mantid::VATES;
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory(ThresholdRange_scptr(pRange), "signal", 1);
    TS_ASSERT_THROWS(factory.createScalarArray() , std::runtime_error);
  }

  void testCreateWithoutInitializeThrows()
  {
    using namespace Mantid::VATES;
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory(ThresholdRange_scptr(pRange), "signal", 1);
    TS_ASSERT_THROWS(factory.create(), std::runtime_error);
  }

  void testInitializationDelegates()
  {
    //If the workspace provided is not a 4D imdworkspace, it should call the successor's initalization
    using namespace Mantid::VATES;
    using namespace Mantid::Geometry;
    using namespace testing;

    MockIMDWorkspace* pMockWs = new MockIMDWorkspace;
    EXPECT_CALL(*pMockWs, getNonIntegratedDimensions()).Times(1).WillOnce(Return(VecIMDDimension_const_sptr(2))); //2 dimensions on the workspace.

    MockvtkDataSetFactory* pMockFactorySuccessor = new MockvtkDataSetFactory;
    EXPECT_CALL(*pMockFactorySuccessor, initialize(_)).Times(1); //expect it then to call initialize on the successor.
    EXPECT_CALL(*pMockFactorySuccessor, getFactoryTypeName()).WillOnce(testing::Return("TypeA")); 

    Mantid::API::IMDWorkspace_sptr ws_sptr(pMockWs);
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);

    //Constructional method ensures that factory is only suitable for providing mesh information.
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory =
      vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> (ThresholdRange_scptr(pRange), "signal", (double)0);

    //Successor is provided.
    factory.SetSuccessor(pMockFactorySuccessor);

    factory.initialize(ws_sptr);

    TSM_ASSERT("Workspace not used as expected", Mock::VerifyAndClearExpectations(pMockWs));
    TSM_ASSERT("successor factory not used as expected.", Mock::VerifyAndClearExpectations(pMockFactorySuccessor));
  }

  void testInitializationDelegatesThrows()
  {
    //If the workspace provided is not a 4D imdworkspace, it should call the successor's initalization. If there is no successor an exception should be thrown.
    using namespace Mantid::VATES;
    using namespace Mantid::Geometry;
    using namespace testing;

    MockIMDWorkspace* pMockWs = new MockIMDWorkspace;
    EXPECT_CALL(*pMockWs, getNonIntegratedDimensions()).Times(1).WillOnce(Return(VecIMDDimension_const_sptr(2))); //2 dimensions on the workspace.

    Mantid::API::IMDWorkspace_sptr ws_sptr(pMockWs);
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);

    //Constructional method ensures that factory is only suitable for providing mesh information.
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory =
      vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> (ThresholdRange_scptr(pRange), "signal", (double)0);

    TSM_ASSERT_THROWS("Should have thrown an execption given that no successor was available.", factory.initialize(ws_sptr), std::runtime_error);
  }

  void testCreateDeleagates()
  {
    //If the workspace provided is not a 4D imdworkspace, it should call the successor's initalization
    using namespace Mantid::VATES;
    using namespace Mantid::Geometry;
    using namespace testing;

    MockIMDWorkspace* pMockWs = new MockIMDWorkspace;
    pMockWs->setTransformFromOriginal(new NullTransform);
    EXPECT_CALL(*pMockWs, getNonIntegratedDimensions()).Times(2).WillRepeatedly(Return(VecIMDDimension_const_sptr(2))); //2 dimensions on the workspace.

    MockvtkDataSetFactory* pMockFactorySuccessor = new MockvtkDataSetFactory;
    EXPECT_CALL(*pMockFactorySuccessor, initialize(_)).Times(1); //expect it then to call initialize on the successor.
    EXPECT_CALL(*pMockFactorySuccessor, create()).Times(1); //expect it then to call create on the successor.
    EXPECT_CALL(*pMockFactorySuccessor, getFactoryTypeName()).WillOnce(testing::Return("TypeA")); 

    Mantid::API::IMDWorkspace_sptr ws_sptr(pMockWs);
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);

    //Constructional method ensures that factory is only suitable for providing mesh information.
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory =
      vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> (ThresholdRange_scptr(pRange), "signal", (double)0);

    //Successor is provided.
    factory.SetSuccessor(pMockFactorySuccessor);

    factory.initialize(ws_sptr);
    factory.create(); // should be called on successor.

    TSM_ASSERT("Workspace not used as expected", Mock::VerifyAndClearExpectations(pMockWs));
    TSM_ASSERT("successor factory not used as expected.", Mock::VerifyAndClearExpectations(pMockFactorySuccessor));
  }

  void testTypeName()
  {
    using namespace Mantid::VATES;

    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100);

    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory =
      vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> (ThresholdRange_scptr(pRange), "signal", (double)0);
    TS_ASSERT_EQUALS("vtkThresholdingUnstructuredGridFactory", factory.getFactoryTypeName());
  }

};

//=====================================================================================
// Performance Tests
//=====================================================================================
class vtkThresholdingUnstructuredGridFactoryTestPerformance : public CxxTest::TestSuite
{
private:

  Mantid::API::IMDWorkspace_sptr m_ws_sptr;

public:

  void setUp()
  {
    using namespace Mantid::VATES;
    using namespace Mantid::Geometry;
    using namespace testing;

    //Create a 4D workspace 50 ^ 4
    m_ws_sptr = getFakeMDHistoWorkspace(1.0, 4, 50);
    m_ws_sptr->setTransformFromOriginal(new NullTransform);

//    MockIMDWorkspace* pMockWs = new MockIMDWorkspace;
//    EXPECT_CALL(*pMockWs, getSignalNormalizedAt(_, _, _, _)).Times(AtLeast(1)).WillRepeatedly(Return(1));
//    EXPECT_CALL(*pMockWs, getXDimension()).WillRepeatedly(Return(IMDDimension_const_sptr(new FakeIMDDimension("x", 20))));
//    EXPECT_CALL(*pMockWs, getYDimension()).WillRepeatedly(Return(IMDDimension_const_sptr(new FakeIMDDimension("y", 20))));
//    EXPECT_CALL(*pMockWs, getZDimension()).WillRepeatedly(Return(IMDDimension_const_sptr(new FakeIMDDimension("z", 20))));
//    EXPECT_CALL(*pMockWs, getTDimension()).WillRepeatedly(Return(IMDDimension_const_sptr(new FakeIMDDimension("t", 20))));
//    EXPECT_CALL(*pMockWs, getNonIntegratedDimensions()).WillRepeatedly(Return(VecIMDDimension_const_sptr(4)));
//    m_ws_sptr = Mantid::API::IMDWorkspace_sptr(pMockWs);
  }

  void testGenerateVTKDataSet()
  {
    using namespace Mantid::VATES;
    UserDefinedThresholdRange* pRange = new UserDefinedThresholdRange(0, 100000);
    vtkThresholdingUnstructuredGridFactory<TimeStepToTimeStep> factory(ThresholdRange_scptr(pRange), "signal", 0);
    factory.initialize(m_ws_sptr);
    TS_ASSERT_THROWS_NOTHING(factory.create());
  }

};


#endif
