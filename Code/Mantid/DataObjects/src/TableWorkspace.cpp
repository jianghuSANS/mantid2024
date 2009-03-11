#include "MantidDataObjects/TableWorkspace.h"
#include "MantidKernel/Logger.h"
#include "MantidDataObjects/ColumnFactory.h"
#include "MantidAPI/WorkspaceProperty.h"

#include <iostream>

namespace Mantid
{
  namespace DataObjects
  {

    // Get a reference to the logger
    Kernel::Logger& TableWorkspace::g_log = Kernel::Logger::get("TableWorkspace");

    /// Constructor
    TableWorkspace::TableWorkspace(int nrows)
    {setRowCount(nrows);}

    ///Destructor
    TableWorkspace::~TableWorkspace()
    {}

    /** @param type Data type of the column.
        @param name Column name.
        @return True if the column was successfully created.
    */
    bool TableWorkspace::createColumn(const std::string& type, const std::string& name)
    {
        if (type.empty())
        {
            g_log.error("Empty string passed as type argument of createColumn.");
            return false;
        }
        if (name.empty())
        {
            g_log.error("Empty string passed as name argument of createColumn.");
            return false;
        }
        // Check that there is no column with the same name.
        column_it ci = std::find_if(m_columns.begin(),m_columns.end(),FindName(name));
        if (ci != m_columns.end())
            {
                g_log.error()<<"Column with name "<<name<<" already exists.\n";
                return false;
            }
        try
        {
            boost::shared_ptr<Column> c = ColumnFactory::Instance().create(type);
            m_columns.push_back(c);
            c->setName(name);
            c->resize(rowCount());
        }
        catch(Kernel::Exception::NotFoundError& e)
        {
            g_log.error()<<"Column of type "<<type<<" and name "<<name<<" has not been created.\n";
            g_log.error()<<e.what()<<'\n';
            return false;
        }
        return true;
    }

    /** If count is greater than the current number of rows extra rows are added to the bottom of the table.
        Otherwise rows at the end are erased to reach the new size.
        @param count New number of rows.
    */
    void TableWorkspace::setRowCount(int count)
    {
        if (count == rowCount()) return;
        for(column_it ci=m_columns.begin();ci!=m_columns.end();ci++)
            (*ci)->resize(count);
        m_rowCount = count;
    }

    /// Gets the shared pointer to a column.
    boost::shared_ptr<Column> TableWorkspace::getColumn(const std::string& name)
    {
        column_it ci = std::find_if(m_columns.begin(),m_columns.end(),FindName(name));
        if (ci == m_columns.end())
        {
            std::string str = "Column " + name + " does not exist.\n";
            g_log.error(str);
            throw std::runtime_error(str);
        }
        return *ci;
    }

    void TableWorkspace::removeColumn( const std::string& name)
    {
        column_it ci = std::find_if(m_columns.begin(),m_columns.end(),FindName(name));
        if (ci != m_columns.end())
        {
            if ( !ci->unique() )
            {
                g_log.error()<<"Deleting column in use ("<<name<<").\n";
            }
            m_columns.erase(ci);
        }
    }

    /** @param index Points where to insert the new row.
        @return Position of the inserted row.
    */
    int TableWorkspace::insertRow(int index)
    {
        if (index >= rowCount()) index = rowCount();
        for(column_it ci=m_columns.begin();ci!=m_columns.end();ci++)
            (*ci)->insert(index);
        ++m_rowCount;
        return index;
    }

    /** @param index Row to delete.
    */
    void TableWorkspace::removeRow(int index)
    {
        if (index >= rowCount())
        {
            g_log.error()<<"Attempt to delete a non-existing row ("<<index<<")\n";
            return;
        }
        for(column_it ci=m_columns.begin();ci!=m_columns.end();ci++)
            (*ci)->remove(index);
        --m_rowCount;
    }

    std::vector<std::string> TableWorkspace::getColumnNames()
    {
        std::vector<std::string> nameList;
        for(column_it ci=m_columns.begin();ci!=m_columns.end();ci++)
            nameList.push_back((*ci)->name());
        return nameList;
    }

//    template<>
//    boost::tuples::null_type TableWorkspace::make_TupleRef< boost::tuples::null_type >(int j,const std::vector<std::string>& names,int i)
//    {return boost::tuples::null_type();}

  } // namespace DataObjects
} // namespace Mantid

///\cond TEMPLATE
namespace Mantid
{
  namespace Kernel
  {
    template<> DLLExport
    DataObjects::TableWorkspace_sptr IPropertyManager::getValue<DataObjects::TableWorkspace_sptr>(const std::string &name) const
    {
      PropertyWithValue<DataObjects::TableWorkspace_sptr>* prop =
        dynamic_cast<PropertyWithValue<DataObjects::TableWorkspace_sptr>*>(getPointerToProperty(name));
      if (prop)
      {
        return *prop;
      }
      else
      {
        std::string message = "Attempt to assign property "+ name +" to incorrect type";
        throw std::runtime_error(message);
      }
    }

  } // namespace Kernel
} // namespace Mantid

///\endcond TEMPLATE
