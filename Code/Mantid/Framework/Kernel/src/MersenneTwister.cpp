//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------
#include "MantidKernel/MersenneTwister.h"

namespace Mantid 
{
  namespace Kernel 
  {

    //------------------------------------------------------------------------------
    // Public member functions
    //------------------------------------------------------------------------------
    
    /**
     * Constructor taking a seed value. Sets the range to [0.0,1.0]
     * @param seedValue :: The initial seed
     */
    MersenneTwister::MersenneTwister(const size_t seedValue) :
      m_generator(), m_uniform_dist(), m_currentSeed(0), m_savedStateGenerator(NULL)
    {
      setSeed(seedValue);
      setRange(0.0, 1.0);
    }

    /**
     * Constructor taking a seed value and a range
     * @param seedValue :: The initial seed
     * @param start :: The minimum value a generated number should take
     * @param end :: The maximum value a generated number should take
     */
    MersenneTwister::MersenneTwister(const size_t seedValue, const double start, const double end) :
      m_generator(), m_uniform_dist(), m_currentSeed(), m_savedStateGenerator(NULL)
    {
      setSeed(seedValue);
      setRange(start,end);
    }

    /// Destructor
    MersenneTwister::~MersenneTwister()
    {
      delete m_savedStateGenerator;
    }

    /**
     * (Re-)seed the generator. This clears the current saved state
     * @param seedValue :: A seed for the generator
     */
    void MersenneTwister::setSeed(const size_t seedValue)
    {
      // Bug in earlier versions of this implementation meant
      // that a unsigned int could not be past to the seed function
      m_currentSeed = (boost::mt19937::result_type)seedValue;
      m_generator.seed(m_currentSeed);
      delete m_savedStateGenerator;
      m_savedStateGenerator = NULL;
    }

    /**
     * Sets the range of the subsequent calls to nextValue() 
     * @param start :: The lowest value a call to nextValue() will produce
     * @param end :: The largest value a call to nextValue() will produce
     */
    void MersenneTwister::setRange(const double start, const double end)
    {
      m_uniform_dist = uniform_double(start,end);
    }
    
    /**
     * Returns the next number in the pseudo-random sequence generated by
     * the Mersenne Twister 19937 algorithm.
     * @returns The next number in the pseudo-random sequence
     */
    double MersenneTwister::nextValue()
    {
      /// A variate generator to combine a random number generator with a distribution
      uniform_generator uniform_rand(m_generator, m_uniform_dist);
      // There is no reason why this call shouldn't be considered const
      return uniform_rand();
    }

    /**
     * Resets the generator using the value given at the last call to setSeed
     */
    void MersenneTwister::restart()
    {
      setSeed(m_currentSeed);
    }

    /// Saves the current state of the generator
    void MersenneTwister::save()
    {
      m_savedStateGenerator = new boost::mt19937(m_generator); // Copy the state
    }

    /// Restores the generator to the last saved point, or the beginning if nothing has been saved
    void MersenneTwister::restore()
    {
      // Copy saved to current, still distinct objects so that another restore still brings us
      // back to the originally saved point
      if(m_savedStateGenerator)
      {
        m_generator = boost::mt19937(*m_savedStateGenerator);
      }
      else
      {
        restart();
      }
    }

  }
}
