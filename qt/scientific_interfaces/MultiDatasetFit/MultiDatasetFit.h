// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2018 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once

#include "DllConfig.h"
#include "MantidKernel/Statistics.h"
#include "MantidQtWidgets/Common/UserSubWindow.h"
#include "ui_MultiDatasetFit.h"

namespace Mantid {
namespace API {
class IFunction;
class IAlgorithm;
class MatrixWorkspace;
} // namespace API
} // namespace Mantid

namespace MantidQt {

// Forward declarations
namespace MantidWidgets {
class FunctionBrowser;
class FitOptionsBrowser;
} // namespace MantidWidgets
namespace API {
class AlgorithmRunner;
}

namespace CustomInterfaces {

// Forward declarations
namespace MDF {
class DataController;
class PlotController;
} // namespace MDF

/**
 * Class MultiDatasetFitDialog implements a dialog for setting up a
 * multi-dataset fit
 * and displaying the results.
 */
class MANTIDQT_MULTIDATASETFIT_DLL MultiDatasetFit : public API::UserSubWindow {
  Q_OBJECT
public:
  /// The name of the interface as registered into the factory
  static std::string name() { return "Multi dataset fitting"; }
  // This interface's categories.
  static QString categoryInfo() { return "General"; }
  /// Constructor
  MultiDatasetFit(QWidget *parent = nullptr);
  /// Destructor
  ~MultiDatasetFit() override;
  /// Get the name of the output matrix workspace for the i-th spectrum
  QString getOutputWorkspaceName(int i) const;
  /// Workspace name for the i-th spectrum
  QString getWorkspaceName(int i) const;
  /// Workspace index of the i-th spectrum
  int getWorkspaceIndex(int i) const;
  /// Get the fitting range for the i-th spectrum
  std::pair<double, double> getFittingRange(int i) const;
  /// Total number of spectra (datasets).
  int getNumberOfSpectra() const;
  /// Display info about the plot.
  void showPlotInfo();
  /// Check that the data sets in the table are valid
  void checkSpectra();
  /// Get value of a local parameter
  double getLocalParameterValue(const QString &parName, int i) const;
  /// Set value of a local parameter
  void setLocalParameterValue(const QString &parName, int i, double value);
  /// Check if a local parameter is fixed
  bool isLocalParameterFixed(const QString &parName, int i) const;
  /// Fix/unfix local parameter
  void setLocalParameterFixed(const QString &parName, int i, bool fixed);
  /// Get the tie for a local parameter.
  QString getLocalParameterTie(const QString &parName, int i) const;
  /// Set a tie for a local parameter.
  void setLocalParameterTie(const QString &parName, int i, QString tie);
  /// Log a warning
  static void logWarning(const std::string &msg);

  /// Make it public
  using API::UserSubWindow::runPythonCode;

public slots:
  void reset();

private slots:
  void fit();
  void finishFit(bool /*error*/);
  void enableZoom();
  void enablePan();
  void enableRange();
  void exportCurrentPlot();
  void exportAllPlots();
  void checkFittingType();
  void setLogNames();
  void setParameterNamesForPlotting();
  void invalidateOutput();
  void updateGuessFunction(const QString & /*unused*/,
                           const QString & /*unused*/);

protected:
  void initLayout() override;

private:
  void createPlotToolbar();
  boost::shared_ptr<Mantid::API::IFunction> createFunction() const;
  void updateParameters(const Mantid::API::IFunction &fun);
  void showInfo(const QString &text);
  bool eventFilter(QObject *widget, QEvent *evn) override;
  void showFunctionBrowserInfo();
  void showFitOptionsBrowserInfo();
  void showTableInfo();
  void removeSpectra(QList<int> rows);
  void loadSettings();
  void saveSettings() const;
  void fitSequential();
  void fitSimultaneous();
  void removeOldOutput();
  void showParameterPlot();
  void setFitStatusInfo(const QString &status, const QString &chiSquared);
  void clearFitStatusInfo();

  /// The form generated by Qt Designer
  Ui::MultiDatasetFit m_uiForm;
  /// Controls the plot and plotted data.
  MDF::PlotController *m_plotController;
  /// Contains all logic of dealing with data sets.
  MDF::DataController *m_dataController;
  /// Function editor
  MantidWidgets::FunctionBrowser *m_functionBrowser;
  /// Browser for setting other Fit properties
  MantidWidgets::FitOptionsBrowser *m_fitOptionsBrowser;
  /// Name of the output workspace
  QString m_outputWorkspaceName;
  /// Fit algorithm runner
  boost::shared_ptr<API::AlgorithmRunner> m_fitRunner;
  /// Remembers setting for just current session
  int m_fitAllSettings;
  /// Fit output status
  QString m_fitStatus;
};

} // namespace CustomInterfaces
} // namespace MantidQt
