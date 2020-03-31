/*! \file vfat3.cpp
 *  \brief RPC module for VFAT3 methods
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#include "ctp7_modules/common/vfat3.h"
#include "ctp7_modules/server/vfat3.h"
#include "ctp7_modules/common/amc.h"
#include "ctp7_modules/common/optohybrid.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"

#include "reedmuller.h"

#include "xhal/common/rpc/register.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <thread>

namespace vfat3 {
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
}

uint16_t vfat3::decodeChipID(const uint32_t& encChipID)
{
  // can the generator be static to limit creation/destruction of resources?
  static reedmuller rm = 0;
  static std::unique_ptr<int[]> encoded = nullptr;
  static std::unique_ptr<int[]> decoded = nullptr;

  if ((!(rm = reedmuller_init(2, 5)))
      || (!(encoded = std::make_unique<int[]>(rm->n)))
      || (!(decoded = std::make_unique<int[]>(rm->k)))
      ) {
    std::stringstream errmsg;
    errmsg << "Out of memory";

    reedmuller_free(rm);

    throw std::runtime_error(errmsg.str());
  }

  uint32_t maxcode = reedmuller_maxdecode(rm);
  if (encChipID > maxcode) {
    std::stringstream errmsg;
    errmsg << std::hex << std::setw(8) << std::setfill('0') << encChipID
           << " is larger than the maximum decodeable by RM(2,5)"
           << std::hex << std::setw(8) << std::setfill('0') << maxcode
           << std::dec;
    throw std::out_of_range(errmsg.str());
  }

  for (int j = 0; j < rm->n; ++j)
    encoded.get()[(rm->n-j-1)] = (encChipID>>j) &0x1;

  const int result = reedmuller_decode(rm, encoded.get(), decoded.get());

  if (result) {
    uint16_t decChipID = 0x0;

    char tmp_decoded[1024];
    char *dp = tmp_decoded;

    for (int j=0; j < rm->k; ++j)
      dp += sprintf(dp,"%d", decoded.get()[j]);

    char *p;
    errno = 0;

    const uint32_t conv = strtoul(tmp_decoded, &p, 2);
    if (errno != 0 || *p != '\0') {
      std::stringstream errmsg;
      errmsg << "Unable to convert " << std::string(tmp_decoded) << " to int type";

      reedmuller_free(rm);

      throw std::runtime_error(errmsg.str());
    } else {
      decChipID = conv;
      reedmuller_free(rm);
      return decChipID;
    }
  } else {
    std::stringstream errmsg;
    errmsg << "Unable to decode message 0x"
           << std::hex << std::setw(8) << std::setfill('0') << encChipID
           << ", probably more than " << reedmuller_strength(rm) << " errors";

    reedmuller_free(rm);
    throw std::runtime_error(errmsg.str());
  }
}

uint32_t vfat3::vfatSyncCheck::operator()(const uint16_t& ohN, const uint32_t& mask) const
{
  uint32_t goodVFATs = 0;
  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    std::string regName = "GEM_AMC.OH_LINKS.OH" + std::to_string(ohN) + ".VFAT" + std::to_string(vfatN);
    bool linkGood = utils::readReg(regName+".LINK_GOOD");
    uint32_t linkErrors = utils::readReg(regName+".SYNC_ERR_CNT");
    goodVFATs = goodVFATs | ((linkGood && (linkErrors == 0)) << vfatN);
  }

  // FIXME move this common block into the sync check function, but need to address concern below
  // How to return a value and warning, just always issue a warning, and return
  const uint32_t notmask = ~mask & 0xFFFFFF;

  if ((notmask & goodVFATs) != notmask) {
    std::stringstream errmsg;
    errmsg << "One of the unmasked VFATs is not sync'd: "
           << "goodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\tnotmask: 0x" << std::hex << std::setw(8) << std::setfill('0') << notmask   << std::dec;
    // throw std::runtime_error(errmsg.str());
    LOG4CPLUS_WARN(logger, errmsg.str());
  }

  return goodVFATs;
}

void vfat3::configureVFAT3DACMonitor::operator()(const uint16_t& ohN, const uint32_t& mask, const uint32_t& dacSelect) const
{
  LOG4CPLUS_INFO(logger, "Programming VFAT3 ADC Monitoring for DAC " << dacSelect);

  const uint32_t notmask   = ~mask & 0xFFFFFF;
  const uint32_t goodVFATs = vfatSyncCheck{}(ohN);
  // FIXME, rather than this block, override mask, but the warning message was already sent, how to ensure it's noticed?
  if ((notmask & goodVFATs) != notmask) {
    std::stringstream errmsg;
    errmsg << "One of the unmasked VFATs is not sync'd: "
           << "goodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\tnotmask: 0x" << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
    throw std::runtime_error(errmsg.str());
  }

  // Get ref voltage and monitor gain
  // These should have been set at time of configure
  std::vector<uint32_t> adcVRefValues(oh::VFATS_PER_OH);
  std::vector<uint32_t> monitorGainValues(oh::VFATS_PER_OH);
  adcVRefValues     = oh::broadcastRead{}(ohN, "CFG_VREF_ADC", mask);
  monitorGainValues = oh::broadcastRead{}(ohN, "CFG_MON_GAIN", mask);

  // Loop over all vfats and set the dacSelect
  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    if (!((notmask >> vfatN) & 0x1)) {
      continue;
    }

    // Build global control 4 register
    const uint32_t glbCtr4 = (adcVRefValues.at(vfatN) << 8) + (monitorGainValues.at(vfatN) << 7) + dacSelect;
    const std::string regName = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".CFG_4";
    utils::writeReg(regName, glbCtr4);
  }

  return;
}

void vfat3::configureVFAT3DACMonitorMultiLink::operator()(const uint16_t& ohMask, const std::array<uint32_t, amc::OH_PER_AMC>& vfatMasks, const uint32_t& dacSelect) const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value ("<< supOH << "), request will be reset to register max");

  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    if (!((ohMask >> ohN) & 0x1)) {
      continue;
    }

    const uint32_t vfatMask = amc::getOHVFATMask{}(ohN);

    LOG4CPLUS_INFO(logger, "Programming VFAT3 ADC Monitoring on OH" << ohN << " for DAC selection " << dacSelect);
    configureVFAT3DACMonitor{}(ohN, vfatMask, dacSelect);
  }

  return;
}

void vfat3::configureVFAT3s::operator()(const uint16_t& ohN, const uint32_t& vfatMask) const
{
  const uint32_t notmask   = ~vfatMask & 0xFFFFFF;
  const uint32_t goodVFATs = vfatSyncCheck{}(ohN);

  if ((notmask & goodVFATs) != notmask) {
    std::stringstream errmsg;
    errmsg << "One of the unmasked VFATs is not sync'd: "
           << "goodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\tnotmask: 0x" << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
    throw std::runtime_error(errmsg.str());
  }

  std::string line, dacName;
  uint32_t dacVal = 0x0;

  LOG4CPLUS_INFO(logger, "Loading configuration settings");
  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    if ((notmask >> vfatN) & 0x1) {
      const std::string configFile = "/mnt/persistent/gemdaq/vfat3/config_OH" + std::to_string(ohN) + "_VFAT" + std::to_string(vfatN) + ".txt";
      std::ifstream infile(configFile);
      if (!infile.is_open()) {
        std::stringstream errmsg;
        errmsg << "Could not open config file " << configFile;
        throw std::runtime_error(errmsg.str());
      }

      const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".CFG_";

      std::getline(infile,line);
      while (std::getline(infile,line)) {
        std::stringstream iss(line);
        if (!(iss >> dacName >> dacVal)) {
          std::stringstream errmsg;
          errmsg << "Error reading settings";
          // throw std::runtime_error(errmsg.str());  // FIXME throw or break?
          LOG4CPLUS_ERROR(logger, errmsg.str());
          break;
        } else {
          // regName = regBase + dacName;
          utils::writeReg(regBase + dacName, dacVal);
        }
      }
    }
  }
}

std::vector<uint32_t> vfat3::getChannelRegistersVFAT3::operator()(const uint16_t& ohN, const uint32_t& vfatMask) const
{
  LOG4CPLUS_INFO(logger, "Read channel register settings");

  const uint32_t notmask = ~vfatMask & 0xFFFFFF;

  std::vector<uint32_t> chanRegData(oh::VFATS_PER_OH*128);

  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    if (!((notmask >> vfatN) & 0x1)) {
      for (size_t chan = 0; chan < 128; ++chan) {
        const uint32_t idx = vfatN*128 + chan;
        chanRegData.at(idx) = 0x0; // FIXME 0x0 or 0xdeaddead
        // chanRegData.push_back(0x0); // FIXME 0x0 or 0xdeaddead, only push_back/emplace_back if we don't preallocate
      }
      continue;
    }

    const uint32_t goodVFATs = vfatSyncCheck{}(ohN);
    if (!((goodVFATs >> vfatN) & 0x1)) {
      std::stringstream errmsg;
      errmsg << "The requested VFAT is not sync'd: "
             << "goodVFATs: 0x"       << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
             << "\t requested VFAT: " << vfatN
             << "\tvfatMask: 0x"      << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
      throw std::runtime_error(errmsg.str());
    }

    uint32_t chanAddr = 0x0;
    for (size_t chan = 0; chan < 128; ++chan) {
      const uint32_t idx = vfatN*128 + chan;

      const std::string regName = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".VFAT_CHANNELS.CHANNEL" + std::to_string(chan);
      chanAddr = utils::getAddress(regName);

      // Build the channel register
      LOG4CPLUS_INFO(logger, "Reading channel register for VFAT" << vfatN << " channel " << chan);
      chanRegData.at(idx) = utils::readRawAddress(chanAddr);
      // chanRegData.push_back(readRawAddress(chanAddr));  // FIXME only push_back/emplace_back if we don't preallocate
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
  }

  return chanRegData;
}

std::vector<uint32_t> vfat3::readVFAT3ADC::operator()(const uint16_t& ohN, const bool& useExtRefADC, const uint32_t& vfatMask) const
{
  LOG4CPLUS_INFO(logger, "Reading VFAT3 ADCs for OH" << ohN << " using VFAT mask 0x" << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec);
  // std::vector<uint32_t> data(oh::VFATS_PER_OH);
  if (useExtRefADC) {
    std::vector<uint32_t> tmpdata = oh::broadcastRead{}(ohN, "ADC1_UPDATE", vfatMask);
    std::this_thread::sleep_for(std::chrono::microseconds(20));
    return oh::broadcastRead{}(ohN, "ADC1_CACHED", vfatMask);
  } else {
    std::vector<uint32_t> tmpdata = oh::broadcastRead{}(ohN, "ADC0_UPDATE", vfatMask);
    std::this_thread::sleep_for(std::chrono::microseconds(20));
    return oh::broadcastRead{}(ohN, "ADC0_CACHED", vfatMask);
  }
}

std::map<uint32_t,std::vector<uint32_t>> vfat3::readVFAT3ADCMultiLink::operator()(const uint16_t& ohMask, const bool& useExtRefADC) const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value ("<< supOH << "), request will be reset to register max");

  std::map<uint32_t, std::vector<uint32_t>> adcData;
  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    if (!((ohMask >> ohN) & 0x1)) {
      continue;
    }

    const uint32_t vfatMask = amc::getOHVFATMask{}(ohN);
    adcData[ohN] = readVFAT3ADC{}(ohN, useExtRefADC, vfatMask);
  }

  return adcData;
}

void vfat3::setChannelRegistersVFAT3Simple::operator()(const uint16_t& ohN, const std::vector<uint32_t> &chanRegData, const uint32_t& vfatMask) const
{
  const uint32_t notmask = ~vfatMask & 0xFFFFFF;

  if (chanRegData.size() != oh::VFATS_PER_OH*128) {
    std::stringstream errmsg;
    errmsg << "The provided channel configuration data has the wrong size: "
           << chanRegData.size() << " != " << oh::VFATS_PER_OH*128;
    throw std::runtime_error(errmsg.str());
  }

  LOG4CPLUS_INFO(logger, "Write channel register settings");
  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    if (!((notmask >> vfatN) & 0x1)) {
      continue;
    }

    const uint32_t goodVFATs = vfatSyncCheck{}(ohN); // FIXME does this need to happen inside the loop?
    if (!((goodVFATs >> vfatN) & 0x1)) {
      std::stringstream errmsg;
      errmsg << "The requested VFAT is not sync'd: "
             << "goodVFATs: 0x"       << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
             << "\t requested VFAT: " << vfatN
             << "\tvfatMask: 0x"      << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
      // FIXME should this throw or skip?
      // throw std::runtime_error(errmsg.str());
      LOG4CPLUS_WARN(logger, errmsg.str());
      continue;
    }

    uint32_t chanAddr = 0x0;
    for (size_t chan = 0; chan < 128; ++chan) {
      const uint32_t idx = vfatN*128 + chan;

      const std::string regName = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".VFAT_CHANNELS.CHANNEL" + std::to_string(chan);
      chanAddr = utils::getAddress(regName);
      utils::writeRawAddress(chanAddr, chanRegData[idx]);
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
  }

  return;
}

void vfat3::setChannelRegistersVFAT3::operator()(const uint16_t& ohN,
                                                 const std::vector<uint32_t>& calEnable,
                                                 const std::vector<uint32_t>& masks,
                                                 const std::vector<uint32_t>& trimARM,
                                                 const std::vector<uint32_t>& trimARMPol,
                                                 const std::vector<uint32_t>& trimZCC,
                                                 const std::vector<uint32_t>& trimZCCPol,
                                                 const uint32_t& vfatMask) const
{
  const uint32_t notmask = ~vfatMask & 0xFFFFFF;

  // FIXME should run this check for all vectors?
  // FIXME should get rid of this function altogether?
  if (calEnable.size() != oh::VFATS_PER_OH*128) {
    std::stringstream errmsg;
    errmsg << "The provided channel configuration data calEnable has the wrong size: "
           << calEnable.size() << " != " << oh::VFATS_PER_OH*128;
    throw std::runtime_error(errmsg.str());
  }

  LOG4CPLUS_INFO(logger, "Write channel register settings");
  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    if (!((notmask >> vfatN) & 0x1)) {
      continue;
    }

    const uint32_t goodVFATs = vfatSyncCheck{}(ohN);
    if (!((goodVFATs >> vfatN) & 0x1)) {
      std::stringstream errmsg;
      errmsg << "The requested VFAT is not sync'd: "
             << "goodVFATs: 0x"       << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
             << "\t requested VFAT: " << vfatN
             << "\tvfatMask: 0x"      << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
      // FIXME should this throw or skip?
      // throw std::runtime_error(errmsg.str());
      LOG4CPLUS_WARN(logger, errmsg.str());
      continue;
    }

    uint32_t chanAddr   = 0x0;
    uint32_t chanRegVal = 0x0;
    for (size_t chan=0; chan < 128; ++chan) {
      const uint32_t idx = vfatN*128 + chan;

      const std::string regName = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".VFAT_CHANNELS.CHANNEL" + std::to_string(chan);
      chanAddr = utils::getAddress(regName);

      if (trimARM[idx] > 0x3F || trimARM[idx] < 0x0) {
        std::stringstream errmsg;
        errmsg << "The arming comparator trim value must be positive in the range [0x0,0x3F]. Value given for VFAT"
               << vfatN << " chan " << chan << ": 0x" << std::hex << std::setw(2) << std::setfill('0') << trimARM[idx] << std::dec;
        // FIXME should this throw or skip?
        // throw std::invalid_argument(errmsg.str());
        LOG4CPLUS_WARN(logger, errmsg.str());
        continue;
      } else if (trimZCC[idx] > 0x3F || trimZCC[idx] < 0x0) {
        std::stringstream errmsg;
        errmsg << "The zero crossing comparator trim value must be positive in the range [0x0,0x3F]. Value given for VFAT"
               << vfatN << " chan " << chan << ": 0x" << std::hex << std::setw(2) << std::setfill('0') << trimZCC[idx] << std::dec;
        // FIXME should this throw or skip?
        // throw std::invalid_argument(errmsg.str());
        LOG4CPLUS_WARN(logger, errmsg.str());
        continue;
      }

      // Build the channel register
      LOG4CPLUS_INFO(logger, "Setting channel register for VFAT" << vfatN << " chan " << chan);
      chanRegVal = (calEnable[idx] << 15) + (masks[idx] << 14) + \
        (trimZCCPol[idx] << 13) + (trimZCC[idx] << 7) +          \
        (trimARMPol[idx] << 6) + (trimARM[idx]);
      utils::writeRawAddress(chanAddr, chanRegVal);
      std::this_thread::sleep_for(std::chrono::microseconds(200)); // FIXME why?
    }
  }

  return;
}

std::map<std::string, std::vector<uint32_t>> vfat3::statusVFAT3s::operator()(const uint16_t& ohN) const
{
  std::string regs [] = {
    "CFG_PULSE_STRETCH",
    "CFG_SYNC_LEVEL_MODE",
    "CFG_FP_FE",
    "CFG_RES_PRE",
    "CFG_CAP_PRE",
    "CFG_PT",
    "CFG_SEL_POL",
    "CFG_FORCE_EN_ZCC",
    "CFG_SEL_COMP_MODE",
    "CFG_VREF_ADC",
    "CFG_IREF",
    "CFG_THR_ARM_DAC",
    "CFG_LATENCY",
    "CFG_CAL_SEL_POL",
    "CFG_CAL_DAC",
    "CFG_CAL_MODE",
    "CFG_BIAS_CFD_DAC_2",
    "CFG_BIAS_CFD_DAC_1",
    "CFG_BIAS_PRE_I_BSF",
    "CFG_BIAS_PRE_I_BIT",
    "CFG_BIAS_PRE_I_BLCC",
    "CFG_BIAS_PRE_VREF",
    "CFG_BIAS_SH_I_BFCAS",
    "CFG_BIAS_SH_I_BDIFF",
    "CFG_BIAS_SH_I_BFAMP",
    "CFG_BIAS_SD_I_BDIFF",
    "CFG_BIAS_SD_I_BSF",
    "CFG_BIAS_SD_I_BFCAS",
    "CFG_RUN"
  };

  std::map<std::string, std::vector<uint32_t>> values;

  LOG4CPLUS_INFO(logger, "Reading VFAT3 status");

  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    std::string regBase = "GEM_AMC.OH_LINKS.OH" + std::to_string(ohN) + ".VFAT" + std::to_string(vfatN) + ".";
    for (auto &reg : regs) {
      values[reg].push_back(utils::readReg(regBase+reg));
    }
  }

  return values;
}

std::vector<uint32_t> vfat3::getVFAT3ChipIDs::operator()(const uint16_t& ohN, const uint32_t& vfatMask, const bool& rawID) const
{
  const uint32_t notmask   = ~vfatMask & 0xFFFFFF;
  const uint32_t goodVFATs = vfatSyncCheck{}(ohN);
  if ((notmask & goodVFATs) != notmask) {
    std::stringstream errmsg;
    errmsg << "One of the unmasked VFATs is not sync'd: "
           << "goodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\tnotmask: 0x" << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
    throw std::runtime_error(errmsg.str());
  }

  std::vector<uint32_t> chipIDs;

  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    const std::string regName = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".HW_CHIP_ID";

    if (!((notmask >> vfatN) & 0x1)) {
      chipIDs.push_back(0xdeaddead);
      continue;
    }

    const uint32_t id = utils::readReg(regName);
    uint16_t decChipID = 0x0;
    try {
      decChipID = decodeChipID(id);
      std::stringstream msg;
      msg << "OH" << ohN << "::VFAT" << vfatN << ": chipID is:"
          << std::hex<<std::setw(8)<<std::setfill('0')<<id<<std::dec
          <<"(raw) or "
          << std::hex<<std::setw(8)<<std::setfill('0')<<decChipID<<std::dec
          << "(decoded)";
      LOG4CPLUS_INFO(logger, msg.str());

      if (rawID)
        chipIDs.push_back(id);
      else
        chipIDs.push_back(decChipID);
    } catch (std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "Error decoding chipID: " << e.what()
             << ", returning raw chipID";
      LOG4CPLUS_ERROR(logger,errmsg.str());
      chipIDs.push_back(id);
    }
  }

  return chipIDs;
}

extern "C" {
  const char *module_version_key = "vfat3 v1.0.1";
  const int module_activity_color = 4;

  void module_init(ModuleManager *modmgr)
  {
    utils::initLogging();

    if (memhub_open(&memsvc) != 0) {
      auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
      LOG4CPLUS_ERROR(logger, "Unable to connect to memory service: " << memsvc_get_last_error(memsvc));
      LOG4CPLUS_ERROR(logger, "Unable to load module");
      return;
    }

    xhal::common::rpc::registerMethod<vfat3::configureVFAT3s>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::configureVFAT3DACMonitor>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::configureVFAT3DACMonitorMultiLink>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::getChannelRegistersVFAT3>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::getVFAT3ChipIDs>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::readVFAT3ADC>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::readVFAT3ADCMultiLink>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::setChannelRegistersVFAT3>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::statusVFAT3s>(modmgr);
    xhal::common::rpc::registerMethod<vfat3::vfatSyncCheck>(modmgr);
  }
}
