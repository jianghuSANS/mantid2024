// Mantid Repository : https://github.com/mantidproject/mantid
//
// Copyright &copy; 2020 ISIS Rutherford Appleton Laboratory UKRI,
//   NScD Oak Ridge National Laboratory, European Spallation Source,
//   Institut Laue - Langevin & CSNS, Institute of High Energy Physics, CAS
// SPDX - License - Identifier: GPL - 3.0 +
#pragma once
#include "MantidAPI/Workspace_fwd.h"
#include "MantidDataObjects/TableWorkspace.h"
#include "MantidDataObjects/Workspace2D.h"
#include "MantidGeometry/IDTypes.h"
#include "MantidKernel/Logger.h"
#include "MantidNexus/NexusClasses.h"

namespace Mantid {
namespace DataHandling {

class DLLExport LoadMuonStrategy {
public:
  // Constructor
  LoadMuonStrategy(Kernel::Logger &g_log, const std::string &filename);
  // Virtual destructor
  virtual ~LoadMuonStrategy() {}
  // Load muon log data
  virtual void loadMuonLogData() = 0;
  // Returns the good frames from the nexus entry
  virtual void loadGoodFrames() = 0;
  // Apply time zero correction
  virtual void applyTimeZeroCorrection() = 0;
  // Load detector grouping
  virtual API::Workspace_sptr loadDetectorGrouping() const = 0;
  // Load dead time table
  virtual API::Workspace_sptr loadDeadTimeTable() const = 0;

protected:
  // Create grouping table
  DataObjects::TableWorkspace_sptr
  createDetectorGroupingTable(const std::vector<detid_t> &specToLoad,
                              const std::vector<detid_t> &grouping) const;
  // Create deadtimes table
  DataObjects::TableWorkspace_sptr
  createDeadTimeTable(const std::vector<detid_t> &detectorsLoaded,
                      const std::vector<double> &deadTimes) const;
  // Logger
  Kernel::Logger &m_logger;
  // Filename, used for running child algorithms
  const std::string &m_filename;
};
} // namespace DataHandling
} // namespace Mantid
