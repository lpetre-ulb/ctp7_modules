/*! \file vfat3.h
 *  \brief RPC module for VFAT3 methods
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#ifndef COMMON_VFAT3_H
#define COMMON_VFAT3_H

#include "ctp7_modules/common/hw_constants.h"

#include "xhal/common/rpc/common.h"

#include <map>
#include <string>
#include <vector>

namespace vfat3 {

  /*!
   *  \brief Verify whether VFATs on a specified front end optical link are in sync
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c mask VFAT mask, default is all VFATs unmasked
   *
   *  \returns Bitmask of sync'ed VFATs
   */
  struct vfatSyncCheck : public xhal::common::rpc::Method
  {
    uint32_t operator()(const uint16_t& ohN, const uint32_t& mask=0xff000000) const;
  };

  /*!
   *  \brief configures the VFAT3s on optohybrid ohN to use their ADCs to monitor the DAC provided by dacSelect.
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c mask VFAT mask, default is all VFATs unmasked
   *  \param \c dacSelect the monitoring selection for the VFAT3 ADC, possible values are [0,16] and [32,41].  See VFAT3 manual for details
   */
  struct configureVFAT3DACMonitor : public xhal::common::rpc::Method
  {
    void operator()(const uint16_t& ohN, const uint32_t& mask, const uint32_t& dacSelect) const;
  };

  /*!
   *  \brief As \ref \c configureVFAT3DACMonitor but for all optical links specified in \c ohMask on the AMC
   *
   *  \param \c ohMask specifies which OptoHybrids to read from, this is a 12 bit number where a 1 in the n^th bit indicates that the n^th OH should be read back.
   *  \param \c vfatMasks is an array of size 12 where each element is the standard vfatMask for OH specified by the array index.
   *  \param \c dacSelect the monitoring selection for the VFAT3 ADC, possible values are [0,16] and [32,41].  See VFAT3 manual for details
   */
  struct configureVFAT3DACMonitorMultiLink : public xhal::common::rpc::Method
  {
    void operator()(const uint16_t& ohMask, const std::array<uint32_t, amc::OH_PER_AMC>& vfatMasks, const uint32_t& dacSelect) const;
  };

  /*!
   *  \brief Configures VFAT3 chips
   *
   *  \details VFAT configurations are stored in files under \c /mnt/persistent/gemdaq/vfat3/config_OHX_VFATY.txt. Has to be updated later.
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatMask VFAT mask, default is all VFATs unmasked
   */
  struct configureVFAT3s : public xhal::common::rpc::Method
  {
    void operator()(const uint16_t& ohN, const uint32_t& vfatMask=0xff000000) const;
  };

  /*!
   *  \brief reads all channel registers for unmasked vfats and stores values in chanRegData
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatMask VFAT mask, default is all VFATs unmasked
   *
   *  \returns \c std::vector<uint32_t> containing channel registers with idx = vfatN * 128 + chan
   */
  struct getChannelRegistersVFAT3 : public xhal::common::rpc::Method
  {
    std::vector<uint32_t> operator()(const uint16_t& ohN, const uint32_t& vfatMask=0xff000000) const;
  };

  /*!
   *  \brief reads the ADC of all unmasked VFATs
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c useExtRefADC true (false) read the ADC1 (ADC0) which uses an external (internal) reference
   *  \param \c vfatMask VFAT mask, default is all VFATs unmasked //FIXME deprecate
   *
   *  \returns \c std::vector<uint32_t> containing the ADC results
   */
  struct readVFAT3ADC : public xhal::common::rpc::Method
  {
    std::vector<uint32_t> operator()(const uint16_t& ohN, const bool &useExtRefADC=false, const uint32_t& vfatMask=0xff000000) const;
  };


  /*!
   *  \brief As readVFAT3ADC(...) but for all optical links specified in ohMask on the AMC
   *
   *  \param \c ohMask specifies which OptoHybrids to read from, this is a 12 bit number where a 1 in the n^th bit indicates that the n^th OH should be read back.
   *  \param \c useExtRefADC true (false) read the ADC1 (ADC0) which uses an external (internal) reference
   *  \param \c ohVfatMaskArray which is an array of size 12 where each element is the standard vfatMask for OH specified by the array index. //FIXME deprecate
   *
   *  \returns \c std::map whose keys are the OH number and the data is an std::vector<uint32_t> containing the requested ADC data
   */
  struct readVFAT3ADCMultiLink : public xhal::common::rpc::Method
  {
    std::map<uint32_t,std::vector<uint32_t>> operator()(const uint16_t& ohMask, const bool &useExtRefADC) const;
  };

  /*!
   *  \brief Writes all VFAT3 channel registers based on the channel register
   *
   *  \description The channel setting vector should contain VFATS_PER_OH*NUM_CHANS entries.
   *               The (vfatN,ch) pairing determines the index via: idx = vfatN*128 + ch
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c chanRegData contains the channel registers
   *  \param \c vfatMask VFAT mask, default is all VFATs unmasked
   */
  struct setChannelRegistersVFAT3Simple : public xhal::common::rpc::Method
  {
    void operator()(const uint16_t& ohN, const std::vector<uint32_t>& chanRegData, const uint32_t& vfatMask=0xff000000) const;
  };

  /*!
   *  \brief Writes all VFAT3 channel registers based on the bits in the channel register
   *
   *  \description Each vector should contain VFATS_PER_OH*NUM_CHANS entries.
   *               The (vfatN,ch) pairing determines the index via: idx = vfatN*128 + ch
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatMask VFAT mask, default is all VFATs unmasked
   *  \param \c calEnable contains the calibration pulse enable settings
   *  \param \c masks contains the channel mask settings
   *  \param \c trimARM contains the ARM DAC channel trim settings
   *  \param \c trimARMPol contains the ARM DAC channel polarity settings
   *  \param \c trimZCC contains the ZCC DAC channel trim settings
   *  \param \c trimZCCPol contains the ZCC DAC channel polarity settings
   */
  struct setChannelRegistersVFAT3 : public xhal::common::rpc::Method
  {
    void operator()(const uint16_t& ohN,
                    const std::vector<uint32_t> &calEnable,
                    const std::vector<uint32_t> &masks,
                    const std::vector<uint32_t> &trimARM,
                    const std::vector<uint32_t> &trimARMPol,
                    const std::vector<uint32_t> &trimZCC,
                    const std::vector<uint32_t> &trimZCCPol,
                    const uint32_t& vfatMask=0xff000000)
      const;
  };

  /*!
   *  \brief
   *
   *  \param \c ohN OptoHybrid optical link number
   *
   *  \returns \c std::map whose keys are the register name and whose values are std::vector<uint32_t> of the values for each VFAT
   */
  struct statusVFAT3s : public xhal::common::rpc::Method
  {
    std::map<std::string, std::vector<uint32_t>> operator()(const uint16_t& ohN) const;
  };

  /*!
   *  \brief Returns list of values of the most important VFAT3 register
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatMask VFAT mask, default is all VFATs unmasked
   *  \param \c rawID if true, do not perform Reed--Muller decoding of the read chip ID (default is false)
   *
   *  \returns \c std::vector containing the chipID values, indexed by the VFAT position number
   */
  struct getVFAT3ChipIDs : public xhal::common::rpc::Method
  {
    std::vector<uint32_t> operator()(const uint16_t& ohN, const uint32_t& vfatMask=0xff000000, const bool& rawID=false) const;
  };

}

#endif
