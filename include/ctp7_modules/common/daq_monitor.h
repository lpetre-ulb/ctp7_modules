/*! \file daq_monitor.h
 *  \brief RPC module for daq monitoring methods
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brin Dorney <brian.l.dorney@cern.ch>
 */

#ifndef COMMON_DAQ_MONITOR_H
#define COMMON_DAQ_MONITOR_H

#include "xhal/common/rpc/common.h"

#include <map>
#include <string>

namespace daqmon {
  const int NOH_MAX = 12;

  /*!
   *  \brief Reads a set of TTC monitoring registers
   *
   *  \returns \c std::map
   */
  struct getmonTTCmain : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()() const;
  };

  /*!
   *  \brief Reads a set of trigger monitoring registers
   *
   *  \param \c ohMask A 12 bit number which specifies which OptoHybrids to read from.
   *                A value of 1 in the n^th bit indicates that the n^th OptoHybrid should be considered.
   *
   *  \returns \c std::map
   */
  struct getmonTRIGGERmain : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
  };

  /*!
   *  \brief Reads a set of trigger monitoring registers at the OH
   *
   *    * LINK{0,1}_SBIT_OVERFLOW_CNT -- this is an interesting counter to monitor from operations perspective, but is not related to the health of the link itself.
   *      Rather it shows how many times OH had too many clusters which it couldn't fit into the 8 cluster per BX bandwidth.
   *      If this counter is going up it just means that OH is seeing a very high hit occupancy, which could be due to high radiation background, or thresholds configured too low.
   *
   *    The other 3 counters reflect the health of the optical links:
   *    * LINK{0,1}_OVERFLOW_CNT and LINK{0,1}_UNDERFLOW_CNT going up indicate a clocking issue.
   *      Specifically they go up when the frequency of the clock on the OH doesn't match the frequency on the CTP7.
   *      Given that CTP7 is providing the clock to OH, this in principle should not happen unless the OH is sending complete garbage and thus the clock cannot be recovered on CTP7 side, or the bit-error rate is insanely high, or the fiber is just disconnected, or OH is off.
   *      In other words, this indicates a critical problem.
   *    * LINK{0,1}_MISSED_COMMA_CNT also monitors the health of the link, but it's more sensitive, because it can go up due to isolated single bit errors.
   *      With radiation around, this might count one or two counts in a day or two.
   *      But if it starts running away, that would indicate a real problem, but in this case most likely the overflow and underflow counters would also see it.
   *
   *  \param \c ohMask A 12 bit number which specifies which OptoHybrids to read from.
   *         Having a value of 1 in the n^th bit indicates that the n^th OptoHybrid should be considered.
   *
   *  \returns \c std::map where keys are the aforementioned register names prefixed with OHX, values are the read register values
   */
  struct getmonTRIGGEROHmain : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
  };

  /*!
   *  \brief Reads a set of DAQ monitoring registers
   *
   *  \returns \c std::map
   */
  struct getmonDAQmain : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()() const;
  };

  /*!
   *  \brief Reads a set of DAQ monitoring registers at the OH
   *
   *  \param \c ohMask A 12 bit number which specifies which OptoHybrids to read from.
   *         Having a value of 1 in the n^th bit indicates that the n^th OptoHybrid should be considered.
   *
   *  \returns \c std::map
   */
  struct getmonDAQOHmain : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
  };

  /*!
   *  \brief Reads the GBT link status registers (READY, WAS_NOT_READY, etc...) for a particular ohMask
   *
   *  \param \c doReset boolean if true (false) a link reset will (not) be sent
   *
   *  \returns \c std::map
   */
  struct getmonGBTLink : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()(const bool& doReset=false) const;
  };

  /*!
   *  \brief Reads a set of OH monitoring registers at the OH
   *
   *  \param \c ohMask A 12 bit number which specifies which OptoHybrids to read from.
   *         Having a value of 1 in the n^th bit indicates that the n^th OptoHybrid should be considered.
   *
   *  \returns \c std::map
   */
  struct getmonOHmain : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
  };

  /*!
   *  \brief Reads the SCA Monitoring values of all OH's (voltage and temperature); these quantities are reported in ADC units
   *
   *  \deprecated
   *
   *  \param \c ohMask A 12 bit number which specifies which OptoHybrids to read from.
   *         Having a value of 1 in the n^th bit indicates that the n^th OptoHybrid should be considered.
   *
   *  \returns \c std::map
   */
  struct [[deprecated]] getmonOHSCAmain : xhal::common::rpc::Method
  {
    // __attribute__((__deprecated__))
    std::map<std::string, uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
  };

  /*!
   *  \brief reads FPGA Sysmon values of all unmasked OH's
   *
   *  \details Reads FPGA core temperature, core voltage (1V), and I/O voltage (2.5V); these quantities are reported in ADC units.
   *           The LSB for the core temperature correspons to 0.49 C.
   *           The LSB for the core voltage (both 1V and 2.5V) corresponds to 2.93 mV.
   *  \details Will also check error conditions (over temperature, 1V VCCINT, and 2.5V VCCAUX), and the error conunters for those conditions.
   *
   *  \param \c ohMask A 12 bit number which specifies which OptoHybrids to read from.
   *         Having a value of 1 in the n^th bit indicates that the n^th OptoHybrid should be considered.
   *  \param \c doReset reset counters CNT_OVERTEMP, CNT_VCCAUX_ALARM and CNT_VCCINT_ALARM (presently not working in FW)
   *
   *  \returns \c std::map
   */
  struct getmonOHSysmon : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()(const uint16_t& ohMask=0xfff, const bool& doReset=false) const;
  };

  /*!
   *  \brief Reads a set of SCA monitoring registers
   *
   *  \returns \c std::map
   */
  struct getmonSCA : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()() const;
  };

  /*!
   *  \brief Reads the VFAT link status registers (LINK_GOOD, SYNC_ERR_CNT, etc...) for a particular ohMask
   *
   *  \param \c doReset boolean if true (false) a link reset will (not) be sent
   *
   *  \returns \c std::map
   */
  struct getmonVFATLink : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()(const bool& doReset=false) const;
  };

  /*!
   *  \brief Creates a dump of the required registers, specified by a file expected in <blah>/registers.txt
   *
   *  \throws \c std::runtime_error if it is not possible to open \c fname
   *
   *  \returns \c std::map
   */
  struct getmonCTP7dump : xhal::common::rpc::Method
  {
    std::map<std::string, uint32_t> operator()(const std::string &fname="/mnt/persistent/gemdaq/mon_registers.txt") const;
  };
}

#endif
