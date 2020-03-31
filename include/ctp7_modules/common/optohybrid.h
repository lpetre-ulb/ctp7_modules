/*! \file optohybrid.h
 *  \brief RPC module for \c OptoHybrid methods
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Brin Dorney <brian.l.dorney@cern.ch>
 */
#ifndef COMMON_OPTOHYBRID_H
#define COMMON_OPTOHYBRID_H

#include "xhal/common/rpc/common.h"

#include <map>
#include <string>
#include <vector>

namespace oh {

  /*!
   *  \brief Performs a write transaction on a specified regiser for unmasked the VFATs of a specified \c OptoHybrid
   *
   *  \param \c ohN \c OptoHybrid optical link number
   *  \param \c regName Register name
   *  \param \c value Register value to write
   *  \param \c mask VFAT mask. Default: no chips will be masked
   */
  struct broadcastWrite : public xhal::common::rpc::Method
  {
    void operator()(const uint32_t& ohN, const std::string& regName, const uint32_t& value, const uint32_t& mask=0xff000000) const;
  };

  /*!
   *  \brief Performs a read transaction on a specified regiser on unmasked VFATs of a specified \c OptoHybrid
   *
   *  \param \c ohN \c OptoHybrid optical link number
   *  \param \c regName Register name
   *  \param \c mask VFAT mask. Default: no chips will be masked
   *
   *  \returns std::vector<uint32_t> holding the results of the broadcast read
   */
  struct broadcastRead : public xhal::common::rpc::Method
  {
    std::vector<uint32_t> operator()(const uint32_t& ohN, const std::string& regName, const uint32_t& mask=0xff000000) const;
  };

  /*!
   *  \brief Sets default values to VFAT parameters. VFATs will remain in sleep mode
   *
   *  \param \c ohN \c OptoHybrid optical link number
   *  \param \c mask VFAT mask. Default: no chips will be masked
   */
  struct biasAllVFATs : public xhal::common::rpc::Method
  {
    void operator()(const uint32_t& ohN, const uint32_t& mask=0xff000000) const;
  };

  /*!
   *  \briefSets VFATs to run mode
   *
   *  \param \c ohN \c OptoHybrid optical link number (string)
   *  \param \c mask VFAT mask. Default: no chips will be masked
   */
  struct setAllVFATsToRunMode : public xhal::common::rpc::Method
  {
    void operator()(const uint32_t& ohN, const uint32_t& mask=0xff000000) const;
  };

  /*!
   *  \brief Sets VFATs to sleep mode
   *
   *  \param \c ohN \c OptoHybrid optical link number (string)
   *  \param \c mask VFAT mask. Default: no chips will be masked
   */
  struct setAllVFATsToSleepMode : public xhal::common::rpc::Method
  {
    void operator()(const uint32_t& ohN, const uint32_t& mask=0xff000000) const;
  };

  /*!
   *  \brief Returns a list of the most important monitoring registers of \c OptoHybrids
   *
   *  \param \c ohMask Bit mask of enabled optical links
   *
   *  \returns std::map with keys corresponding to the OH status registers
               and values are stored in an std::vector with one entry for each OH
   */
  struct statusOH : public xhal::common::rpc::Method
  {
    std::map<std::string, std::vector<uint32_t>> operator()(const uint32_t& ohMask) const;
  };

  /*!
   *  \brief Disables calibration pulse in channels between chMin and chMax
   *
   *  \param \c ohN \c OptoHybrid optical link number
   *  \param \c mask VFAT mask. Default: no chips will be masked
   *  \param \c chMin Minimal channel number
   *  \param \c chMax Maximal channel number
   */
  struct stopCalPulse2AllChannels : public xhal::common::rpc::Method
  {
    void operator()(const uint32_t& ohN, const uint32_t& mask, const uint32_t& ch_min, const uint32_t& ch_max) const;
  };

}

#endif
