#include "ctp7_modules/common/calibration_routines.h"
#include "ctp7_modules/server/calibration_routines.h"
#include "ctp7_modules/common/calibration_enums.h"
#include "ctp7_modules/common/amc.h"
#include "ctp7_modules/server/amc.h"
#include "ctp7_modules/common/optohybrid.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"
#include "ctp7_modules/common/vfat3.h"
#include "ctp7_modules/common/hw_constants.h"

#include "xhal/common/rpc/register.h"

#include <chrono>
#include <math.h>
#include <thread>

namespace calibration {
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
}

std::unordered_map<uint32_t, uint32_t> calibration::setSingleChanMask(const uint8_t& ohN, const uint8_t& vfatN, const uint8_t& ch)
{
  std::unordered_map<uint32_t, uint32_t> map_chanOrigMask;
  uint32_t chanMaskAddr;
  const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".VFAT_CHANNELS.CHANNEL";
  for (size_t chan = 0; chan < 128; ++chan) {
    uint32_t chMask = 1;
    if (ch == chan) {
      chMask = 0;
    }
    const std::string regName = regBase + std::to_string(chan) + ".MASK";
    chanMaskAddr = utils::getAddress(regName);
    map_chanOrigMask[chanMaskAddr] = utils::readReg(regName);
    utils::writeRawAddress(chanMaskAddr, chMask);
  }
  return map_chanOrigMask;
}

void calibration::applyChanMask(const std::unordered_map<uint32_t, uint32_t> &chMasks)
{
  for (auto const& chMask : chMasks) {
    utils::writeRawAddress(chMask.first, chMask.second);
  }
}

void calibration::confCalPulse::operator()(const uint16_t& ohN,
                                           const uint32_t& vfatMask,
                                           const uint8_t& ch,
                                           const bool& toggleOn,
                                           const bool& currentPulse,
                                           const uint32_t& calScaleFactor) const
{
  const uint32_t notmask = (~vfatMask)&0xffffff;

  if (ch >= 128) {
    std::stringstream errmsg;
    errmsg << "Invalid channel selection: " << ch << " > 128";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  } else if (ch >= 128 && toggleOn == true) {
    std::stringstream errmsg;
    errmsg << "Enabling a calibration pulse to all channels can result in undefined behaviour";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  } else if (ch == 128 && toggleOn == false) {
    for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN);
      if ((notmask >> vfatN) & 0x1) {
        for (size_t chan = 0; chan < 128; ++chan) {
          utils::writeReg(regBase + ".VFAT_CHANNELS.CHANNEL" + std::to_string(chan) + ".CALPULSE_ENABLE", 0x0);
        }
        utils::writeReg(regBase + ".CFG_CAL_MODE", static_cast<uint32_t>(VFATCalibrationMode::DISABLED));
      }
    }
  } else {
    for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      if ((notmask >> vfatN) & 0x1) {
        const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN);
        if (toggleOn == true) {
          utils::writeReg(regBase + ".VFAT_CHANNELS.CHANNEL" + std::to_string(ch) + ".CALPULSE_ENABLE", 0x1);
          if (currentPulse) {
            utils::writeReg(regBase + ".CFG_CAL_MODE", static_cast<uint32_t>(VFATCalibrationMode::CURRENT));
            // Set cal current pulse scale factor.
            // Q = CAL DUR[s] * CAL DAC * 10nA * CAL FS[%] (00 = 25%, 01 = 50%, 10 = 75%, 11 = 100%)
            utils::writeReg(regBase + ".CFG_CAL_FS", calScaleFactor);
            utils::writeReg(regBase + ".CFG_CAL_DUR", 0x0);
          } else {
            utils::writeReg(regBase + ".CFG_CAL_MODE", static_cast<uint32_t>(VFATCalibrationMode::VOLTAGE));
          }
        } else {
          utils::writeReg(regBase + ".VFAT_CHANNELS.CHANNEL" + std::to_string(ch) + ".CALPULSE_ENABLE", 0x0);
          utils::writeReg(regBase + ".CFG_CAL_MODE", static_cast<uint32_t>(VFATCalibrationMode::DISABLED));
        }
      }
    }
  }

  return;
}

void calibration::dacMonConf::operator()(const uint16_t& ohN, const uint8_t& ch) const
{
  utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.ENABLE", 0x0);
  utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.RESET", 0x1);
  utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.OH_SELECT", ohN);
  if (ch>127) {
    utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.VFAT_CHANNEL_GLOBAL_OR", 0x1);
  } else {
    utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.VFAT_CHANNEL_SELECT", ch);
    utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.VFAT_CHANNEL_GLOBAL_OR", 0x0);
  }
}

void calibration::ttcGenToggle::operator()(const bool& enable) const
{
  if (enable) {
    // Internal TTC generator enabled, TTC cmds from backplane are ignored
    utils::writeReg("GEM_AMC.TTC.GENERATOR.ENABLE", 0x1);
  } else {
    // Internal TTC generator disabled, TTC cmds from backplane
    utils::writeReg("GEM_AMC.TTC.GENERATOR.ENABLE", 0x0);
  }
}

void calibration::ttcGenConf::operator()(const uint32_t& pulseDelay, const uint32_t& L1Ainterval,
                                         const bool& enable) const
{
  LOG4CPLUS_INFO(logger, "Entering ttcGenConf");

  utils::writeReg("GEM_AMC.TTC.GENERATOR.RESET", 0x1);
  utils::writeReg("GEM_AMC.TTC.GENERATOR.CYCLIC_L1A_GAP", L1Ainterval);
  utils::writeReg("GEM_AMC.TTC.GENERATOR.CYCLIC_CALPULSE_TO_L1A_GAP", pulseDelay);

  ttcGenToggle{}(enable);
}

std::vector<uint32_t> calibration::genScan::operator()(const uint16_t& ohN,
                                                       const uint32_t& vfatMask,
                                                       const uint8_t& ch,
                                                       const bool& useCalPulse,
                                                       const bool& currentPulse,
                                                       const uint32_t& calScaleFactor,
                                                       const uint32_t& nevts,
                                                       const uint32_t& dacMin,
                                                       const uint32_t& dacMax,
                                                       const uint32_t& dacStep,
                                                       const std::string& scanReg,
                                                       const bool& useUltra,
                                                       const bool& useExtTrig) const
{
  const uint32_t notmask = (~vfatMask)&0xffffff;

  // std::vector<uint32_t> outData(oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep);
  std::vector<uint32_t> outData;

  const uint32_t goodVFATs = vfat3::vfatSyncCheck{}(ohN);
  if ((notmask & goodVFATs) != notmask) {
    std::stringstream errmsg;
    errmsg << "One of the unmasked VFATs is not sync'd: "
           << "goodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\tnotmask: 0x" << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
    throw std::runtime_error(errmsg.str());
  } else if (currentPulse && calScaleFactor > 3) {
    std::stringstream errmsg;
    errmsg << "Bad value for CFG_CAL_FS: 0x"
           << std::hex << calScaleFactor << std::dec
           << ". Possible values are {0b00, 0b01, 0b10, 0b11}";
    throw std::runtime_error(errmsg.str());
  }

  if (useCalPulse) {
    try {
      confCalPulse{}(ohN, vfatMask, ch, true, currentPulse, calScaleFactor);
    } catch (std::runtime_error const& e) {
      std::stringstream errmsg;
      errmsg << "Unable to configure CalPulse ON for ohN " << ohN
             << " vfatMask 0x" << std::hex <<std::setw(8) << std::setfill('0') << vfatMask << std::dec
             << " chanel " << ch << std::endl
             << ". Caught " << e.what();
      // rethrow here or return?
      throw std::runtime_error(errmsg.str());
    }
  }

  uint32_t daqMonAddr[oh::VFATS_PER_OH];
  const uint32_t l1CntAddr = utils::getAddress("GEM_AMC.TTC.CMD_COUNTERS.L1A");

  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    const std::string regName = "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.VFAT" + std::to_string(vfatN) + ".GOOD_EVENTS_COUNT";
    daqMonAddr[vfatN] = utils::getAddress(regName);
  }

  // TTC Config
  if (useExtTrig) {
    utils::writeReg("GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x0);
    utils::writeReg("GEM_AMC.TTC.CTRL.CNT_RESET",  0x1);
  } else {
    utils::writeReg("GEM_AMC.TTC.GENERATOR.CYCLIC_L1A_COUNT", nevts);
    utils::writeReg("GEM_AMC.TTC.GENERATOR.SINGLE_RESYNC",    0x1);
  }

  // Configure VFAT_DAQ_MONITOR
  dacMonConf{}(ohN, ch);

  // Scan over DAC values
  for (uint32_t dacVal = dacMin; dacVal <= dacMax; dacVal += dacStep) {
    for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      if ((notmask >> vfatN) & 0x1) {
        utils::writeReg("GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".CFG_" + scanReg, dacVal);
      }
    }

    // Reset and enable the VFAT_DAQ_MONITOR
    utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.RESET",  0x1);
    utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.ENABLE", 0x1);

    // Start the triggers
    if (useExtTrig) {
      utils::writeReg("GEM_AMC.TTC.CTRL.CNT_RESET",  0x1);
      utils::writeReg("GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x1);

      uint32_t l1aCnt = 0;
      while (l1aCnt < nevts) {
        l1aCnt = utils::readRawAddress(l1CntAddr);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
      }

      utils::writeReg("GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x0);
      l1aCnt = utils::readRawAddress(l1CntAddr);
    } else {
      utils::writeReg("GEM_AMC.TTC.GENERATOR.CYCLIC_START", 0x1);
      if (utils::readReg("GEM_AMC.TTC.GENERATOR.ENABLE")) {
        while (utils::readReg("GEM_AMC.TTC.GENERATOR.CYCLIC_RUNNING")) {
          std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
      }
    }

    // Stop the DAQ monitor counters from incrementing
    utils::writeReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.ENABLE", 0x0);

    // Read the DAQ Monitor counters
    for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      if (!((notmask >> vfatN) & 0x1)) {
        outData.push_back(0x0);
        continue;
      }

      // const uint32_t idx = vfatN*(dacMax-dacMin+1)/dacStep+(dacVal-dacMin)/dacStep;
      outData.push_back(utils::readRawAddress(daqMonAddr[vfatN]));

      LOG4CPLUS_DEBUG(logger, scanReg << " Value: " << dacVal
                      << "; Readback Val: "
                      << utils::readReg("GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".CFG_" + scanReg)
                      << "; Nhits: "
                      << utils::readReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.VFAT" + std::to_string(vfatN) + ".CHANNEL_FIRE_COUNT")
                      << "; Nev: "
                      << utils::readReg("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.VFAT" + std::to_string(vfatN) + ".GOOD_EVENTS_COUNT")
                      << "; CFG_THR_ARM: "
                      << utils::readReg("GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".CFG_THR_ARM_DAC")
                      );
    }
  }

  // If the calpulse for channel ch was turned on, turn it off
  if (useCalPulse) {
    try {
      confCalPulse{}(ohN, vfatMask, ch, false, currentPulse, calScaleFactor);
    } catch (const std::runtime_error &e) {
      std::stringstream errmsg;
      errmsg << "Unable to configure CalPulse OFF for OH" << ohN
             << " with vfatMask 0x" << std::hex << std::setw(6) << std::setfill('0') << vfatMask << std::dec
             << " channel " << ch
             << ". Caught " << e.what();
      throw std::runtime_error(errmsg.str());
    }
  }

  return outData;
}

std::map<uint32_t, std::vector<uint32_t>> calibration::genChannelScan::operator()(const uint16_t& ohN,
                                                                                  const uint32_t& vfatMask,
                                                                                  const bool& useCalPulse,
                                                                                  const bool& currentPulse,
                                                                                  const uint32_t& calScaleFactor,
                                                                                  const uint32_t& nevts,
                                                                                  const uint32_t& dacMin,
                                                                                  const uint32_t& dacMax,
                                                                                  const uint32_t& dacStep,
                                                                                  const std::string& scanReg,
                                                                                  const bool& useUltra=false,
                                                                                  const bool& useExtTrig=false) const
{
  std::map<uint32_t, std::vector<uint32_t>> outData;

  for (size_t ch = 0; ch < 128; ++ch) {
    outData[ch] = genScan{}(ohN, vfatMask, ch, useCalPulse, currentPulse, calScaleFactor, nevts, dacMin, dacMax, dacStep, scanReg, useUltra, useExtTrig);
  }
  return outData;
}

std::map<uint32_t, uint32_t> calibration::sbitRateScan::operator()(const uint16_t& ohN,
                                                                   const uint32_t& vfatMask,
                                                                   const uint8_t& ch,
                                                                   const uint16_t& dacMin,
                                                                   const uint16_t& dacMax,
                                                                   const uint16_t& dacStep,
                                                                   const std::string& scanReg,
                                                                   const uint32_t& waitTime,
                                                                   const bool& invertVFATPos) const
{
  std::map<uint32_t, uint32_t> outData;

  // Hard code possible vfatMask values and how they map to vfatN
  const std::unordered_map<uint32_t,uint32_t> vfatMask2vfatN {
    {0xfffffe,  0}, {0xfffffd,  1}, {0xfffffb,  2}, {0xfffff7,  3},
    {0xffffef,  4}, {0xffffdf,  5}, {0xffffbf,  6}, {0xffff7f,  7},
    {0xfffeff,  8}, {0xfffdff,  9}, {0xfffbff, 10}, {0xfff7ff, 11},
    {0xffefff, 12}, {0xffdfff, 13}, {0xffbfff, 14}, {0xff7fff, 15},
    {0xfeffff, 16}, {0xfdffff, 17}, {0xfbffff, 18}, {0xf7ffff, 19},
    {0xefffff, 20}, {0xdfffff, 21}, {0xbfffff, 22}, {0x7fffff, 23}
  };

  // Determine vfatN based on input vfatMask
  auto const&& vfatNptr = vfatMask2vfatN.find(vfatMask);
  if (vfatNptr == vfatMask2vfatN.end()) {
    std::stringstream errmsg;
    errmsg << "Input vfatMask: 0x"
           << std::hex << std::setw(6) << std::setfill('0') << vfatMask << std::dec
           << "not recgonized. Please make sure all but one VFAT is unmasked and then try again";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
  const uint32_t vfatN     = (invertVFATPos) ? 23 - vfatNptr->second : vfatNptr->second;
  const uint32_t goodVFATs = vfat3::vfatSyncCheck{}(ohN);
  if (!((goodVFATs >> vfatN) & 0x1)) {
    std::stringstream errmsg;
    errmsg << "The requested VFAT is not sync'd: "
           << "goodVFATs: 0x"       << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\t requested VFAT: " << vfatN
           << "\tvfatMask: 0x"      << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
    throw std::runtime_error(errmsg.str());
  }

  // If ch!=128 store the original channel mask settings
  // Then mask all other channels except for channel ch
  std::unordered_map<uint32_t, uint32_t> map_chanOrigMask;
  if (ch != 128)
    map_chanOrigMask = setSingleChanMask(ohN,vfatN,ch);

  const uint32_t ohTrigRateAddr = utils::getAddress("GEM_AMC.TRIGGER.OH" + std::to_string(ohN) + ".TRIGGER_RATE");

  // Store the original OH VFAT mask, and then reset it
  const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN);
  const uint32_t ohVFATMaskAddr = utils::getAddress(regBase + ".FPGA.TRIG.CTRL.VFAT_MASK");
  const uint32_t vfatMaskOrig   = utils::readRawAddress(ohVFATMaskAddr);
  utils::writeRawAddress(ohVFATMaskAddr, vfatMask);

  utils::writeReg("GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

  // Loop from dacMin to dacMax in steps of dacStep
  for (uint32_t dacVal = dacMin; dacVal <= dacMax; dacVal += dacStep) {
    utils::writeReg(regBase + ".GEB.VFAT" + std::to_string(vfatN) + ".CFG_" + scanReg, dacVal);
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
    outData[dacVal] = utils::readRawAddress(ohTrigRateAddr);
  }

  // Restore the original channel masks if specific channel was requested
  if (ch != 128)
    applyChanMask(map_chanOrigMask);

  // Restore the original vfatMask
  utils::writeRawAddress(ohVFATMaskAddr, vfatMaskOrig);

  return outData;
}

std::map<uint32_t, std::vector<uint32_t>> calibration::sbitRateScanParallel::operator()(const uint8_t& ch,
                                                                                        const uint16_t& dacMin,
                                                                                        const uint16_t& dacMax,
                                                                                        const uint16_t& dacStep,
                                                                                        const std::string& scanReg,
                                                                                        const uint16_t& ohMask,
                                                                                        const uint32_t& waitTime) const
{
  if (ohMask > 0xfff) {
    std::stringstream errmsg;
    errmsg << "sbitRateScanParallel supports only up to 12 OptoHybrids per CTP7"; // FIXME GE1/1 CTP7 specific
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }

  std::map<uint32_t, std::vector<uint32_t>> outData;

  uint32_t vfatmask[amc::OH_PER_AMC] = {0};
  std::unordered_map<uint32_t, uint32_t> origVFATmasks[amc::OH_PER_AMC][oh::VFATS_PER_OH];

  for (size_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
    if ((ohMask >> ohN) & 0x1) {
      vfatmask[ohN] = amc::getOHVFATMask{}(ohN);
      LOG4CPLUS_INFO(logger, "VFAT Mask for OH" << ohN << " Determined to be 0x"
                     << std::hex << std::setw(8) << std::setfill('0') << vfatmask[ohN] << std::dec);

      // If ch!=128 store the original channel mask settings
      if (ch != 128) {
        // Then mask all other channels except for channel ch
        const uint32_t notmask = ~vfatmask[ohN] & 0xffffff;
        for (size_t vfat = 0; vfat < oh::VFATS_PER_OH; ++vfat) {
          if (!((notmask >> vfat) & 0x1))
            continue;
          origVFATmasks[ohN][vfat] = setSingleChanMask(ohN,vfat,ch);
        }
      }
    }
  }

  // Get the s-bit Rate Monitor Address
  uint32_t ohTrigRateAddr[amc::OH_PER_AMC][oh::VFATS_PER_OH + 1];
  for (uint16_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
    if ((ohMask >> ohN) & 0x1) {
      const std::string regName = "GEM_AMC.TRIGGER.OH" + std::to_string(ohN) + ".TRIGGER_RATE";
      ohTrigRateAddr[ohN][oh::VFATS_PER_OH] = utils::getAddress(regName);
      for (size_t vfat = 0; vfat < oh::VFATS_PER_OH; ++vfat) {
        const std::string vfatRegName = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".FPGA.TRIG.CNT.VFAT" + std::to_string(vfat) + "_SBITS";
        ohTrigRateAddr[ohN][vfat] = utils::getAddress(vfatRegName);
      }
    }
  }

  // Take the VFATs out of slow control only mode
  utils::writeReg("GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

  std::unordered_map<uint32_t,uint32_t> map_origSBITPersist;
  std::unordered_map<uint32_t,uint32_t> map_origSBITTimeMax;
  for (uint16_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
    if ((ohMask >> ohN) & 0x1) {
      const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".FPGA.TRIG.CNT.SBIT_CNT_";
      map_origSBITPersist[ohN] = utils::readReg(regBase + "PERSIST");
      map_origSBITTimeMax[ohN] = utils::readReg(regBase + "TIME_MAX");
      // reset all counters after SBIT_CNT_TIME_MAX
      utils::writeReg(regBase + "PERSIST", 0x0);
      // count for a number of BX's specified by waitTime
      utils::writeReg(regBase + "TIME_MAX", static_cast<uint32_t>(0x02638e98*waitTime));
    }
  }

  for (uint32_t dacVal = dacMin; dacVal <= dacMax; dacVal += dacStep) {
    LOG4CPLUS_INFO(logger, "Setting " << scanReg << " to " << dacVal
                   << " for all OptoHybrids in 0x"
                   << std::hex << std::setw(3) << std::setfill('0') << ohMask << std::dec);

    // Set the scan register value
    for (uint16_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
      if ((ohMask >> ohN) & 0x1) {
        const uint32_t notmask = ~vfatmask[ohN] & 0xffffff;
        const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT";
        for (size_t vfat = 0; vfat < oh::VFATS_PER_OH; ++vfat) {
          if (!((notmask >> vfat) & 0x1))
            continue;
          utils::writeReg(regBase + std::to_string(vfat) + ".CFG_" + scanReg, dacVal);
        }
      }
    }

    // Reset the counters
    for (size_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
      if ((ohMask >> ohN) & 0x1) {
        utils::writeReg("GEM_AMC.OH.OH" + std::to_string(ohN) + ".FPGA.TRIG.CNT.RESET", 0x1);
      }
    }

    std::this_thread::sleep_for(std::chrono::seconds(waitTime));

    // Read the counters
    for (size_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
      if ((ohMask >> ohN) & 0x1) {
        const uint32_t notmask = ~vfatmask[ohN] & 0xffffff;
        for (size_t vfat = 0; vfat < oh::VFATS_PER_OH; ++vfat) {
          if (!((notmask >> vfat) & 0x1)) {
            outData[dacVal].push_back(0x0);
          } else {
            outData[dacVal].push_back(utils::readRawAddress(ohTrigRateAddr[ohN][vfat]));
          }
        }
        outData[dacVal].push_back(utils::readRawAddress(ohTrigRateAddr[ohN][oh::VFATS_PER_OH]));
      }
    }
  }

  // Restore the original s-bit counter persist setting
  for (size_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
    if ((ohMask >> ohN) & 0x1) {
      const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".FPGA.TRIG.CNT.SBIT_CNT_";
      utils::writeReg(regBase + "PERSIST", map_origSBITPersist[ohN]);
      utils::writeReg(regBase + "MAX",     map_origSBITTimeMax[ohN]);
    }
  }

  // Restore the original channel masks if specific channel was requested
  if (ch != 128) {
    for (size_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
      if ((ohMask >> ohN) & 0x1) {
        const uint32_t notmask = ~vfatmask[ohN] & 0xffffff;
        for (size_t vfat = 0; vfat < oh::VFATS_PER_OH; ++vfat) {
          if (!((notmask >> vfat) & 0x1))
            continue;
          applyChanMask(origVFATmasks[ohN][vfat]);
        }
      }
    }
  }

  return outData;
}

std::vector<uint32_t> calibration::checkSbitMappingWithCalPulse::operator()(const uint16_t& ohN,
                                                                      const uint32_t& vfatN,
                                                                      const uint32_t& vfatMask,
                                                                      const bool& useCalPulse,
                                                                      const bool& currentPulse,
                                                                      const uint32_t& calScaleFactor,
                                                                      const uint32_t& nevts,
                                                                      const uint32_t& L1Ainterval,
                                                                      const uint32_t& pulseDelay) const
{
  const uint32_t notmask   = ~vfatMask & 0xffffff;
  const uint32_t goodVFATs = vfat3::vfatSyncCheck{}(ohN);
  if ((notmask & goodVFATs) != notmask) {
    std::stringstream errmsg;
    errmsg << "One of the unmasked VFATs is not sync'd: "
           << "goodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\tnotmask: 0x" << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
    throw std::runtime_error(errmsg.str());
  } else if (currentPulse && calScaleFactor > 3) {
    std::stringstream errmsg;
    errmsg << "Bad value for CFG_CAL_FS: 0x"
           << std::hex << calScaleFactor << std::dec
           << ". Possible values are {0b00, 0b01, 0b10, 0b11}";
    throw std::runtime_error(errmsg.str());
  }

  std::vector<uint32_t> outData;

  // Get current channel register data, mask all channels and disable calpulse
  auto const chanRegData_orig = vfat3::getChannelRegistersVFAT3{}(ohN, vfatMask);
  std::vector<uint32_t> chanRegData_tmp(oh::CHANNELS_PER_OH);
  for (size_t idx = 0; idx < oh::CHANNELS_PER_OH; ++idx) {
    chanRegData_tmp[idx]=chanRegData_orig[idx]+(0x1 << 14); // set channel mask to true
    chanRegData_tmp[idx]=chanRegData_tmp[idx]-(0x1 << 15);  // disable calpulse
  }
  vfat3::setChannelRegistersVFAT3Simple{}(ohN, chanRegData_tmp, vfatMask);

  // Setup TTC Generator
  ttcGenConf{}(pulseDelay, L1Ainterval, true);
  utils::writeReg("GEM_AMC.TTC.GENERATOR.SINGLE_RESYNC", 0x1);
  utils::writeReg("GEM_AMC.TTC.GENERATOR.CYCLIC_L1A_COUNT", 0x1); // One pulse at a time
  uint32_t addrTtcStart = utils::getAddress("GEM_AMC.TTC.GENERATOR.CYCLIC_START");

  // Set all chips out of run mode
  oh::broadcastWrite{}(ohN, "CFG_RUN", 0x0, vfatMask);

  // Take the VFATs out of slow control only mode
  utils::writeReg("GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

  // Setup the s-bit monitor
  const uint32_t nclusters = 8;
  utils::writeReg("GEM_AMC.TRIGGER.SBIT_MONITOR.OH_SELECT", ohN);
  const uint32_t addrSbitMonReset = utils::getAddress("GEM_AMC.TRIGGER.SBIT_MONITOR.RESET");
  uint32_t addrSbitCluster[nclusters];
  for (size_t iCluster = 0; iCluster < nclusters; ++iCluster) {
    addrSbitCluster[iCluster] = utils::getAddress("GEM_AMC.TRIGGER.SBIT_MONITOR.CLUSTER" + std::to_string(iCluster));
  }

  if (!((notmask >> vfatN) & 0x1)) {
    std::stringstream errmsg;
    errmsg << "The VFAT of interest " << vfatN << " should not be part of the vfatMask 0x"
           << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
    throw std::runtime_error(errmsg.str());
  }

  const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN);
  const std::string vfatRegBase = regBase + ".GEB.VFAT" + std::to_string(vfatN);
  // mask all other vfats from trigger
  utils::writeReg(regBase + ".FPGA.TRIG.CTRL.VFAT_MASK", 0xffffff&~(0x1<<(vfatN)));

  // Place this vfat into run mode
  utils::writeReg(vfatRegBase + ".CFG_RUN", 0x1);

  for (size_t chan = 0; chan < 128; ++chan) {
    utils::writeReg(vfatRegBase + ".VFAT_CHANNELS.CHANNEL" + std::to_string(chan) + ".MASK", 0x0);

    // Turn on the calpulse for this channel
    try {
      confCalPulse{}(ohN, ~((0x1)<<vfatN) & 0xffffff, chan, useCalPulse, currentPulse, calScaleFactor);
    } catch (const std::runtime_error &e) {
      std::stringstream errmsg;
      errmsg << "Unable to configure CalPulse " << useCalPulse
             << " for " << ohN << " mask 0x"
             << std::hex << std::setw(8) << std::setfill('0') << (~(0x1<<vfatN)&0xffffff) << std::dec
             << "channel " << chan
             << ". Caught " << e.what();
      throw std::runtime_error(errmsg.str());
      // Calibration pulse is not configured correctly
    }

    // Start Pulsing
    for (size_t iPulse = 0; iPulse < nevts; ++iPulse) {
      // Reset monitors
      utils::writeRawAddress(addrSbitMonReset, 0x1);

      // Start the TTC Generator
      if (useCalPulse) {
        utils::writeRawAddress(addrTtcStart, 0x1);
      }

      // Sleep for 200 us + pulseDelay * 25 ns * (0.001 us / ns)
      std::this_thread::sleep_for(std::chrono::microseconds(200+static_cast<int>(ceil(pulseDelay*25*0.001))));

      // Check clusers
      for (size_t cluster = 0; cluster < nclusters; ++cluster) {
        // bits [10:0] is the address of the cluster
        // bits [14:12] is the cluster size
        // bits 15 and 11 are not used
        const uint32_t thisCluster = utils::readRawAddress(addrSbitCluster[cluster]);
        const uint32_t clusterSize = (thisCluster >> 12) & 0x7;
        const uint32_t sbitAddress = (thisCluster & 0x7ff);
        const bool isValid = (sbitAddress < oh::SBITS_PER_OH); // Possible values are [0,(oh::SBITS_PER_OH)-1]
        const int vfatObserved = 7-static_cast<int>(sbitAddress/192)+static_cast<int>((sbitAddress%192)/64)*8;
        const int sbitObserved = sbitAddress % 64;

        outData.push_back(((clusterSize&0x7)<<27)
                          + ((isValid&0x1)<<26)
                          + ((vfatObserved&0x1f)<<21)
                          + ((vfatN&0x1f)<<16)
                          + ((sbitObserved&0xff)<<8)
                          + (chan&0xff));

        if (isValid) {
          LOG4CPLUS_INFO(logger, "valid sbit data: useCalPulse " << useCalPulse
                         << "; thisClstr 0x" << std::hex << thisCluster << std::dec
                         << "; clstrSize 0x" << std::hex << clusterSize << std::dec
                         << "; sbitAddr 0x"  << std::hex << sbitAddress << std::dec
                         << "; isValid 0x"   << std::hex << isValid << std::dec
                         << "; vfatN "       << vfatN
                         << "; vfatObs "     << vfatObserved
                         << "; chan "        << chan
                         << "; sbitObs "     << sbitObserved
                         );
        }
      }
    }

    // Turn off the calpulse for this channel
    try {
      confCalPulse{}(ohN, (~((0x1)<<vfatN)&0xffffff), chan, false, currentPulse, calScaleFactor);
    } catch (const std::runtime_error &e) {
      std::stringstream errmsg;
      errmsg << "Unable to configure CalPulse OFF for OH" << ohN << " with mask 0x"
             << std::hex << std::setw(6) << std::setfill('0') << (~((0x1)<<vfatN)&0xffffff) << std::dec
             << " channel " << chan
             << ". Caught " << e.what();
      throw std::runtime_error(errmsg.str());
      // Calibration pulse is not configured correctly
    }

    // mask this channel
    utils::writeReg("GEM_AMC.OH.OH" + std::to_string(ohN)
                    + ".GEB.VFAT" + std::to_string(vfatN)
                    + ".VFAT_CHANNELS.CHANNEL" + std::to_string(chan) + ".MASK", 0x1);
  }

  utils::writeReg(vfatRegBase + ".CFG_RUN", 0x0);

  ttcGenToggle{}(false);

  // Return channel register settings to their original values
  vfat3::setChannelRegistersVFAT3Simple{}(ohN, chanRegData_orig, vfatMask);

  // Set trigger vfat mask for this OH back to 0
  utils::writeReg(regBase + "i.FPGA.TRIG.CTRL.VFAT_MASK", 0x0);

  return outData;
}

std::map<std::string, std::vector<uint32_t>> calibration::checkSbitRateWithCalPulse::operator()(const uint16_t& ohN,
                                                                                                const uint32_t& vfatN,
                                                                                                const uint32_t& vfatMask,
                                                                                                const bool& useCalPulse,
                                                                                                const bool& currentPulse,
                                                                                                const uint32_t& calScaleFactor,
                                                                                                const uint32_t& waitTime,
                                                                                                const uint32_t& pulseRate,
                                                                                                const uint32_t& pulseDelay) const
{
  const uint32_t notmask   = ~vfatMask & 0xffffff;
  const uint32_t goodVFATs = vfat3::vfatSyncCheck{}(ohN);
  if ((notmask & goodVFATs) != notmask) {
    std::stringstream errmsg;
    errmsg << "One of the unmasked VFATs is not sync'd: "
           << "goodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\tnotmask: 0x" << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
    throw std::runtime_error(errmsg.str());
  } else if (currentPulse && calScaleFactor > 3) {
    std::stringstream errmsg;
    errmsg << "Bad value for CFG_CAL_FS: 0x"
           << std::hex << calScaleFactor << std::dec
           << ". Possible values are {0b00, 0b01, 0b10, 0b11}";
    throw std::runtime_error(errmsg.str());
  }

  std::map<std::string, std::vector<uint32_t>> outData;

  // Get current channel register data, mask all channels and disable calpulse
  LOG4CPLUS_INFO(logger, "Storing VFAT3 channel registers for ohN " << ohN);
  auto const chanRegData_orig = vfat3::getChannelRegistersVFAT3{}(ohN, vfatMask);
  std::vector<uint32_t> chanRegData_tmp(oh::CHANNELS_PER_OH);
  LOG4CPLUS_INFO(logger, "Masking all channels and disabling CalPulse for VFATs on ohN " << ohN);

  for (size_t idx = 0; idx < oh::CHANNELS_PER_OH; ++idx) {
    chanRegData_tmp.at(idx) = chanRegData_orig.at(idx)+(0x1 << 14); // set channel mask to true
    chanRegData_tmp.at(idx) = chanRegData_tmp.at(idx) -(0x1 << 15); // disable calpulse
  }
  vfat3::setChannelRegistersVFAT3Simple{}(ohN, chanRegData_tmp, vfatMask);

  // Setup TTC Generator
  uint32_t L1Ainterval = 0;
  if (pulseRate > 0) {
    L1Ainterval = int(40079000/pulseRate);
  } else {
    L1Ainterval = 0;
  }

  const uint32_t addrTTCReset = utils::getAddress("GEM_AMC.TTC.GENERATOR.RESET");
  const uint32_t addrTTCStart = utils::getAddress("GEM_AMC.TTC.GENERATOR.CYCLIC_START");

  const std::string regBase     = "GEM_AMC.OH.OH" + std::to_string(ohN);
  const std::string vfatRegBase = regBase + ".GEB.VFAT" + std::to_string(vfatN);

  // Get Trigger addresses
  uint32_t ohTrigRateAddr[oh::VFATS_PER_OH + 2];
  // idx 0->oh::VFATS_PER_OH VFAT counters; idx oh::VFATS_PER_OH+1 - rate measured by OH FPGA; last idx - rate measured by CTP7
  for (size_t vfat = 0; vfat < oh::VFATS_PER_OH; ++vfat) {
    ohTrigRateAddr[vfat] = utils::getAddress(regBase + ".FPGA.TRIG.CNT.VFAT" + std::to_string(vfat) + "_SBITS");
  }
  ohTrigRateAddr[oh::VFATS_PER_OH] = utils::getAddress(regBase + ".FPGA.TRIG.CNT.CLUSTER_COUNT");
  ohTrigRateAddr[oh::VFATS_PER_OH + 1] = utils::getAddress("GEM_AMC.TRIGGER.OH" + std::to_string(ohN) + ".TRIGGER_RATE");
  const uint32_t addTrgCntResetOH   = utils::getAddress(regBase + ".FPGA.TRIG.CNT.RESET");
  const uint32_t addTrgCntResetCTP7 = utils::getAddress("GEM_AMC.TRIGGER.CTRL.CNT_RESET");

  // Set all chips out of run mode
  LOG4CPLUS_INFO(logger, "Writing CFG_RUN to 0x0 for all VFATs on OH" << ohN
                 << " using vfatMask 0x" << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec);
  oh::broadcastWrite{}(ohN, "CFG_RUN", 0x0, vfatMask);

  // Take the VFATs out of slow control only mode
  LOG4CPLUS_INFO(logger, "Taking VFAT3s out of slow control only mode");
  utils::writeReg("GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

  // Prep the s-bit counters
  LOG4CPLUS_INFO(logger, "Preping s-bit counters for OH" << ohN);
  utils::writeReg(regBase + ".FPGA.TRIG.CNT.SBIT_CNT_PERSIST", 0x0); // reset all counters after SBIT_CNT_TIME_MAX
  utils::writeReg(regBase + ".FPGA.TRIG.CNT.SBIT_CNT_TIME_MAX", uint32_t(0x02638e98*waitTime/1000.));
  // count for a number of BX's specified by waitTime

  if (!((notmask >> vfatN) & 0x1)) {
    std::stringstream errmsg;
    errmsg << "The vfat of interest "
           << vfatN << " should not be part of the vfats to be masked: 0x"
           << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec;
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }

  const uint32_t mask = (~(0x1<<vfatN))&0xffffff;
  LOG4CPLUS_INFO(logger, "Masking VFATs 0x"
                 << std::hex << std::setw(8) << std::setfill('0') << mask << std::dec
                 << " from trigger in ohN " << ohN);
  utils::writeReg(regBase + ".FPGA.TRIG.CTRL.VFAT_MASK", mask);

  LOG4CPLUS_INFO(logger, "Placing VFAT" << vfatN << " on OH" << ohN << " in run mode");
  utils::writeReg(regBase + ".GEB.VFAT" + std::to_string(vfatN) + ".CFG_RUN", 0x1);

  LOG4CPLUS_INFO(logger, "Looping over all channels of VFAT" << vfatN << " on OH" << ohN);
  for (size_t chan = 0; chan < 128; ++chan) {
    const std::string chRegBase = vfatRegBase + ".VFAT_CHANNELS.CHANNEL" + std::to_string(chan);
    LOG4CPLUS_INFO(logger, "Unmasking channel " << chan << " on VFAT" << vfatN << " of OH" << ohN);
    utils::writeReg(chRegBase + ".MASK", 0x0);

    LOG4CPLUS_INFO(logger, "Enabling CalPulse for channel " << chan << " on VFAT" << vfatN << " of OH" << ohN);
    try {
      confCalPulse{}(ohN, mask, chan, useCalPulse, currentPulse, calScaleFactor);
    } catch (const std::runtime_error &e) { // FIXME proper exception handling
      std::stringstream errmsg;
      errmsg << "Unable to configure CalPulse " << useCalPulse << " for " << ohN
             << " mask 0x" << std::hex << std::setw(8) << std::setfill('0') << mask << std::dec
             << " channel " << chan
             << ". Caught " << e.what();
      throw std::runtime_error(errmsg.str());
      // Calibration pulse is not configured correctly
    }

    // Reset counters
    LOG4CPLUS_INFO(logger, "Reseting trigger counters on OH & CTP7");
    utils::writeRawAddress(addTrgCntResetOH,   0x1);
    utils::writeRawAddress(addTrgCntResetCTP7, 0x1);

    // Start the TTC Generator
    LOG4CPLUS_INFO(logger, "Configuring TTC Generator to use OH" << ohN
                   << " with pulse delay " << pulseDelay
                   << " and L1Ainterval " << L1Ainterval);
    ttcGenConf{}(pulseDelay, L1Ainterval, true);
    utils::writeReg("GEM_AMC.TTC.GENERATOR.SINGLE_RESYNC",    0x1);
    utils::writeReg("GEM_AMC.TTC.GENERATOR.CYCLIC_L1A_COUNT", 0x0);
    LOG4CPLUS_INFO(logger, "Starting TTC Generator");
    utils::writeRawAddress(addrTTCStart, 0x1);

    // Sleep for waitTime of milliseconds
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));

    // Read All Trigger Registers
    LOG4CPLUS_INFO(logger, "Reading trigger counters");
    outData["CTP7"].push_back(utils::readRawAddress(ohTrigRateAddr[oh::VFATS_PER_OH + 1]));
    outData["FPGA"].push_back(utils::readRawAddress(ohTrigRateAddr[oh::VFATS_PER_OH]));  // *waitTime/1000.);
    outData["VFAT"].push_back(utils::readRawAddress(ohTrigRateAddr[vfatN]));             // *waitTime/1000.;

    // Reset the TTC Generator
    LOG4CPLUS_INFO(logger, "Stopping TTC Generator");
    utils::writeRawAddress(addrTTCReset, 0x1);

    // Turn off the calpulse for this channel
    LOG4CPLUS_INFO(logger, "Disabling CalPulse for channel " << chan
                   << " on VFAT" << vfatN << " of OH" << ohN);
    const uint32_t mask = (~(0x1<<vfatN))&0xffffff;
    try {
      confCalPulse{}(ohN, mask, chan, false, currentPulse, calScaleFactor);
    } catch (const std::runtime_error &e) { // FIXME proper exception handling
      std::stringstream errmsg;
      errmsg << "Unable to disable CalPulse for " << ohN
             << " mask 0x" << std::hex << std::setw(8) << std::setfill('0') << mask << std::dec
             << "channel " << chan
             << ". Caught " << e.what();
      throw std::runtime_error(errmsg.str());
      // Calibration pulse is not configured correctly
    }

    LOG4CPLUS_INFO(logger, "Masking channel " << chan << " on VFAT" << vfatN << " of OH" << ohN);
    utils::writeReg(chRegBase + std::to_string(chan) + ".MASK", 0x1);
  }

  // Place this vfat out of run mode FIXME why??
  LOG4CPLUS_INFO(logger, "Finished looping over all channels. Taking VFAT" << vfatN << " on OH" << ohN << " out of run mode");
  utils::writeReg(regBase + ".GEB.VFAT" + std::to_string(vfatN) + ".CFG_RUN", 0x0);

  LOG4CPLUS_INFO(logger, "Disabling TTC Generator");
  ttcGenToggle{}(false);

  LOG4CPLUS_INFO(logger, "Reverting VFAT3 channel registers for OH" << ohN << " to original values");
  vfat3::setChannelRegistersVFAT3Simple{}(ohN, chanRegData_orig, vfatMask);

  // Set trigger VFAT mask for this OH back to 0
  LOG4CPLUS_INFO(logger, "Reverting GEM_AMC.OH.OH" << ohN << ".FPGA.TRIG.CTRL.VFAT_MASK to 0x0");
  utils::writeReg(regBase + ".FPGA.TRIG.CTRL.VFAT_MASK", 0x0);

  return outData;
}

std::vector<uint32_t> calibration::dacScan::operator()(const uint16_t& ohN,
                                                       const uint16_t& dacSelect,
                                                       const uint16_t& dacStep,
                                                       const uint32_t& vfatMask,
                                                       const bool& useExtRefADC) const
{
  if (calibration::vfat3DACAndSize.count(dacSelect) == 0) {
    std::string errmsg = "Monitoring Select value " + std::to_string(dacSelect) + " not found, possible values are:\n";
    for (auto const& iterDacSel : calibration::vfat3DACAndSize) {
      errmsg+="\t" + std::to_string(iterDacSel.first) + "\t" + std::get<0>(iterDacSel.second) + "\n";
    }
    throw std::runtime_error(errmsg);
  }

  // Check which VFATs are sync'd
  const uint32_t notmask   = (~vfatMask)&0xffffff;
  const uint32_t goodVFATs = vfat3::vfatSyncCheck{}(ohN);
  if ((notmask & goodVFATs) != notmask) {
    std::stringstream errmsg;
    errmsg << "One of the unmasked VFATs is not sync'd: "
           << "goodVFATs: 0x" << std::hex << std::setw(8) << std::setfill('0') << goodVFATs << std::dec
           << "\tnotmask: 0x" << std::hex << std::setw(8) << std::setfill('0') << notmask << std::dec;
    throw std::runtime_error(errmsg.str());
  }

  // Determine the addresses
  auto && regName = std::get<0>(calibration::vfat3DACAndSize.at(dacSelect));
  LOG4CPLUS_INFO(logger, "Scanning DAC: " << regName);
  uint32_t adcAddr[oh::VFATS_PER_OH];
  uint32_t adcCacheUpdateAddr[oh::VFATS_PER_OH];
  bool foundADCCached = false;
  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    if (!((notmask >> vfatN) & 0x1))
      continue;

    std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN);

    if (useExtRefADC) {
      // for backward compatibility, use ADC1 instead of ADC1_CACHED if it exists
      foundADCCached = !utils::regExists(regBase + ".ADC1_CACHED").empty();
      if (foundADCCached) {
        adcAddr[vfatN]            = utils::getAddress(regBase + ".ADC1_CACHED");
        adcCacheUpdateAddr[vfatN] = utils::getAddress(regBase + ".ADC1_UPDATE");
      } else {
        adcAddr[vfatN] = utils::getAddress(regBase + ".ADC1");
      }
    } else {
      // for backward compatibility, use ADC0 instead of ADC0_CACHED if it exists
      foundADCCached = !utils::regExists(regBase + ".ADC0_CACHED").empty();
      if (foundADCCached) {
        adcAddr[vfatN]            = utils::getAddress(regBase + ".ADC0_CACHED");
        adcCacheUpdateAddr[vfatN] = utils::getAddress(regBase + ".ADC0_UPDATE");
      } else {
        adcAddr[vfatN] = utils::getAddress(regBase + ".ADC0");
      }
    }
  }

  // make the output container and correctly size it
  const uint32_t dacMax = std::get<2>(calibration::vfat3DACAndSize.at(dacSelect));
  const uint32_t dacMin = std::get<1>(calibration::vfat3DACAndSize.at(dacSelect));
  const uint32_t nDacValues = (dacMax-dacMin+1)/dacStep;
  std::vector<uint32_t> dacScanData(oh::VFATS_PER_OH*nDacValues);

  // Block L1A's then take VFATs out of run mode FIXME why?
  utils::writeReg("GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x0);
  oh::broadcastWrite{}(ohN, "CFG_RUN", 0x0, vfatMask);

  // Configure the DAC Monitoring on all the VFATs
  vfat3::configureVFAT3DACMonitor{}(ohN, vfatMask, dacSelect);

  // Set the VFATs into run mode
  utils::writeReg("GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);
  oh::broadcastWrite{}(ohN, "CFG_RUN", 0x1, vfatMask);
  LOG4CPLUS_INFO(logger, "VFATs not in 0x"
                 << std::hex << std::setw(8) << std::setfill('0') << vfatMask << std::dec
                 << " were set to run mode");
  std::this_thread::sleep_for(std::chrono::seconds(1));
  // I noticed that DAC values behave weirdly immediately after VFAT is placed in run mode
  // (probably voltage/current takes a moment to stabalize)

  const uint32_t nReads = 100;
  for (uint32_t dacVal = dacMin; dacVal <= dacMax; dacVal += dacStep) {
    for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      if (!((notmask >> vfatN) & 0x1)) {
        // Store word, but with adcVal = 0
        // FIXME drawback of structure when it is redundant?
        const uint32_t idx = vfatN*(dacMax-dacMin+1)/dacStep+(dacVal-dacMin)/dacStep;
        dacScanData.at(idx) = ((ohN & 0xf) << 23) + ((vfatN & 0x1f) << 18) + (dacVal & 0xff);
        // dacScanData.push_back(((ohN & 0xf) << 23) + ((vfatN & 0x1f) << 18) + (dacVal & 0xff));
        continue;
      } else {
        std::string strDacReg = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + "." + regName;
        utils::writeReg(strDacReg, dacVal);
        uint32_t adcVal = 0;
        for (size_t i = 0; i < nReads; ++i) {
          if (foundADCCached) {
            // either reading or writing this register will trigger a cache update
            utils::readRawAddress(adcCacheUpdateAddr[vfatN]);
            // updating the cache takes 20 us, including a 50% safety factor
            std::this_thread::sleep_for(std::chrono::microseconds(25));
          }
          adcVal += utils::readRawAddress(adcAddr[vfatN]);
        }

        adcVal = adcVal/nReads;

        // FIXME drawback of structure when it is redundant?
        const uint32_t idx = vfatN*(dacMax-dacMin+1)/dacStep+(dacVal-dacMin)/dacStep;
        dacScanData.at(idx) = ((ohN & 0xf) << 23) + ((vfatN & 0x1f) << 18) + ((adcVal & 0x3ff) << 8) + (dacVal & 0xff);
        // dacScanData.push_back(((ohN & 0xf) << 23) + ((vfatN & 0x1f) << 18) + ((adcVal & 0x3ff) << 8) + (dacVal & 0xff));
      }
    }
  }

  // Take the VFATs out of Run Mode
  oh::broadcastWrite{}(ohN, "CFG_RUN", 0x0, vfatMask);

  return dacScanData;
}

std::map<uint32_t, std::vector<uint32_t>> calibration::dacScanMultiLink::operator()(const uint16_t& ohMask,
                                                                                    const uint16_t& dacSelect,
                                                                                    const uint16_t& dacStep,
                                                                                    const uint32_t& mask,
                                                                                    const bool& useExtRefADC) const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value ("<< supOH << "), request will be reset to register max");

  std::map<uint32_t, std::vector<uint32_t>> outData;
  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    std::vector<uint32_t> dacScanResults;

    // If this OptoHybrid is masked skip it
    if (!((ohMask >> ohN) & 0x1)) {
      const uint32_t dacMax = std::get<2>(calibration::vfat3DACAndSize.at(dacSelect));
      dacScanResults.assign((dacMax+1)*oh::VFATS_PER_OH/dacStep, 0xdeaddead);
      outData[ohN] = dacScanResults;
      continue;
    }

    LOG4CPLUS_INFO(logger, "Getting VFAT Mask for OH" << ohN);
    const uint32_t vfatMask = amc::getOHVFATMask{}(ohN);

    LOG4CPLUS_INFO(logger, "Performing DAC Scan for OH" << ohN);
    outData[ohN] = dacScan{}(ohN, dacSelect, dacStep, vfatMask, useExtRefADC);

    LOG4CPLUS_INFO(logger, "Finished DAC scan for OH" <<  ohN);
  }

  LOG4CPLUS_INFO(logger, "Finished DAC scans for OH Mask 0x"
                 << std::hex << std::setw(3) << std::setfill('0') << ohMask << std::dec);

  return outData;
}

extern "C" {
  const char *module_version_key = "calibration_routines v1.0.1";
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

    xhal::common::rpc::registerMethod<calibration::confCalPulse>(modmgr);
    xhal::common::rpc::registerMethod<calibration::dacMonConf>(modmgr);
    xhal::common::rpc::registerMethod<calibration::ttcGenToggle>(modmgr);
    xhal::common::rpc::registerMethod<calibration::ttcGenConf>(modmgr);
    xhal::common::rpc::registerMethod<calibration::genScan>(modmgr);
    xhal::common::rpc::registerMethod<calibration::genChannelScan>(modmgr);
    xhal::common::rpc::registerMethod<calibration::sbitRateScan>(modmgr);
    xhal::common::rpc::registerMethod<calibration::sbitRateScanParallel>(modmgr);
    xhal::common::rpc::registerMethod<calibration::checkSbitMappingWithCalPulse>(modmgr);
    xhal::common::rpc::registerMethod<calibration::checkSbitRateWithCalPulse>(modmgr);
    xhal::common::rpc::registerMethod<calibration::dacScan>(modmgr);
    xhal::common::rpc::registerMethod<calibration::dacScanMultiLink>(modmgr);
  }
}
