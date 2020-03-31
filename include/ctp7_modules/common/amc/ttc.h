/*!
 * \file amc/ttc.h
 * \brief RPC module for AMC TTC methods, copied in from cmsgemos/gem::hw::HwGenericAMC
 * \author Jared Sturdy <sturdy@cern.ch>
 */

#ifndef COMMON_AMC_TTC_H
#define COMMON_AMC_TTC_H

#include "xhal/common/rpc/common.h"

namespace amc {
  namespace ttc {

    /*!
     * \defgroup ttc TTC module functionality
     */

    /** Locally executed methods */
    /*** CTRL submodule ***/
    /*!
     * \brief Reset the TTC module
     */
    struct ttcModuleReset : public xhal::common::rpc::Method
    {
      void operator()() const;
    };

    /*!
     * \brief Reset the MMCM of the TTC module
     */
    struct ttcMMCMReset : public xhal::common::rpc::Method
    {
      void operator()() const;
    };

    /*!
     * \brief Shift the phase of the MMCM of the TTC module
     *
     * \details Performs the locking procedure performs up to 3840 shifts (full width of one good + bad region).
     *          If relock is true, then the procedure will:
     *            * first shift into a bad region (not an edge)
     *            * find the next good lock status
     *            * shift halfway through the region (1920 for BC0_LOCKED, 1000 for PLL_LOCKED)
     *          If relock is false, then the procedure will find X consecutive "good" locks for X = 200 (50) for BC0_LOCKED (PLL_LOCKED).
     *          It will then reverse direction and shift backwards halfway.
     *          If a bad lock is encountered it will reset and try again.  Otherwise it will take the phase at the back half point
     *
     * \throws \c std::runtime_error if readback of \c TTC.CTRL registers does not match expectation or if a lock is not found
     *
     * \param relock controls whether the procedure will force a relock
     * \param modeBC0 controls whether the procedure will use the BC0_LOCKED or the PLL_LOCKED register.
     *        Note for GEM_AMC FW > 1.13.0 BC0_LOCKED doesn't work.
     * \param scan tells the procedure to run through the full possibility of phases several times, and just logs the place where it found a lock
     */
    struct ttcMMCMPhaseShift : public xhal::common::rpc::Method
    {
      void operator()(const bool& relock=false, const bool& modeBC0=false, const bool& scan=false) const;
    };

    /*!
     * \brief Resets the MMCM PLL and checks if it relocks
     *
     * \param readAttempts Specifies the number of times to reset the PLL and check for a relock
     *
     * \return Returns the number of times the MMCM PLL relocked
     */
    struct checkPLLLock : public xhal::common::rpc::Method
    {
      uint32_t operator()(const uint32_t& readAttempts) const;
    };

    /*!
     * \brief Get the mean value of the MMCM phase
     *
     * \param readAttempts Specifies the number of times to read the MMCM phase and compute the mean
     *        * 0 means read the mean calculated in the FW
     *        * 1+ means read the mean compute the mean of the specified number of reads
     *
     * \returns Mean value of the MMCH phase
     */
    struct getMMCMPhaseMean : public xhal::common::rpc::Method
    {
      float operator()(const uint32_t& readAttempts) const;
    };

    /*!
     * \brief Get the mean value of the GTH phase
     *
     * \param readAttempts Specifies the number of times to read the GTH phase and compute the mean
     *        * 0 means read the mean calculated in the FW
     *        * 1+ means read the mean compute the mean of the specified number of reads
     *
     * \returns Mean value (calculated in firmware) of the GTH phase
     */
    struct getGTHPhaseMean : public xhal::common::rpc::Method
    {
      float operator()(const uint32_t& readAttempts) const;
    };

    /*!
     * \brief Get the median value of the MMCM phase
     *
     * \param readAttempts Specifies the number of times to read the MMCM phase and compute the median
     *
     * \returns Median value of the MMCH phase
     */
    struct getMMCMPhaseMedian : public xhal::common::rpc::Method
    {
      float operator()(const uint32_t& readAttempts) const;
    };

    /*!
     * \brief Get the median value of the GTH phase
     *
     * \param readAttempts Specifies the number of times to read the GTH phase and compute the median
     *
     * \returns Median value of the GTH phase
     */
    struct getGTHPhaseMedian : public xhal::common::rpc::Method
    {
      float operator()(const uint32_t& readAttempts) const;
    };

    /*!
     * \brief Reset the counters of the TTC module
     */
    struct ttcCounterReset : public xhal::common::rpc::Method
    {
      void operator()() const;
    };

    /*!
     * \returns whether or not L1As are currently enabled on the GenericAMC
     */
    struct getL1AEnable : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \param whether or not to enable L1As on the GenericAMC
     */
    struct setL1AEnable : public xhal::common::rpc::Method
    {
      void operator()(const bool& enable=true) const;
    };

    /*** CONFIG submodule ***/
    /*!
     * \param cmd AMCTTCCommandT enum type to retrieve the current configuration of
     *
     * \returns TTC configuration register values
     */
    struct getTTCConfig : public xhal::common::rpc::Method
    {
      uint32_t operator()(const uint8_t& cmd) const;
    };

    /*!
     * \param cmd AMCTTCCommandT to set the current configuration of
     */
    struct setTTCConfig : public xhal::common::rpc::Method
    {
      void operator()(const uint8_t& cmd, const uint8_t& value) const;
    };

    /*** STATUS submodule ***/
    /*!
     * \brief Returns the first status register of the TTC module
     */
    struct getTTCStatus : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \brief Returns the error count of the TTC module
     *
     * \param specify whether single or double error count
     */
    struct getTTCErrorCount : public xhal::common::rpc::Method
    {
      uint32_t operator()(const bool& single=true) const;
    };

    /*** CMD_COUNTERS submodule ***/
    /*!
     * \param cmd to get the current configuration of
     *
     * \returns Returns the counter for the specified TTC command
     */
    struct getTTCCounter : public xhal::common::rpc::Method
    {
      uint32_t operator()(const uint8_t& cmd) const;
    };

    /*!
     * \returns Returns the L1A ID received by the TTC module
     */
    struct getL1AID : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns the curent L1A rate (in Hz)
     */
    struct getL1ARate : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \brief Query the entries in the TTC spy buffer
     *
     * \returns 32-bit word corresponding to the 8 most recent TTC commands received
     */
    struct getTTCSpyBuffer : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };
  }
}

#endif
