// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2019 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "../../../ISISReflectometry/GUI/Common/Decoder.h"
#include "../ReflMockObjects.h"
#include "CoderCommonTester.h"
#include "MantidAPI/FileFinder.h"
#include "MantidPythonInterface/core/WrapPython.h"
#include "MantidQtWidgets/Common/QtJSONUtils.h"

#include <cxxtest/TestSuite.h>

#include <QMap>
#include <QString>
#include <QVariant>

namespace {

const static QString DIR_PATH = QString::fromStdString(FileFinder::Instance().getFullPath("ISISReflectometry"));
const static QString MAINWINDOW_FILE = "mainwindow.json";
const static QString BATCH_FILE = "batch.json";
const static QString EMPTY_BATCH_FILE = "empty_batch.json";
const static QString EIGHT_COL_BATCH_FILE = "8_col_batch.json";
const static QString NINE_COL_BATCH_FILE = "9_col_batch.json";
const static QString TEN_COL_BATCH_FILE = "10_col_batch.json";

} // namespace

namespace MantidQt {
namespace CustomInterfaces {
namespace ISISReflectometry {
class DecoderTest : public CxxTest::TestSuite {
public:
  static DecoderTest *createSuite() { return new DecoderTest(); }
  static void destroySuite(DecoderTest *suite) { delete suite; }

  DecoderTest() {
    PyRun_SimpleString("import mantid.api as api\n"
                       "api.FrameworkManager.Instance()");
  }

  void test_decodeMainWindow() {
    CoderCommonTester tester;
    Decoder decoder;
    auto map = MantidQt::API::loadJSONFromFile(DIR_PATH + MAINWINDOW_FILE);
    auto widget = decoder.decode(map, "");
    tester.testMainWindowView(dynamic_cast<QtMainWindowView *>(widget), map);
  }

  void test_decodeEmptyBatch() {
    CoderCommonTester tester;
    auto map = MantidQt::API::loadJSONFromFile(DIR_PATH + EMPTY_BATCH_FILE);
    QtMainWindowView mwv;
    mwv.initLayout();
    auto gui = dynamic_cast<QtBatchView *>(mwv.batches()[0]);
    Decoder decoder;
    decoder.decodeBatch(&mwv, 0, map);

    tester.testBatch(gui, &mwv, map);
  }

  void test_decodePopulatedBatch() {
    CoderCommonTester tester;
    auto map = MantidQt::API::loadJSONFromFile(DIR_PATH + BATCH_FILE);
    QtMainWindowView mwv;
    mwv.initLayout();
    auto gui = dynamic_cast<QtBatchView *>(mwv.batches()[0]);
    Decoder decoder;
    decoder.decodeBatch(&mwv, 0, map);

    tester.testBatch(gui, &mwv, map);
  }

  void test_decodeBatchWhenInstrumentChanged() {
    CoderCommonTester tester;
    auto map = MantidQt::API::loadJSONFromFile(DIR_PATH + BATCH_FILE);
    QtMainWindowView mwv;
    mwv.initLayout();
    auto gui = dynamic_cast<QtBatchView *>(mwv.batches()[0]);
    // Set the initial instrument to something different to the one we are
    // decoding
    gui->runs()->setSearchInstrument("POLREF");

    Decoder decoder;
    decoder.decodeBatch(&mwv, 0, map);

    tester.testBatch(gui, &mwv, map);
  }

  void test_decodeLegacyTenColBatchFile() {
    QtMainWindowView mwv;
    mwv.initLayout();
    auto gui = dynamic_cast<QtBatchView *>(mwv.batches()[0]);
    Decoder decoder;
    // Decode from the old 9-column format
    auto oldMap = MantidQt::API::loadJSONFromFile(DIR_PATH + TEN_COL_BATCH_FILE);
    decoder.decodeBatch(&mwv, 0, oldMap);

    // Check that the result matches the new format
    QList<QVariant> expectedRowValues{"0.5", "", "13463", "13464", "4", "0.01", "0.1", "0.02", "", "4", "5"};
    CoderCommonTester tester;
    constexpr auto rowIndex = int{0};
    tester.checkPerAngleDefaultsRowEquals(gui, expectedRowValues, rowIndex);
  }

  void test_decodeLegacyNineColBatchFile() {
    QtMainWindowView mwv;
    mwv.initLayout();
    auto gui = dynamic_cast<QtBatchView *>(mwv.batches()[0]);
    Decoder decoder;
    // Decode from the old 9-column format
    auto oldMap = MantidQt::API::loadJSONFromFile(DIR_PATH + NINE_COL_BATCH_FILE);
    decoder.decodeBatch(&mwv, 0, oldMap);

    // Check that the result matches the new format
    QList<QVariant> expectedRowValues{"0.5", "", "13463", "13464", "4", "0.01", "0.1", "0.02", "", "4", ""};
    CoderCommonTester tester;
    constexpr auto rowIndex = int{0};
    tester.checkPerAngleDefaultsRowEquals(gui, expectedRowValues, rowIndex);
  }

  void test_decodeInvalidEightColBatchFile() {
    QtMainWindowView mwv;
    mwv.initLayout();
    Decoder decoder;
    // Decode from the old 9-column format
    auto oldMap = MantidQt::API::loadJSONFromFile(DIR_PATH + EIGHT_COL_BATCH_FILE);
    TS_ASSERT_THROWS(decoder.decodeBatch(&mwv, 0, oldMap), std::out_of_range const &);
  }

  void test_decodeVersionOneFiles() {
    auto map = MantidQt::API::loadJSONFromFile(DIR_PATH + BATCH_FILE);
    Decoder decoder;
    auto constexpr expectedVersion = 1;
    TS_ASSERT_EQUALS(expectedVersion, decoder.decodeVersion(map));
  }

  void test_decodeVersionLegacy() {
    auto map = MantidQt::API::loadJSONFromFile(DIR_PATH + TEN_COL_BATCH_FILE);
    Decoder decoder;
    auto constexpr expectedVersion = 0;
    TS_ASSERT_EQUALS(expectedVersion, decoder.decodeVersion(map));
  }
};
} // namespace ISISReflectometry
} // namespace CustomInterfaces
} // namespace MantidQt
