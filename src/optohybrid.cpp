#include "ctp7_modules/common/optohybrid.h"
#include "ctp7_modules/server/amc.h"
#include "ctp7_modules/common/hw_constants.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"
#include "ctp7_modules/common/vfat_parameters.h"

#include "xhal/common/rpc/register.h"

#include <chrono>
#include <fstream>
#include <thread>

namespace oh {
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
}

void oh::broadcastWrite::operator()(const uint32_t& ohN, const std::string& regName, const uint32_t& value, const uint32_t& mask) const
{
  std::string t_regName;
  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    if (!((mask >> vfatN)&0x1)) {
      std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".";
      t_regName = regBase + regName;
      utils::writeReg(t_regName, value);
    }
  }
}

std::vector<uint32_t> oh::broadcastRead::operator()(const uint32_t& ohN, const std::string& regName, const uint32_t& mask) const
{
  std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT";

  std::string t_regName;
  std::vector<uint32_t> data;
  for (size_t i = 0; i < oh::VFATS_PER_OH; ++i) {
    if ((mask >> i)&0x1) {
      data.push_back(0);
    } else {
      t_regName = regBase + std::to_string(i) + "." + regName;

      try {
        data.push_back(utils::readReg(t_regName));
      } catch (...) { // FIXME add exception handling
        LOG4CPLUS_WARN(logger, "Error reading register " + t_regName);
        data.push_back(0xdeaddead); // FIXME do this right
      }
    }
  }
  return data;
}

void oh::biasAllVFATs::operator()(const uint32_t& ohN, const uint32_t& mask) const
{
  for (auto const& it : vfat_parameters) {
    broadcastWrite{}(ohN, it.first, it.second, mask);
  }
}

void oh::setAllVFATsToRunMode::operator()(const uint32_t& ohN, const uint32_t& mask) const
{
  broadcastWrite{}(ohN, "CFG_RUN", 0x1, mask);
}

void oh::setAllVFATsToSleepMode::operator()(const uint32_t& ohN, const uint32_t& mask) const
{
  broadcastWrite{}(ohN, "CFG_RUN", 0x0, mask);
}

void oh::stopCalPulse2AllChannels::operator()(const uint32_t& ohN, const uint32_t& mask, const uint32_t& chMin, const uint32_t& chMax) const
{
  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    if ((mask >> vfatN) & 0x1)
      continue;
    std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".VFAT_CHANNELS.CHANNEL";
    for (uint32_t chan = chMin; chan <= chMax; ++chan) {
      utils::writeReg(regBase + std::to_string(chan) + ".CALPULSE_ENABLE", 0x0);
    }
  }
}

std::map<std::string, std::vector<uint32_t>> oh::statusOH::operator()(const uint32_t& ohMask) const
{
  LOG4CPLUS_INFO(logger, "Reeading OH status registers");

  std::array<std::string, 23 + 1> regs = {
    "CFG_PULSE_STRETCH", // FIXME seems like a copy/paste error?
    "TRIG.CTRL.SBIT_SOT_READY",
    "TRIG.CTRL.SBIT_SOT_UNSTABLE",
    "GBT.TX.TX_READY",
    "GBT.RX.RX_READY",
    "GBT.RX.RX_VALID",
    "GBT.RX.CNT_LINK_ERR",
    "ADC.CTRL.CNT_OVERTEMP",
    "ADC.CTRL.CNT_VCCAUX_ALARM",
    "ADC.CTRL.CNT_VCCINT_ALARM",
    "CONTROL.RELEASE.DATE",
    "CONTROL.RELEASE.VERSION.MAJOR",
    "CONTROL.RELEASE.VERSION.MINOR",
    "CONTROL.RELEASE.VERSION.BUILD",
    "CONTROL.RELEASE.VERSION.GENERATION",
    "CONTROL.SEM.CNT_SEM_CRITICAL",
    "CONTROL.SEM.CNT_SEM_CORRECTION",
    "TRIG.CTRL.SOT_INVERT",
    "GBT.TX.CNT_RESPONSE_SENT",
    "GBT.RX.CNT_REQUEST_RECEIVED",
    "CLOCKING.CLOCKING.GBT_MMCM_LOCKED",
    "CLOCKING.CLOCKING.LOGIC_MMCM_LOCKED",
    "CLOCKING.CLOCKING.GBT_MMCM_UNLOCKED_CNT",
    "CLOCKING.CLOCKING.LOGIC_MMCM_UNLOCKED_CNT"
  };

  std::map<std::string, std::vector<uint32_t>> statusValues;

  for (size_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
    const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".";
    for (auto &reg : regs) {
      if ((ohMask >> ohN) & 0x1) {
        statusValues[reg].push_back(utils::readReg(regBase + reg));
      } else {
        statusValues[reg].push_back(0xdeaddead);  // FIXME 0xdeaddead or 0x0?
      }
    }
  }

  return statusValues;
}

extern "C" {
  const char *module_version_key = "optohybrid v1.0.1";
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

    xhal::common::rpc::registerMethod<oh::broadcastWrite>(modmgr);
    xhal::common::rpc::registerMethod<oh::broadcastRead>(modmgr);
    xhal::common::rpc::registerMethod<oh::biasAllVFATs>(modmgr);
    xhal::common::rpc::registerMethod<oh::setAllVFATsToRunMode>(modmgr);
    xhal::common::rpc::registerMethod<oh::setAllVFATsToSleepMode>(modmgr);
    xhal::common::rpc::registerMethod<oh::statusOH>(modmgr);
    xhal::common::rpc::registerMethod<oh::stopCalPulse2AllChannels>(modmgr);
  }
}
