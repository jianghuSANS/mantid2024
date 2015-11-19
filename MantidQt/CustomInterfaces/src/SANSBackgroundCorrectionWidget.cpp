#include "MantidQtCustomInterfaces/SANSBackgroundCorrectionWidget.h"
#include "MantidQtCustomInterfaces/SANSBackgroundCorrectionSettings.h"
#include "MantidKernel/Logger.h"
namespace {
  bool convertQtInt(int state) {
    return state == 2 ? true : false;
  }
}

namespace {
    /// static logger for main window
    Mantid::Kernel::Logger g_log("SANSBackgroundCorrectionWidget");
}

namespace MantidQt
{
namespace CustomInterfaces
{
  SANSBackgroundCorrectionWidget::SANSBackgroundCorrectionWidget(QWidget* parent) : QWidget(parent) {
    m_ui.setupUi(this);

    // Disable all inputs initially
    handleTimeDetectorsOnOff(0);
    handleTimeMonitorsOnOff(0);
    handleUampDetectorsOnOff(0);
    handleUampMonitorsOnOff(0);

    // Setup signal slot connections
    setupConnections();
  }

  /**
   * Set the dark run settings for time-based subtractions for detectors
   * @param setting: the dark run settings for time-based subtractions, ie when we want 
   */
  void SANSBackgroundCorrectionWidget::setDarkRunSettingForTimeDetectors(SANSBackgroundCorrectionSettings setting) {
    if (setting.getUseMon()) {
      g_log.warning("SANSBackgroundCorrectionWidget: Trying to pass a background correction "
        "setting of a monitor to a detector display.");
        return;
    }

    m_ui.bckgnd_cor_det_time_use_check_box->setChecked(true);
    m_ui.bckgnd_cor_det_time_run_line_edit->setText(setting.getRunNumber());
    m_ui.bckgnd_cor_det_mean_check_box->setChecked(setting.getUseMean());
  }

  /**
   * Get the dark run settings for time-based subtractions for detectors
   * @returns the dark run settings for time-based subtractions
   */
  SANSBackgroundCorrectionSettings SANSBackgroundCorrectionWidget::getDarkRunSettingForTimeDetectors() {
    QString runNumber("");
    bool useMean = false;
    bool useMon = false;
    QString monNumber("");

    if (m_ui.bckgnd_cor_det_time_use_check_box->isChecked()) {
      runNumber = m_ui.bckgnd_cor_det_time_run_line_edit->text();
      useMean = m_ui.bckgnd_cor_det_mean_check_box->isChecked();
    }
    return SANSBackgroundCorrectionSettings(runNumber, useMean, useMon, monNumber);
  }

  /**
  * Set the dark run settings for uamp-based subtractions for detectors
  * @param setting: the dark run settings for uamp-based subtractions, ie when we want
  */
  void SANSBackgroundCorrectionWidget::setDarkRunSettingForUampDetectors(SANSBackgroundCorrectionSettings setting) {
    if (setting.getUseMon()) {
      g_log.warning("SANSBackgroundCorrectionWidget: Trying to pass a background correction "
        "setting of a monitor to a detector display.");
      return;
    }
    m_ui.bckgnd_cor_det_uamp_use_check_box->setChecked(true);
    m_ui.bckgnd_cor_det_uamp_run_line_edit->setText(setting.getRunNumber());
  }

  /**
  * Get the dark run settings for uamp-based subtractions for detectors
  * @returns the dark run settings for uamp-based subtractions
  */
  SANSBackgroundCorrectionSettings SANSBackgroundCorrectionWidget::getDarkRunSettingForUampDetectors() {
    QString runNumber("");
    bool useMean = false;
    bool useMon = false;
    QString monNumber("");

    if (m_ui.bckgnd_cor_det_uamp_use_check_box->isChecked()) {
      runNumber = m_ui.bckgnd_cor_det_uamp_run_line_edit->text();
    }
    return SANSBackgroundCorrectionSettings(runNumber, useMean, useMon, monNumber);
  }

  /**
  * Set the dark run settings for time-based subtractions for monitors
  * @param setting: the dark run settings for time-based subtractions, ie when we want
  */
  void SANSBackgroundCorrectionWidget::setDarkRunSettingForTimeMonitors(SANSBackgroundCorrectionSettings setting) {
    if (!setting.getUseMon()) {
      g_log.warning("SANSBackgroundCorrectionWidget: Trying to pass a background correction "
        "setting of a detector to a monitor display.");
      return;
    }

    m_ui.bckgnd_cor_mon_time_use_check_box->setChecked(true);
    m_ui.bckgnd_cor_mon_time_run_line_edit->setText(setting.getRunNumber());
    m_ui.bckgnd_cor_mon_mean_check_box->setChecked(setting.getUseMean());
  }

  /**
  * Get the dark run settings for time-based subtractions for detectors
  * @returns the dark run settings for time-based subtractions
  */
  SANSBackgroundCorrectionSettings SANSBackgroundCorrectionWidget::getDarkRunSettingForTimeMonitors() {
    QString runNumber("");
    bool useMean = false;
    bool useMon = false;
    QString monNumber("");

    if (m_ui.bckgnd_cor_mon_time_use_check_box->isChecked()) {
      runNumber = m_ui.bckgnd_cor_mon_time_run_line_edit->text();
      useMean = m_ui.bckgnd_cor_mon_mean_check_box->isChecked();
    }
    return SANSBackgroundCorrectionSettings(runNumber, useMean, useMon, monNumber);
  }

  /**
  * Set the dark run settings for uamp-based subtractions for detectors
  * @param setting: the dark run settings for uamp-based subtractions, ie when we want
  */
  void SANSBackgroundCorrectionWidget::setDarkRunSettingForUampMonitors(SANSBackgroundCorrectionSettings setting) {
    if (!setting.getUseMon()) {
      g_log.warning("SANSBackgroundCorrectionWidget: Trying to pass a background correction "
        "setting of a detector to a monitor display.");
      return;
    }

    m_ui.bckgnd_cor_mon_uamp_use_check_box->setChecked(true);
    m_ui.bckgnd_cor_mon_uamp_run_line_edit->setText(setting.getRunNumber());
  }

  /**
  * Get the dark run settings for uamp-based subtractions for detectors
  * @returns the dark run settings for uamp-based subtractions
  */
  SANSBackgroundCorrectionSettings SANSBackgroundCorrectionWidget::getDarkRunSettingForUampMonitors() {
    QString runNumber("");
    bool useMean = false;
    bool useMon = false;
    QString monNumber("");

    if (m_ui.bckgnd_cor_mon_uamp_use_check_box->isChecked()) {
      runNumber = m_ui.bckgnd_cor_mon_uamp_run_line_edit->text();
    }
    return SANSBackgroundCorrectionSettings(runNumber, useMean, useMon, monNumber);
  }

  void SANSBackgroundCorrectionWidget::setupConnections() {
    QObject::connect(m_ui.bckgnd_cor_det_time_use_check_box, SIGNAL(stateChanged(int)),
      this, SLOT(handleTimeDetectorsOnOff(int)));
    QObject::connect(m_ui.bckgnd_cor_det_uamp_use_check_box, SIGNAL(stateChanged(int)),
      this, SLOT(handleUampDetectorsOnOff(int)));

    QObject::connect(m_ui.bckgnd_cor_mon_time_use_check_box, SIGNAL(stateChanged(int)),
      this, SLOT(handleTimeMonitorsOnOff(int)));
    QObject::connect(m_ui.bckgnd_cor_mon_uamp_use_check_box, SIGNAL(stateChanged(int)),
      this, SLOT(handleUampMonitorsOnOff(int)));
  }

  void SANSBackgroundCorrectionWidget::handleTimeDetectorsOnOff(int stateInt) {
    auto state = convertQtInt(stateInt);
    m_ui.bckgnd_cor_det_time_run_line_edit->setEnabled(state);
    m_ui.bckgnd_cor_det_mean_check_box->setEnabled(state);
  }

  void SANSBackgroundCorrectionWidget::handleUampDetectorsOnOff(int stateInt) {
    auto state = convertQtInt(stateInt);
    m_ui.bckgnd_cor_det_uamp_run_line_edit->setEnabled(state);
  }

  void SANSBackgroundCorrectionWidget::handleTimeMonitorsOnOff(int stateInt) {
    auto state = convertQtInt(stateInt);
    m_ui.bckgnd_cor_mon_time_run_line_edit->setEnabled(state);
    m_ui.bckgnd_cor_mon_mean_check_box->setEnabled(state);
    m_ui.bckgnd_cor_mon_time_mon_num_line_edit->setEnabled(state);
  }

  void SANSBackgroundCorrectionWidget::handleUampMonitorsOnOff(int stateInt) {
    auto state = convertQtInt(stateInt);
    m_ui.bckgnd_cor_mon_uamp_run_line_edit->setEnabled(state);
    m_ui.bckgnd_cor_mon_uamp_mon_num_line_edit->setEnabled(state);
  }
}
}