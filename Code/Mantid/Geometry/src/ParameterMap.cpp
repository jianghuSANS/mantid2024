#include "MantidGeometry/ParameterMap.h"

namespace Mantid
{
namespace Geometry
{

// Get a reference to the logger
Kernel::Logger& ParameterMap::g_log = Kernel::Logger::get("ParameterMap");

/// Used in std::find_if as comparison opeartor.
class equalParameterName
{
public:
    /// Constructor
    equalParameterName(const std::string& str):m_name(str){}
    /// Comparison operator
    bool operator()(const ParameterMap::pmap::value_type p)
    {
        return p.second->name() == m_name;
    }
private:
    std::string m_name;///< parameter name
};

Parameter_sptr ParameterMap::get(const IComponent* comp,const std::string& name)const
{
    pmap_it it_found = m_map.find(comp);
    if (it_found == m_map.end()) 
    {
        //g_log.error("Component "+comp+" does not have any parameters.");
        return Parameter_sptr();// ?
    }

    pmap_it it_begin = m_map.lower_bound(comp);
    pmap_it it_end = m_map.upper_bound(comp);

    pmap_it param = std::find_if(it_begin,it_end,equalParameterName(name));

    if ( param == it_end )
    {
        //g_log.error("Component "+comp+" does not have parameter "+name+".");
        return Parameter_sptr();// ?
    }
        
    return param->second;
}

std::string ParameterMap::getString(const IComponent* comp,const std::string& name)
{
    Parameter_sptr param = get(comp,name);
    if (!param) return "";
    return param->asString();
}

std::vector<std::string> ParameterMap::nameList(const IComponent* comp)const
{
    std::vector<std::string> lst;

    pmap_it it_found = m_map.find(comp);
    if (it_found == m_map.end()) 
    {
        return lst;
    }

    pmap_it it_begin = m_map.lower_bound(comp);
    pmap_it it_end = m_map.upper_bound(comp);

    for(pmap_it it = it_begin;it!=it_end;it++)
        lst.push_back(it->second->name());

    return lst;

}


/** Create or adjust "pos" parameter for a component
  * Assumed that name either equals "x", "y" or "z" otherwise this method will not add/modify "pos" parameter
    @param comp Component
    @param name Parameter name
    @param value Parameter value
  */
void ParameterMap::addPositionCoordinate(const IComponent* comp,const std::string& name, const double value)
{
  Parameter_sptr param = get(comp,"pos");
  V3D position;
  if (param)
  {
    // so "pos" already defined
    position = param->value<V3D>();
  }
  else
  {
    // so "pos" is not defined - therefore get position from component
    position = comp->getPos();
  }

  // adjust position

  if ( name.compare("x")==0 )
    position.setX(value);
  else if ( name.compare("y")==0 )
    position.setY(value);
  else if ( name.compare("z")==0 )
    position.setZ(value);
  else
  {
    g_log.warning() << "addPositionCoordinate() called with unrecognised coordinate symbol: " << name;
    return;
  }

  // finally add or update "pos" parameter

  if (param)
    param->set(position);
  else
    addV3D(comp, "pos", position);
}

/// Create or adjust "rot" parameter for a component
void ParameterMap::addRotationParam(const IComponent* comp, const double deg)
{
  Parameter_sptr param = get(comp,"rot");
  Quat quat;
  if (param)
  {
    // so "rot" already defined
    quat = param->value<Quat>();
  }
  else
  {
    // so "rot" is not defined - therefore get quarternion from component
    quat = comp->getRelativeRot();
  }

  // adjust rotation
  quat.setRotation(deg);


  // finally add or update "pos" parameter
  if (param)
    param->set(quat);
  else
    addQuat(comp, "rot", quat);
}


} // Namespace Geometry

} // Namespace Mantid

