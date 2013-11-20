#include "MantidQtMantidWidgets/CatalogHelper.h"
#include "MantidQtAPI/AlgorithmDialog.h"
#include "MantidQtAPI/InterfaceManager.h"
#include "MantidKernel/DateAndTime.h"

#include <boost/algorithm/string/regex.hpp>
#include <QCoreApplication>

namespace MantidQt
{
  namespace MantidWidgets
  {

    /**
     * Constructor
     */
    CatalogHelper::CatalogHelper() : m_numberOfResults(0) {}

    /**
     * Obtain the list of instruments from the ICAT Catalog algorithm.
     * @return A vector containing the list of all instruments available.
     */
    const std::vector<std::string> CatalogHelper::getInstrumentList()
    {
      auto catalogAlgorithm = createCatalogAlgorithm("CatalogListInstruments");
      executeAsynchronously(catalogAlgorithm);
      // return the vector containing the list of instruments available.
      return (catalogAlgorithm->getProperty("InstrumentList"));
    }

    /**
     * Obtain the list of investigation types from the ICAT Catalog algorithm.
     * @return A vector containing the list of all investigation types available.
     */
    const std::vector<std::string> CatalogHelper::getInvestigationTypeList()
    {
      auto catalogAlgorithm = createCatalogAlgorithm("CatalogListInvestigationTypes");
      executeAsynchronously(catalogAlgorithm);
      // return the vector containing the list of investigation types available.
      return (catalogAlgorithm->getProperty("InvestigationTypes"));
    }

    /**
     * Search the archive with the user input terms provided and save them to a workspace ("searchResults").
     * @param userInputFields :: A map containing all users' search fields - (key => FieldName, value => FieldValue).
     * @param offset   :: skip this many rows and start returning rows from this point.
     * @param limit    :: limit the number of rows returned by the query.
     */
    void CatalogHelper::executeSearch(const std::map<std::string, std::string> &userInputFields,
        const int &offset, const int &limit)
    {
      auto catalogAlgorithm = createCatalogAlgorithm("CatalogSearch");

      // Set the properties to limit the number of results returned for paging purposes.
      catalogAlgorithm->setProperty("Limit", offset);
      catalogAlgorithm->setProperty("Offset", limit);

      // This will be the workspace where the content of the search result is output to.
      catalogAlgorithm->setProperty("OutputWorkspace", "__searchResults");

      // Iterate over the provided map of user input fields. For each field that isn't empty (e.g. a value was input by the user)
      // then we will set the algorithm property with the key and value of that specific value.
      for (auto it = userInputFields.begin(); it != userInputFields.end(); it++)
      {
        std::string value = it->second;
        // If the user has input any search terms.
        if (!value.empty())
        {
          // Set the property that the search algorithm uses to: (key => FieldName, value => FieldValue) (e.g., (Keywords, bob))
          catalogAlgorithm->setProperty(it->first, value);
        }
      }

      // Allow asynchronous execution to update label while search is being carried out.
      executeAsynchronously(catalogAlgorithm);

      // The number of results to be returned and used by the GUI.
      m_numberOfResults = catalogAlgorithm->getProperty("NumberOfSearchResults");
    }


    /**
     * The number of results returned by the search query
     * (based on values of input fields in executeSearch() above).
     * @return Number of results returned by the search query.
     */
    int64_t CatalogHelper::getNumberOfSearchResults()
    {
      return m_numberOfResults;
    }

    /**
     * Search the archives for all dataFiles related to an "investigation id" then save results to workspace ("dataFileResults").
     * @param investigationId :: The investigation id to use for the search.
     */
    void CatalogHelper::executeGetDataFiles(const int64_t &investigationId)
    {
      auto catalogAlgorithm = createCatalogAlgorithm("CatalogGetDataFiles");

      // Search for all related dataFiles to this investigation id.
      catalogAlgorithm->setProperty("InvestigationId", investigationId);
      // This will be the workspace where the content of the search result is saved to.
      catalogAlgorithm->setPropertyValue("OutputWorkspace","__dataFileResults");

      // Allow asynchronous execution to update label(s) while search is being carried out.
      executeAsynchronously(catalogAlgorithm);
    }

    /**
     * Retrieve the path(s) to the file that was downloaded (via HTTP) or is stored in the archive.
     * @param userSelectedFiles :: The file(s) the user has selected and wants to download.
     * @param downloadPath      :: The location to save the datafile(s).
     * @return A vector containing the paths to the file(s) the user wants.
     */
    const std::vector<std::string> CatalogHelper::downloadDataFiles(const std::vector<std::pair<int64_t, std::string>> &userSelectedFiles,const std::string &downloadPath)
    {
      auto catalogAlgorithm = createCatalogAlgorithm("CatalogDownloadDataFiles");

      // Prepare for the ugly!

      // These two vectors are required by the "CatalogDownloadDataFiles" algorithm.
      std::vector<int64_t> fileIDs;
      std::vector<std::string> fileNames;

      // For each pair in userSelectedFiles we want to add them to their related vector to pass to the algorithm.
      for (auto it = userSelectedFiles.begin(); it != userSelectedFiles.end(); ++it)
      {
        fileIDs.push_back(it->first);
        fileNames.push_back(it->second);
      }

      // End of the ugly!

      // The file IDs and file names of the data file(s) the user wants to download.
      catalogAlgorithm->setProperty("FileIds",fileIDs);
      catalogAlgorithm->setProperty("FileNames",fileNames);
      catalogAlgorithm->setProperty("DownloadPath",downloadPath);

      executeAsynchronously(catalogAlgorithm);
      // Return a vector containing the file paths to the files to download.
      return (catalogAlgorithm->getProperty("FileLocations"));
    }

    /**
     * Validate each input field against the related algorithm property.
     * @param inputFields :: The name of the input field and value of the field (key => "StartDate", value => "00/00/0000").
     * @return The name of the input field(s) marker to update and related error to throw.
     */
    const std::map<std::string, std::string> CatalogHelper::validateProperties(const std::map<std::string, std::string> &inputFields)
    {
      auto catalogAlgorithm = createCatalogAlgorithm("CatalogSearch");

      // Holds the name of the marker to update if an error is found, and the related error message to use.
      // E.g. key => "StartDate_err", value => "The start date for..."
      std::map<std::string, std::string> errors;

      // Validate all input elements in the map.
      for(auto iter = inputFields.begin(); iter != inputFields.end(); ++iter)
      {
        try
        {
          catalogAlgorithm->setProperty(iter->first, iter->second);
        }
        catch (std::invalid_argument&)
        {
          std::string documentation = propertyDocumentation(catalogAlgorithm->getProperties(), iter->first);

          // Add the input name + "_err" (to indicate the error marker in the GUI,
          // rather than the input field) as the key, and the related error as the value.
          errors.insert(std::make_pair(iter->first + "_err", documentation));
        }
      }
      return errors;
    }

    /**
    * Creates a time_t value from an input date ("23/06/2003") for comparison.
    * @param inputDate :: string containing the date.
    * @return time_t value of date
    */
    time_t CatalogHelper::getTimevalue(const std::string& inputDate)
    {
      // Prevent any possible errors.
      if(inputDate.empty()) return 0;
      // A container to hold the segments of the date.
      std::vector<std::string> dateSegments;
      // Split input by "/" prior to rearranging the date
      boost::algorithm::split_regex(dateSegments, inputDate, boost::regex("/"));
      // Reorganise the date to be ISO format.
      std::string isoDate = dateSegments.at(2) + "-" + dateSegments.at(1) + "-" + dateSegments.at(0) + " 0:00:00.000";
      // Return the date as time_t value.
      return Mantid::Kernel::DateAndTime(isoDate).to_time_t();
    }

    /**
     * Obtain the algorithm documentation for the given property.
     * @param properties :: A list of properties for a provided algorithm.
     * @param name       :: The name of the property to search for.
     * @return The documentation for a given property name.
     */
    const std::string CatalogHelper::propertyDocumentation(const std::vector<Mantid::Kernel::Property*> &properties, const std::string &name)
    {
      for (unsigned i = 0; i < properties.size(); i++)
      {
        if (properties.at(i)->name() == name)
        {
          return properties.at(i)->documentation();
        }
      }
      return "";
    }

    /**
     * Creates an algorithm with the provided name.
     * @param algName :: The name of the algorithm to create.
     * @return A shared pointer to the algorithm created.
     */
    Mantid::API::IAlgorithm_sptr CatalogHelper::createCatalogAlgorithm(const std::string &algName)
    {
      // If there is an exception we want it to be thrown.
      return Mantid::API::AlgorithmManager::Instance().create(algName);
    }

    /**
     * Execute the given algorithm asynchronously.
     * @param algorithm :: The algorithm to execute.
     */
    void CatalogHelper::executeAsynchronously(const Mantid::API::IAlgorithm_sptr &algorithm)
    {
      Poco::ActiveResult<bool> result(algorithm->executeAsync());
      while(!result.available())
      {
        QCoreApplication::processEvents();
      }
    }

  } // namespace MantidWidgets
} // namespace MantidQt
