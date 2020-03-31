/*! \file amc/daq.h
 *  \brief RPC module for AMC DAQ methods, copied in from cmsgemos/gem::hw::HwGenericAMC
 *  \author Jared Sturdy <sturdy@cern.ch>
 */

#ifndef COMMON_AMC_DAQ_H
#define COMMON_AMC_DAQ_H

#include "xhal/common/rpc/common.h"

namespace amc {
  namespace daq {

    /*!
     * \defgroup daq DAQ module related setup and status
     */

    /*!
     * \brief Set the enable mask and enable the DAQ link
     *
     * \param \c enableMask 32 bit word for the 24 bit enable mask
     */
    struct  enableDAQLink : public xhal::common::rpc::Method
    {
      void operator()(const uint32_t& enableMask=0x1) const;
    };

    /*!
     * \brief Set the DAQ link off and disable all inputs
     */
    struct disableDAQLink : public xhal::common::rpc::Method
    {
      void operator()() const;
    };

    /*!
     * \brief Set the zero suppression mode
     *
     * \param \c enable true means any VFAT data packet with all 0's will be suppressed
     */
    struct setZS : public xhal::common::rpc::Method
    {
      void operator()(const bool& enable=true) const;
    };
    /*!
     * \brief Disable zero suppression of VFAT data
     */
    struct disableZS : public xhal::common::rpc::Method
    {
      void operator()() const;
    };
    /*!
     * \brief reset the DAQ link and write the DAV timout
     *        - assert RESET and then release
     *        - disable DAQ link
     *        - set each link EOE_TIMEOUT to default value
     *        - set DAV_TIMEOUT  to supplied value
     *        - set TTS_OVERRIDE to supplied value
     *
     * \param \c davTO value to use for the DAV timeout
     * \param \c ttsOverride value to use for the TTS override
     */
    struct resetDAQLink : public xhal::common::rpc::Method
    {
      void operator()(const uint32_t& davTO=0x500, const uint32_t& ttsOverride=0x0) const;
    };

    /*!
     * \returns Returns the 32 bit word corresponding to the DAQ link control register
     */
    struct getDAQLinkControl : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns the 32 bit word corresponding to the DAQ link status register
     */
    struct getDAQLinkStatus : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns true if the DAQ link is ready
     */
    struct daqLinkReady : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \returns Returns true if the DAQ link is clock is locked
     */
    struct daqClockLocked : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \returns Returns true if the TTC is ready
     */
    struct daqTTCReady : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \returns Returns the current TTS state asserted by the DAQ link firmware
     */
    struct daqTTSState : public xhal::common::rpc::Method
    {
      uint8_t operator()() const;
    };

    /*!
     * \returns Returns true if the event FIFO is almost full (70%)
     */
    struct daqAlmostFull : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \returns Returns true if the L1A FIFO is empty (0%)
     */
    struct l1aFIFOIsEmpty : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \returns Returns true if the L1A FIFO is almost full (70%)
     */
    struct l1aFIFOIsAlmostFull : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \returns Returns true if the L1A FIFO is full (100%)
     */
    struct l1aFIFOIsFull : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \returns Returns true if the L1A FIFO is underflos
     */
    struct l1aFIFOIsUnderflow : public xhal::common::rpc::Method
    {
      bool operator()() const;
    };

    /*!
     * \returns Returns the number of events built and sent on the DAQ link
     */
    struct getDAQLinkEventsSent : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns the curent L1AID (number of L1As received)
     */
    struct getDAQLinkL1AID : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns the curent L1A rate (in Hz)
     */
    struct getDAQLinkL1ARate : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns
     */
    struct getDAQLinkDisperErrors : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns
     */
    struct getDAQLinkNonidentifiableErrors : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns the DAQ link input enable mask
     */
    struct getDAQLinkInputMask : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns the timeout used in the event builder before closing the event and sending the (potentially incomplete) data
     */
    struct getDAQLinkDAVTimeout : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \param \c max is a bool specifying whether to query the max timer or the last timer
     *
     * \returns Returns the spent building an event
     */
    struct getDAQLinkDAVTimer : public xhal::common::rpc::Method
    {
      uint32_t operator()(const bool& max) const;
    };

    /*!
     * \param \c gtx is the input link status to query
     *
     * \returns Returns the the 32-bit word corresponding DAQ status for the specified link
     */
    struct getLinkDAQStatus : public xhal::common::rpc::Method
    {
      uint32_t operator()(const uint8_t& gtx) const;
    };

    /*!
     * \param \c gtx is the input link counter to query
     * \param \c mode specifies whether to query the corrupt VFAT count (0x0) or the event number
     *
     * \returns Returns the link counter for the specified mode
     */
    struct getLinkDAQCounters : public xhal::common::rpc::Method
    {
      uint32_t operator()(const uint8_t& gtx, const uint8_t& mode) const;
    };

    /*!
     * \param \c gtx is the input link status to query
     *
     * \returns Returns a block of the last 7 words received from the OH on the link specified
     */
    struct getLinkLastDAQBlock : public xhal::common::rpc::Method
    {
      uint32_t operator()(const uint8_t& gtx) const;
    };

    /*!
     * \returns Returns the timeout before the event builder firmware will close the event and send the data
     */
    struct getDAQLinkInputTimeout : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Returns the run type stored in the data stream
     */
    struct getDAQLinkRunType : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Special run parameters 1,2,3 as a single 24 bit word
     */
    struct getDAQLinkRunParameters : public xhal::common::rpc::Method
    {
      uint32_t operator()() const;
    };

    /*!
     * \returns Special run parameter written into data stream
     */
    struct getDAQLinkRunParameter : public xhal::common::rpc::Method
    {
      uint32_t operator()(const uint8_t& parameter) const;
    };


    /*!
     * \brief Set DAQ link timeout
     *
     * \param \c inputTO is the number of clock cycles to wait after receipt of last L1A and
     *        last packet received from the optical link before closing an "event"
     *        (in units of 160MHz clock cycles, value/4 for 40MHz clock cycles)
     */
    struct setDAQLinkInputTimeout : public xhal::common::rpc::Method
    {
      void operator()(const uint32_t& inputTO=0x100) const;
    };

    /*!
     * \brief Special run type to be written into data stream
     *
     * \param \c rtype is the run type
     */
    struct setDAQLinkRunType : public xhal::common::rpc::Method
    {
      void operator()(const uint32_t& rtype) const;
    };

    /*!
     * \param \c value is a 24 bit word to write into the run paramter portion of the GEM header
     *
     * \returns Set special run parameter to be written into data stream
     */
    struct setDAQLinkRunParameters : public xhal::common::rpc::Method
    {
      void operator()(const uint32_t& value) const;
    };
    /*!
     * \param \c parameter is the number of parameter to be written (1-3)
     * \param \c value is the run paramter to write into the specified parameter
     *
     * \returns Special run parameter written into data stream
     */
    struct setDAQLinkRunParameter : public xhal::common::rpc::Method
    {
      void operator()(const uint8_t& parameter, const uint8_t& value) const;
    };

    /** Composite functions, no specific "local" function defined */
    struct configureDAQModule : public xhal::common::rpc::Method
    {
      void operator()(const bool& enableZS=false, const uint32_t& runType=0x1, const bool& doPhaseShift=false, const bool& relock=false, const bool& bc0LockPSMode=false) const;
    };

    struct enableDAQModule : public xhal::common::rpc::Method
    {
      void operator()(const bool& enableZS=false) const;
    };
  }
}

#endif
