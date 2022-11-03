// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "MantidQtWidgets/InstrumentView/BaseCustomInstrumentModel.h"
#include "MantidAPI/Algorithm.h"
#include "MantidAPI/AlgorithmManager.h"
#include "MantidAPI/AnalysisDataService.h"
#include "MantidAPI/MatrixWorkspace.h"
#include "MantidAPI/NumericAxis.h"
#include "MantidGeometry/Instrument.h"
#include "MantidKernel/Unit.h"

#include <utility>

namespace {
const int ERRORCODE = -999;
const std::string EXTRACTEDWS = "extractedTubes_";
const std::string CURVES = "Curves";

} // namespace

using namespace Mantid::API;
namespace MantidQt::MantidWidgets {

BaseCustomInstrumentModel::BaseCustomInstrumentModel()
    : m_currentRun(0), m_tmpName("ALF_tmp"), m_instrumentName("ALF"), m_wsName("ALFData"), m_numberOfTubesInAverage(0) {
}

BaseCustomInstrumentModel::BaseCustomInstrumentModel(std::string tmpName, std::string instrumentName,
                                                     std::string wsName)
    : m_currentRun(0), m_tmpName(std::move(tmpName)), m_instrumentName(std::move(instrumentName)),
      m_wsName(std::move(wsName)), m_numberOfTubesInAverage(0) {}

void BaseCustomInstrumentModel::loadEmptyInstrument() {
  auto alg = Mantid::API::AlgorithmManager::Instance().create("LoadEmptyInstrument");
  alg->initialize();
  alg->setProperty("OutputWorkspace", m_wsName);
  alg->setProperty("InstrumentName", m_instrumentName);
  alg->execute();
}

/*
 * Runs load data alg
 * @param name:: string name for ALF data
 */
void BaseCustomInstrumentModel::loadAlg(const std::string &name) {
  auto alg = AlgorithmManager::Instance().create("Load");
  alg->initialize();
  alg->setProperty("Filename", name);
  alg->setProperty("OutputWorkspace", getTmpName()); // write to tmp ws
  alg->execute();
}

/*
 * Loads data for use in ALFView
 * Loads data, normalise to current and then converts to d spacing
 * @param name:: string name for ALF data
 * @return std::pair<int,std::string>:: the run number and status
 */
std::pair<int, std::string> BaseCustomInstrumentModel::loadData(const std::string &name) {
  loadAlg(name);
  auto ws = AnalysisDataService::Instance().retrieveWS<MatrixWorkspace>(getTmpName());
  int runNumber = ws->getRunNumber();
  std::string message = "success";
  auto bools = isDataValid();
  if (bools["IsValidInstrument"]) {
    rename();
    m_numberOfTubesInAverage = 0;
  } else {
    // reset to the previous data
    message = "Not the correct instrument, expected " + getInstrument();
    remove();
  }
  if (bools["IsValidInstrument"] && !bools["IsItDSpace"]) {
    transformData();
  }
  return std::make_pair(runNumber, message);
}

void BaseCustomInstrumentModel::averageTube() {
  const std::string name = getInstrument() + std::to_string(getCurrentRun());
  const int oldTotalNumber = m_numberOfTubesInAverage;
  // multiply up current average
  auto ws = AnalysisDataService::Instance().retrieveWS<MatrixWorkspace>(EXTRACTEDWS + name);
  ws *= double(oldTotalNumber);

  // get the data to add
  storeSingleTube(name);
  // rebin to match
  auto rebin = AlgorithmManager::Instance().create("RebinToWorkspace");
  rebin->initialize();
  rebin->setProperty("WorkspaceToRebin", EXTRACTEDWS + name);
  rebin->setProperty("WorkspaceToMatch", ws);
  rebin->setProperty("OutputWorkspace", EXTRACTEDWS + name);
  rebin->execute();

  // add together
  auto alg = AlgorithmManager::Instance().create("Plus");
  alg->initialize();
  alg->setProperty("LHSWorkspace", EXTRACTEDWS + name);
  alg->setProperty("RHSWorkspace", ws);
  alg->setProperty("OutputWorkspace", EXTRACTEDWS + name);
  alg->execute();
  // do division
  ws = AnalysisDataService::Instance().retrieveWS<MatrixWorkspace>(EXTRACTEDWS + name);
  ws->mutableY(0) /= (double(oldTotalNumber) + 1.0);
  AnalysisDataService::Instance().addOrReplace(EXTRACTEDWS + name, ws);
  m_numberOfTubesInAverage++;
}

/*
 * Transforms ALF data; normalise to current and then converts to d spacing
 * If already d-space does nothing.
 */
void BaseCustomInstrumentModel::transformData() {
  auto normAlg = AlgorithmManager::Instance().create("NormaliseByCurrent");
  normAlg->initialize();
  normAlg->setProperty("InputWorkspace", getWSName());
  normAlg->setProperty("OutputWorkspace", getWSName());
  normAlg->execute();

  auto dSpacingAlg = AlgorithmManager::Instance().create("ConvertUnits");
  dSpacingAlg->initialize();
  dSpacingAlg->setProperty("InputWorkspace", getWSName());
  dSpacingAlg->setProperty("Target", "dSpacing");
  dSpacingAlg->setProperty("OutputWorkspace", getWSName());
  dSpacingAlg->execute();
}

void BaseCustomInstrumentModel::extractSingleTube() {
  storeSingleTube(getInstrument() + std::to_string(getCurrentRun()));
  m_numberOfTubesInAverage = 1;
}

void BaseCustomInstrumentModel::storeSingleTube(const std::string &name) {
  auto &ads = AnalysisDataService::Instance();
  if (!ads.doesExist(CURVES))
    return;

  const auto scaleFactor = xConversionFactor(ads.retrieveWS<MatrixWorkspace>(CURVES));
  if (!scaleFactor)
    return;

  // Convert to degrees if the XAxis is an angle in radians
  auto alg = AlgorithmManager::Instance().create("ScaleX");
  alg->initialize();
  alg->setProperty("InputWorkspace", CURVES);
  alg->setProperty("OutputWorkspace", EXTRACTEDWS + name);
  alg->setProperty("Factor", *scaleFactor);
  alg->execute();

  auto histogramAlg = AlgorithmManager::Instance().create("ConvertToHistogram");
  histogramAlg->initialize();
  histogramAlg->setProperty("InputWorkspace", EXTRACTEDWS + name);
  histogramAlg->setProperty("OutputWorkspace", EXTRACTEDWS + name);
  histogramAlg->execute();

  ads.remove(CURVES);
}

/*
 * Checks loaded data is from ALF
 * Loads data, normalise to current and then converts to d spacing
 * @return pair<bool,bool>:: If the instrument is ALF, if it is d-spacing
 */
std::map<std::string, bool> BaseCustomInstrumentModel::isDataValid() {
  auto ws = AnalysisDataService::Instance().retrieveWS<MatrixWorkspace>(getTmpName());
  bool isItALF = false;

  if (ws->getInstrument()->getName() == getInstrument()) {
    isItALF = true;
  }
  auto axis = ws->getAxis(0);
  auto unit = axis->unit()->unitID();
  bool isItDSpace = true;
  if (unit != "dSpacing") {
    isItDSpace = false;
  }
  return {{"IsValidInstrument", isItALF}, {"IsItDSpace", isItDSpace}};
}

/*
 * Returns a conversion factor to be used for ScaleX when the x axis unit is an angle measured in radians. If
 * the x axis unit is not 'Phi' or 'Out of angle plane', no scaling is required.
 * @param workspace:: the workspace to check if a conversion factor is required.
 */
std::optional<double> BaseCustomInstrumentModel::xConversionFactor(MatrixWorkspace_const_sptr workspace) const {
  if (!workspace)
    return std::nullopt;

  if (const auto axis = workspace->getAxis(0)) {
    const auto unit = axis->unit()->unitID();
    const auto label = std::string(axis->unit()->label());
    return unit == "Phi" || label == "Out of plane angle" ? 180.0 / M_PI : 1.0;
  }
  return std::nullopt;
}

std::string BaseCustomInstrumentModel::WSName() {
  std::string name = getInstrument() + std::to_string(getCurrentRun());
  return EXTRACTEDWS + name;
}

void BaseCustomInstrumentModel::rename() { AnalysisDataService::Instance().rename(m_tmpName, m_wsName); }
void BaseCustomInstrumentModel::remove() { AnalysisDataService::Instance().remove(m_tmpName); }

std::string BaseCustomInstrumentModel::dataFileName() { return m_wsName; }

int BaseCustomInstrumentModel::currentRun() {
  try {

    auto ws = AnalysisDataService::Instance().retrieveWS<MatrixWorkspace>(m_wsName);
    return ws->getRunNumber();
  } catch (...) {
    return ERRORCODE;
  }
}

bool BaseCustomInstrumentModel::isErrorCode(const int run) { return (run == ERRORCODE); }

bool BaseCustomInstrumentModel::hasTubeBeenExtracted(const std::string &name) {
  return AnalysisDataService::Instance().doesExist(EXTRACTEDWS + name);
}

bool BaseCustomInstrumentModel::averageTubeCondition(std::map<std::string, bool> tabBools) {
  try {

    bool ifCurve = (tabBools.find("plotStored")->second || tabBools.find("hasCurve")->second);
    return (m_numberOfTubesInAverage > 0 && tabBools.find("isTube")->second && ifCurve &&
            hasTubeBeenExtracted(getInstrument() + std::to_string(getCurrentRun())));
  } catch (...) {
    return false;
  }
}

bool BaseCustomInstrumentModel::extractTubeCondition(std::map<std::string, bool> tabBools) {
  try {

    bool ifCurve = (tabBools.find("plotStored")->second || tabBools.find("hasCurve")->second);
    return (tabBools.find("isTube")->second && ifCurve);
  } catch (...) {
    return false;
  }
}

} // namespace MantidQt::MantidWidgets
