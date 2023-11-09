// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#include "IndirectDataAnalysisConvFitTab.h"
#include "ConvFitDataPresenter.h"
#include "FunctionBrowser/ConvTemplateBrowser.h"

using namespace Mantid::API;

namespace {

std::vector<std::string> CONVFIT_HIDDEN_PROPS = std::vector<std::string>(
    {"CreateOutput", "LogValue", "PassWSIndexToFunction", "OutputWorkspace", "Output", "PeakRadius", "PlotParameter"});

} // namespace

namespace MantidQt::CustomInterfaces::IDA {

IndirectDataAnalysisConvFitTab::IndirectDataAnalysisConvFitTab(QWidget *parent)
    : IndirectDataAnalysisTab(new ConvFitModel, new ConvTemplateBrowser, new ConvFitDataView, CONVFIT_HIDDEN_PROPS,
                              parent) {
  setupFitDataPresenter<ConvFitDataPresenter>();
  setConvolveMembers(true);
}

EstimationDataSelector IndirectDataAnalysisConvFitTab::getEstimationDataSelector() const {
  return [](const Mantid::MantidVec &, const Mantid::MantidVec &,
            const std::pair<double, double>) -> DataForParameterEstimation { return DataForParameterEstimation{}; };
}

} // namespace MantidQt::CustomInterfaces::IDA
