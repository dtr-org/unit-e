// Copyright (c) 2018 The unit-e core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef UNITE_ESPERANZA_PARAMS_H
#define UNITE_ESPERANZA_PARAMS_H

#include <stdint.h>

#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_DLL
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__ ((dllexport))
    #else
      #define DLL_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #else
    #ifdef __GNUC__
      #define DLL_PUBLIC __attribute__ ((dllimport))
    #else
      #define DLL_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #endif
  #define DLL_LOCAL
#else
#if __GNUC__ >= 4
#define DLL_PUBLIC __attribute__ ((visibility ("default")))
    #define DLL_LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define DLL_PUBLIC
#define DLL_LOCAL
#endif
#endif

namespace esperanza {

/*!
 * \brief Esperanza Proof-of-Stake specific blockchain parameters.
 */
class DLL_PUBLIC Params final {

 private:

  //! seconds to elapse before new modifier is computed
  uint32_t m_modifierInterval;

  //! min depth in chain before staked output is spendable
  uint32_t m_stakeMinConfirmations;

  //! targeted number of seconds between blocks
  uint32_t m_targetSpacing;

  uint32_t m_targetTimespan;

  //! bitmask of 4 bits, every kernel stake hash will change every 16 seconds
  uint32_t m_stakeTimestampMask = (1 << 4) - 1;

  uint32_t m_lastImportHeight;

 public:

  uint32_t GetModifierInterval() const;

  uint32_t GetStakeMinConfirmations() const;

  uint32_t GetTargetSpacing() const;

  uint32_t GetTargetTimespan() const;

  uint32_t GetStakeTimestampMask(int nHeight) const;

  uint32_t GetLastImportHeight() const;

};

} // namespace esperanza

#endif // UNITE_ESPERANZA_PARAMS_H
