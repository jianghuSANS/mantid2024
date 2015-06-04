#include "MantidGeometry/Crystal/SpaceGroup.h"

namespace Mantid {
namespace Geometry {

using namespace Kernel;

/**
 * Constructor
 *
 * This constructor creates a space group with the symmetry operations contained
 * in the Group-parameter and assigns the given number and symbol.
 *
 * @param itNumber :: Space group number (ITA)
 * @param hmSymbol :: Herman-Mauguin symbol for the space group
 * @param group :: Group that contains all symmetry operations (including
 *centering).
 */
SpaceGroup::SpaceGroup(size_t itNumber, const std::string &hmSymbol,
                       const Group &group)
    : Group(group), m_number(itNumber), m_hmSymbol(hmSymbol) {}

/// Copy constructor
SpaceGroup::SpaceGroup(const SpaceGroup &other)
    : Group(other), m_number(other.m_number), m_hmSymbol(other.m_hmSymbol) {}

/// Assignment operator, utilizes Group's assignment operator
SpaceGroup &SpaceGroup::operator=(const SpaceGroup &other) {
  Group::operator=(other);

  m_number = other.m_number;
  m_hmSymbol = other.m_hmSymbol;

  return *this;
}

/// Returns the stored space group number
size_t SpaceGroup::number() const { return m_number; }

/// Returns the stored Hermann-Mauguin symbol
std::string SpaceGroup::hmSymbol() const { return m_hmSymbol; }

/**
 * Returns whether the given reflection is allowed or not in this space group
 *
 * Space groups that contain translational symmetry cause certain reflections
 * to be absent due to the contributions of symmetry equivalent atoms to the
 * structure factor cancelling out. This method implements the procedure
 * described in the IUCr teaching pamphlet no. 9 [1] to check whether a
 * reflection is allowed or not according to the symmetry operations in the
 * space group. Please note that certain arrangements of atoms can lead to
 * additional conditions that can not be determined using a space group's
 * symmetry operations alone. For these situations, Geometry::CrystalStructure
 * can help.
 *
 * [1] http://www.iucr.org/education/pamphlets/9/full-text
 *
 * @param hkl :: HKL to be checked.
 * @return :: true if the reflection is allowed, false otherwise.
 */
bool SpaceGroup::isAllowedReflection(const Kernel::V3D &hkl) const {
  for (auto op = m_allOperations.begin(); op != m_allOperations.end(); ++op) {
    if ((*op).hasTranslation()) {

      // Do transformation only if necessary
      if ((fmod(hkl.scalar_prod((*op).vector()), 1.0) != 0) &&
          ((*op).transformHKL(hkl) == hkl)) {
        return false;
      }
    }
  }

  return true;
}

} // namespace Geometry
} // namespace Mantid
