/*! \file daq_monitor.cpp
 *  \brief Contains functions for hardware monitoring
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#include "ctp7_modules/common/daq_monitor.h"
#include "ctp7_modules/server/amc.h"
#include "ctp7_modules/common/hw_constants.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"

#include "xhal/common/rpc/register.h"

#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

namespace daqmon {
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
}

std::map<std::string, uint32_t> daqmon::getmonTTCmain::operator()() const
{
  LOG4CPLUS_INFO(logger, "Called getmonTTCmain");

  std::map<std::string, uint32_t> ttcMon;

  ttcMon["MMCM_LOCKED"]          = utils::readReg("GEM_AMC.TTC.STATUS.CLK.MMCM_LOCKED");
  ttcMon["TTC_SINGLE_ERROR_CNT"] = utils::readReg("GEM_AMC.TTC.STATUS.TTC_SINGLE_ERROR_CNT");
  ttcMon["BC0_LOCKED"]           = utils::readReg("GEM_AMC.TTC.STATUS.BC0.LOCKED");
  ttcMon["L1A_ID"]               = utils::readReg("GEM_AMC.TTC.L1A_ID");
  ttcMon["L1A_RATE"]             = utils::readReg("GEM_AMC.TTC.L1A_RATE");

  return ttcMon;
}

std::map<std::string, uint32_t> daqmon::getmonTRIGGERmain::operator()(const uint16_t& ohMask) const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value (" << supOH << "), request will be reset to register max");

  std::map<std::string, uint32_t> trigMon;

  trigMon["OR_TRIGGER_RATE"] = utils::readReg("GEM_AMC.TRIGGER.STATUS.OR_TRIGGER_RATE");

  std::string t1, t2;
  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    if (!((ohMask >> ohN) & 0x1)) {
      continue;
    }
    t1 = "OH" + std::to_string(ohN) + ".TRIGGER_RATE";
    t2 = "GEM_AMC.TRIGGER.OH" + std::to_string(ohN) + ".TRIGGER_RATE";
    trigMon[t1] = utils::readReg(t2);
  }
  return trigMon;
}

std::map<std::string, uint32_t> daqmon::getmonTRIGGEROHmain::operator()(const uint16_t& ohMask) const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value (" << supOH << "), request will be reset to register max");

  std::map<std::string, uint32_t> trigMon;
  std::string t1, t2;
  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    t1 = "OH" + std::to_string(ohN);
    if (!((ohMask >> ohN) & 0x1)) {
      trigMon[t1 + ".LINK0_MISSED_COMMA_CNT"]  = 0xdeaddead;
      trigMon[t1 + ".LINK1_MISSED_COMMA_CNT"]  = 0xdeaddead;
      trigMon[t1 + ".LINK0_OVERFLOW_CNT"]      = 0xdeaddead;
      trigMon[t1 + ".LINK1_OVERFLOW_CNT"]      = 0xdeaddead;
      trigMon[t1 + ".LINK0_UNDERFLOW_CNT"]     = 0xdeaddead;
      trigMon[t1 + ".LINK1_UNDERFLOW_CNT"]     = 0xdeaddead;
      trigMon[t1 + ".LINK0_SBIT_OVERFLOW_CNT"] = 0xdeaddead;
      trigMon[t1 + ".LINK1_SBIT_OVERFLOW_CNT"] = 0xdeaddead;
      continue;
    }

    t2 = "GEM_AMC.TRIGGER.OH" + std::to_string(ohN);
    trigMon[t1 + ".LINK0_MISSED_COMMA_CNT"]  = utils::readReg(t2 + ".LINK0_MISSED_COMMA_CNT");
    trigMon[t1 + ".LINK1_MISSED_COMMA_CNT"]  = utils::readReg(t2 + ".LINK1_MISSED_COMMA_CNT");
    trigMon[t1 + ".LINK0_OVERFLOW_CNT"]      = utils::readReg(t2 + ".LINK0_OVERFLOW_CNT");
    trigMon[t1 + ".LINK1_OVERFLOW_CNT"]      = utils::readReg(t2 + ".LINK1_OVERFLOW_CNT");
    trigMon[t1 + ".LINK0_UNDERFLOW_CNT"]     = utils::readReg(t2 + ".LINK0_UNDERFLOW_CNT");
    trigMon[t1 + ".LINK1_UNDERFLOW_CNT"]     = utils::readReg(t2 + ".LINK1_UNDERFLOW_CNT");
    trigMon[t1 + ".LINK0_SBIT_OVERFLOW_CNT"] = utils::readReg(t2 + ".LINK0_SBIT_OVERFLOW_CNT");
    trigMon[t1 + ".LINK1_SBIT_OVERFLOW_CNT"] = utils::readReg(t2 + ".LINK1_SBIT_OVERFLOW_CNT");
  }

  return trigMon;
}

std::map<std::string, uint32_t> daqmon::getmonDAQmain::operator()() const
{
  std::map<std::string, uint32_t> daqMon;

  daqMon["DAQ_ENABLE"]          = utils::readReg("GEM_AMC.DAQ.CONTROL.DAQ_ENABLE");
  daqMon["DAQ_LINK_READY"]      = utils::readReg("GEM_AMC.DAQ.STATUS.DAQ_LINK_RDY");
  daqMon["DAQ_LINK_AFULL"]      = utils::readReg("GEM_AMC.DAQ.STATUS.DAQ_LINK_AFULL");
  daqMon["DAQ_OFIFO_HAD_OFLOW"] = utils::readReg("GEM_AMC.DAQ.STATUS.DAQ_OUTPUT_FIFO_HAD_OVERFLOW");
  daqMon["L1A_FIFO_HAD_OFLOW"]  = utils::readReg("GEM_AMC.DAQ.STATUS.L1A_FIFO_HAD_OVERFLOW");
  daqMon["L1A_FIFO_DATA_COUNT"] = utils::readReg("GEM_AMC.DAQ.EXT_STATUS.L1A_FIFO_DATA_CNT");
  daqMon["DAQ_FIFO_DATA_COUNT"] = utils::readReg("GEM_AMC.DAQ.EXT_STATUS.DAQ_FIFO_DATA_CNT");
  daqMon["EVENT_SENT"]          = utils::readReg("GEM_AMC.DAQ.EXT_STATUS.EVT_SENT");
  daqMon["TTS_STATE"]           = utils::readReg("GEM_AMC.DAQ.STATUS.TTS_STATE");
  daqMon["INPUT_ENABLE_MASK"]   = utils::readReg("GEM_AMC.DAQ.CONTROL.INPUT_ENABLE_MASK");
  daqMon["INPUT_AUTOKILL_MASK"] = utils::readReg("GEM_AMC.DAQ.STATUS.INPUT_AUTOKILL_MASK");

  return daqMon;
}

std::map<std::string, uint32_t> daqmon::getmonDAQOHmain::operator()(const uint16_t& ohMask) const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value (" << supOH << "), request will be reset to register max");

  std::map<std::string, uint32_t> daqMon;
  std::string t1, t2;
  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    t1 = "OH" + std::to_string(ohN);
    if (!((ohMask >> ohN) & 0x1)) {
      daqMon[t1 + ".STATUS.EVT_SIZE_ERR"]         = 0xdeaddead;
      daqMon[t1 + ".STATUS.EVENT_FIFO_HAD_OFLOW"] = 0xdeaddead;
      daqMon[t1 + ".STATUS.INPUT_FIFO_HAD_OFLOW"] = 0xdeaddead;
      daqMon[t1 + ".STATUS.INPUT_FIFO_HAD_UFLOW"] = 0xdeaddead;
      daqMon[t1 + ".STATUS.VFAT_TOO_MANY"]        = 0xdeaddead;
      daqMon[t1 + ".STATUS.VFAT_NO_MARKER"]       = 0xdeaddead;
      continue;
    }

    t2 = "GEM_AMC.DAQ." + t1;
    daqMon[t1 + ".STATUS.EVT_SIZE_ERR"]         = utils::readReg(t2 + ".STATUS.EVT_SIZE_ERR");
    daqMon[t1 + ".STATUS.EVENT_FIFO_HAD_OFLOW"] = utils::readReg(t2 + ".STATUS.EVENT_FIFO_HAD_OFLOW");
    daqMon[t1 + ".STATUS.INPUT_FIFO_HAD_OFLOW"] = utils::readReg(t2 + ".STATUS.INPUT_FIFO_HAD_OFLOW");
    daqMon[t1 + ".STATUS.INPUT_FIFO_HAD_UFLOW"] = utils::readReg(t2 + ".STATUS.INPUT_FIFO_HAD_UFLOW");
    daqMon[t1 + ".STATUS.VFAT_TOO_MANY"]        = utils::readReg(t2 + ".STATUS.VFAT_TOO_MANY");
    daqMon[t1 + ".STATUS.VFAT_NO_MARKER"]       = utils::readReg(t2 + ".STATUS.VFAT_NO_MARKER");
  }

  return daqMon;
}

std::map<std::string, uint32_t> daqmon::getmonGBTLink::operator()(const bool& doReset) const
{
  if (doReset) {
    utils::writeReg("GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 0x1);
  }

  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");

  std::map<std::string, uint32_t> gbtMon;
  std::string regName, respName;
  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    for (size_t gbtN = 0; gbtN < gbt::GBTS_PER_OH; ++gbtN) {
      respName = "OH" + std::to_string(ohN) + ".GBT" + std::to_string(gbtN);
      regName  = "GEM_AMC.OH_LINKS" + respName;

      //Ready
      gbtMon[respName + ".READY"] = utils::readReg(regName + "_READY");

      //Was not ready
      gbtMon[respName + ".WAS_NOT_READY"] = utils::readReg(regName + "_WAS_NOT_READY");

      //Rx had overflow
      gbtMon[respName + ".RX_HAD_OVERFLOW"] = utils::readReg(regName + "_RX_HAD_OVERFLOW");

      //Rx had underflow
      gbtMon[respName + ".RX_HAD_UNDERFLOW"] = utils::readReg(regName + "_RX_HAD_UNDERFLOW");
    }
  }

  return gbtMon;
}

std::map<std::string, uint32_t> daqmon::getmonOHmain::operator()(const uint16_t& ohMask) const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value (" << supOH << "), request will be reset to register max");

  std::map<std::string, uint32_t> ohMon;
  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    const std::string keyName = "OH" + std::to_string(ohN);
    if (!((ohMask >> ohN) & 0x1)) {
      ohMon[keyName + ".FW_VERSION"]        = 0xdeaddead;
      ohMon[keyName + ".EVENT_COUNTER"]     = 0xdeaddead;
      ohMon[keyName + ".EVENT_RATE"]        = 0xdeaddead;
      ohMon[keyName + ".GTX.TRK_ERR"]       = 0xdeaddead;
      ohMon[keyName + ".GTX.TRG_ERR"]       = 0xdeaddead;
      ohMon[keyName + ".GBT.TRK_ERR"]       = 0xdeaddead;
      ohMon[keyName + ".CORR_VFAT_BLK_CNT"] = 0xdeaddead;
      ohMon[keyName + ".COUNTERS.SEU"]      = 0xdeaddead;
      ohMon[keyName + ".STATUS.SEU"]        = 0xdeaddead;
      continue;
    }

    if (amc::fw_version_check("getmonOHmain") == 3) {
      uint32_t fwreg = utils::readRawReg("GEM_AMC.OH." + keyName + ".FPGA.CONTROL.RELEASE.VERSION.MAJOR");
      uint32_t fwver = 0x0;
      fwver |= ((fwreg&0x000000ff) << 24);
      fwver |= ((fwreg&0x0000ff00) << 16);
      fwver |= ((fwreg&0x00ff0000) << 8);
      fwver |= (fwreg&0xff000000);
      LOG4CPLUS_INFO(logger, "FW version register for OH" << ohN << " is 0x"
                     << std::hex << std::setw(8) << std::setfill('0') << fwver << std::dec
                     << ", fwver is 0x"
                     << std::hex << std::setw(8) << std::setfill('0') << fwver << std::dec);
      ohMon[keyName + ".FW_VERSION"] = fwver;
    } else {
      ohMon[keyName + ".FW_VERSION"] = utils::readReg("GEM_AMC.OH." + keyName + ".STATUS.FW.VERSION");
    }

    ohMon[keyName + ".EVENT_COUNTER"]     = utils::readReg("GEM_AMC.DAQ." + keyName + ".COUNTERS.EVN");
    ohMon[keyName + ".EVENT_RATE"]        = utils::readReg("GEM_AMC.DAQ." + keyName + ".COUNTERS.EVT_RATE");
    ohMon[keyName + ".GTX.TRK_ERR"]       = utils::readReg("GEM_AMC.OH." + keyName + ".COUNTERS.GTX_LINK.TRK_ERR");
    ohMon[keyName + ".GTX.TRG_ERR"]       = utils::readReg("GEM_AMC.OH." + keyName + ".COUNTERS.GTX_LINK.TRG_ERR");
    ohMon[keyName + ".GBT.TRK_ERR"]       = utils::readReg("GEM_AMC.OH." + keyName + ".COUNTERS.GBT_LINK.TRK_ERR");
    ohMon[keyName + ".CORR_VFAT_BLK_CNT"] = utils::readReg("GEM_AMC.DAQ." + keyName + ".COUNTERS.CORRUPT_VFAT_BLK_CNT");
    ohMon[keyName + ".COUNTERS.SEU"]      = utils::readReg("GEM_AMC.OH." + keyName + ".COUNTERS.SEU");
    ohMon[keyName + ".STATUS.SEU"]        = utils::readReg("GEM_AMC.OH." + keyName + ".STATUS.SEU");
  }

  return ohMon;
}

std::map<std::string, uint32_t> daqmon::getmonOHSCAmain::operator()(const uint16_t& ohMask) const
{
  uint32_t initSCAMonOffMask = 0xffffffff;
  if (!utils::regExists("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.MONITORING_OFF").empty()) {
    initSCAMonOffMask = utils::readReg("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.MONITORING_OFF");
    utils::writeReg("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.MONITORING_OFF", (~ohMask) & 0x3ff);
  }

  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value (" << supOH << "), request will be reset to register max");

  // OH voltage Sensors
  const std::array<std::string, 10> sensors = {
      "AVCCN",
      "AVTTN",
      "1V0_INT",
      "1V8F",
      "1V5",
      "2V5_IO",
      "3V0",
      "1V8",
      "VTRX_RSSI2",
      "VTRX_RSSI1",
  };

  std::map<std::string, uint32_t> scaMon;

  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    const std::string keyName = "OH" + std::to_string(ohN);
    if (!((ohMask >> ohN) & 0x1)) {
      scaMon[keyName + ".SCA_TEMP"] = 0xdeaddead;

      for (uint32_t tempVal = 1; tempVal <= 9; ++tempVal)
        scaMon[keyName+ ".BOARD_TEMP" + std::to_string(tempVal)] = 0xdeaddead;

      for (auto const& sensor : sensors)
        scaMon[keyName + "." + sensor] = 0xdeaddead;
      continue;
    }

    LOG4CPLUS_INFO(logger, "Reading SCA Monitoring Values for " << keyName);

    const std::string regName = "GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING" + keyName;

    scaMon[keyName + ".SCA_TEMP"] = utils::readReg(regName+ ".SCA_TEMP");

    // OH Temperature Sensors
    for (size_t tempVal = 1; tempVal <= 9; ++tempVal) {
      scaMon[keyName + ".BOARD_TEMP" + std::to_string(tempVal)] = utils::readReg(regName+ ".BOARD_TEMP" + std::to_string(tempVal));
    }

    for (auto const& sensor : sensors)
      scaMon[keyName + "." + sensor] = utils::readReg(regName+ "." + sensor);
  }

  if (!utils::regExists("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.MONITORING_OFF").empty()) {
    utils::writeReg("GEM_AMC.SLOW_CONTROL.SCA.ADC_MONITORING.MONITORING_OFF", initSCAMonOffMask);
  }

  return scaMon;
}

std::map<std::string, uint32_t> daqmon::getmonOHSysmon::operator()(const uint16_t& ohMask, const bool& doReset) const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
  if (ohMask > (0x1<<supOH))
    LOG4CPLUS_WARN(logger, "Requested OptoHybrids (0x" << std::hex << std::setw(4) << std::setfill('0') << ohMask << std::dec
                   << ") > NUM_OF_OH AMC register value (" << supOH << "), request will be reset to register max");

  const std::array<std::string, 3> sensors = {
    "FPGA_CORE_TEMP",
    "FPGA_CORE_1V0",
    "FPGA_CORE_2V5_IO",
  };

  std::map<std::string, uint32_t> sysMon;

  if (amc::fw_version_check("getmonOHSysmon") == 3) {
    const std::array<std::string, 6> alarms = {
      "OVERTEMP",
      "CNT_OVERTEMP",
      "VCCAUX_ALARM",
      "CNT_VCCAUX_ALARM",
      "VCCINT_ALARM",
      "CNT_VCCINT_ALARM"
    };

    for (size_t ohN = 0; ohN < supOH; ++ohN) {
      const std::string keyName = "OH" + std::to_string(ohN);
      if (!((ohMask >> ohN) & 0x1)) {
        for (auto const& alarm : alarms)
          sysMon[keyName + "." + alarm] = 0xdeaddead;
        for (auto const& sensor : sensors)
          sysMon[keyName + "." + sensor] = 0xdeaddead;
        continue;
      }

      const std::string regBase = "GEM_AMC.OH.OH" + keyName + ".FPGA.ADC.CTRL.";

      LOG4CPLUS_INFO(logger, "Reading Sysmon values for " << keyName);

      if (doReset) {
        LOG4CPLUS_INFO(logger, "Reseting CNT_OVERTEMP, CNT_VCCAUX_ALARM and CNT_VCCINT_ALARM for " << keyName);
        utils::writeReg(regBase + "RESET", 0x1);
      }

      for (auto const& alarm : alarms)
        sysMon[keyName + "." + alarm] = utils::readReg(regBase + alarm);

      utils::writeReg(regBase + "ENABLE", 0x1);

      uint8_t adcAddr = 0x0;
      for (auto const& sensor : sensors) {
        utils::writeReg(regBase + "ADR_IN", adcAddr++);
        sysMon[keyName + "." + sensor] = ((utils::readReg(regBase + sensor) >> 6) & 0x3ff);
      }

      utils::writeReg(regBase + "ENABLE", 0x0);
    }
  } else {
    for (size_t ohN = 0; ohN < supOH; ++ohN) {
      const std::string keyName = "OH" + std::to_string(ohN);
      if (!((ohMask >> ohN) & 0x1)) {
        for (auto const& sensor : sensors)
          sysMon[keyName + "." + sensor] = 0xdeaddead;
        continue;
      }

      const std::string regBase = "GEM_AMC.OH.OH" + keyName + ".ADC.";

      LOG4CPLUS_INFO(logger, "Reading Sysmon values for " << keyName);

      for (auto const& sensor : sensors)
        sysMon[keyName + "." + sensor] = ((utils::readReg(regBase + sensor) >> 6) & 0x3ff);
    }
  }

  return sysMon;
}

std::map<std::string, uint32_t> daqmon::getmonSCA::operator()() const
{
  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");

  std::map<std::string, uint32_t> scaMon;

  scaMon["SCA.STATUS.READY"]          = utils::readReg("GEM_AMC.SLOW_CONTROL.SCA.STATUS.READY");
  scaMon["SCA.STATUS.CRITICAL_ERROR"] = utils::readReg("GEM_AMC.SLOW_CONTROL.SCA.STATUS.CRITICAL_ERROR");
  for (size_t i = 0; i < supOH; ++i) {
    const std::string keyName = "SCA.STATUS.NOT_READY_CNT_OH";
    const std::string regName = "GEM_AMC.SLOW_CONTROL." + keyName;
    scaMon[keyName] = utils::readReg(regName);
  }

  return scaMon;
}

std::map<std::string, uint32_t> daqmon::getmonVFATLink::operator()(const bool& doReset) const
{
  if (doReset) {
    utils::writeReg("GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(92)); // FIXME sleep for N orbits
  }

  const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");

  std::map<std::string, uint32_t> vfatMon;

  bool vfatOutOfSync = false;
  for (size_t ohN = 0; ohN < supOH; ++ohN) {
    for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      const std::string keyBase = "OH" + std::to_string(ohN) + ".VFAT" + std::to_string(vfatN);
      const std::string regBase = "GEM_AMC.OH_LINKS." + keyBase;
      const uint32_t nSyncErrs = utils::readReg(regBase + ".SYNC_ERR_CNT");
      vfatMon[keyBase + ".SYNC_ERR_CNT"] = nSyncErrs;

      if (nSyncErrs > 0) {
        vfatOutOfSync = true;
      }

      vfatMon[keyBase + ".DAQ_EVENT_CNT"] = utils::readReg(regBase + ".DAQ_EVENT_CNT");
      vfatMon[keyBase + ".DAQ_CRC_ERROR_CNT"] = utils::readReg(regBase + ".DAQ_CRC_ERROR_CNT");
    }
  }

  // Set OOS flag (out of sync)
  if (vfatOutOfSync) {
    LOG4CPLUS_WARN(logger,"One or more VFATs found to be out of sync");
  }

  return vfatMon;
}

std::map<std::string, uint32_t> daqmon::getmonCTP7dump::operator()(const std::string& fname) const
{
  LOG4CPLUS_INFO(logger, "Using registers found in: " << fname);
  std::ifstream ifs(fname);
  if (!ifs) {
    std::stringstream errmsg;
    errmsg << "Error opening file: " << fname;
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }

  std::map<std::string,uint32_t> ctp7Dump;

  std::vector<std::pair<std::string, std::string> > regNames;
  std::string line;
  while (std::getline(ifs, line)) {
    // format0x658030f8 r    GEM_AMC.OH_LINKS.OH11.VFAT23.DAQ_CRC_ERROR_CNT          0x00000000
    std::replace(line.begin(), line.end(), '\t', ' ');
    std::stringstream ss(line);
    std::string token;
    size_t count = 0;
    std::vector<std::string> tmp;
    while (std::getline(ss, token, ' ')) {
      if (token.empty())
        continue;

      if (count == 0 || count == 2) {
        tmp.push_back(token);
        LOG4CPLUS_DEBUG(logger, "Pushing back " << token << " as value " << count);
      }
      ++count;
    }
    ctp7Dump[tmp.at(0)] = utils::readReg(tmp.at(1));
  }

  return ctp7Dump;
}

extern "C" {
  const char *module_version_key = "daq_monitor v1.0.1";
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

    xhal::common::rpc::registerMethod<daqmon::getmonTTCmain>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonTRIGGERmain>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonTRIGGEROHmain>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonDAQmain>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonDAQOHmain>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonGBTLink>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonOHmain>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonOHSCAmain>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonOHSysmon>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonSCA>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonVFATLink>(modmgr);
    xhal::common::rpc::registerMethod<daqmon::getmonCTP7dump>(modmgr);
  }
}
