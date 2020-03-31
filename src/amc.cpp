/*! \file amc.cpp
 *  \brief AMC methods for RPC modules
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#include "ctp7_modules/common/amc.h"
#include "ctp7_modules/server/amc.h"
#include "ctp7_modules/common/amc/ttc.h"
#include "ctp7_modules/common/amc/daq.h"
#include "ctp7_modules/common/amc/sca.h"
#include "ctp7_modules/common/amc/blaster_ram.h"
#include "ctp7_modules/common/hw_constants.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"

#include "xhal/common/rpc/register.h"

#include <chrono>
#include <time.h>
#include <thread>

namespace amc {
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
}

uint32_t amc::fw_version_check(const char* caller_name)
{
    const uint32_t fw_maj = utils::readReg("GEM_AMC.GEM_SYSTEM.RELEASE.MAJOR");

    switch (fw_maj) {
    case 1:
        LOG4CPLUS_INFO(logger, "System release major is 1, v2B electronics behavior");
        break;
    case 3:
        LOG4CPLUS_INFO(logger, "System release major is 3, v3 electronics behavior");
        break;
    default:
        LOG4CPLUS_ERROR(logger, "Unexpected value for system release major!");
        throw std::runtime_error("Unexpected value for system release major!");
    }
    return fw_maj;
}

uint32_t amc::getOHVFATMask::operator()(const uint32_t& ohN) const
{
    uint32_t mask = 0x0;
    std::stringstream regName;
    for (unsigned int vfatN=0; vfatN<oh::VFATS_PER_OH; ++vfatN) {
        regName.clear();
        regName.str("");
        regName << "GEM_AMC.OH_LINKS.OH" << ohN << ".VFAT" << vfatN << ".SYNC_ERR_CNT";
        uint32_t syncErrCnt = utils::readReg(regName.str());

        if (syncErrCnt > 0x0) {
            mask |= (0x1 << vfatN);
        }
    }

    return mask;
}

std::vector<uint32_t> amc::getOHVFATMaskMultiLink::operator()(const uint32_t& ohMask) const
{
    const uint32_t supOH = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");

    if ((ohMask & (0xffffffff << (supOH-1))) > 0) {
        std::stringstream errmsg;
        errmsg << "Supplied OH mask has bits set ("
               << std::setw(4) << std::setfill('0') << std::hex << ohMask << std::dec
               << ") outside the number of supported OHs for this firmware ("
               << supOH << "), will only return values for the supported OHs";
        LOG4CPLUS_WARN(logger, errmsg.str());
    }

    std::vector<uint32_t> vfatMasks;
    for (uint32_t ohN = 0; ohN < supOH; ++ohN) {
        if (!((ohMask >> ohN) & 0x1)) {
            vfatMasks.push_back(0xffffff);
            continue;
        } else {
            vfatMasks.push_back(getOHVFATMask{}(ohN));
            LOG4CPLUS_DEBUG(logger, "Determined VFAT Mask for OH" << ohN << " to be 0x"
                            << std::setw(6) << std::setfill('0') << std::hex << vfatMasks.at(ohN) << std::dec);
        }
    }

    LOG4CPLUS_DEBUG(logger, "All VFAT Masks found, listing:");
    for (uint8_t ohN = 0; ohN < supOH; ++ohN) {
      LOG4CPLUS_DEBUG(logger, "VFAT mask for OH" << ohN << " to be 0x"
                      << std::hex << std::setw(8) << std::setfill('0') << vfatMasks.at(ohN) << std::dec);
    }

    return vfatMasks;
}

std::vector<uint32_t> amc::sbitReadOut::operator()(const uint32_t& ohN, const uint32_t& acquireTime) const
{
    const int nclusters = 8;
    utils::writeReg("GEM_AMC.TRIGGER.SBIT_MONITOR.OH_SELECT", ohN);
    uint32_t addrSbitMonReset = utils::getAddress("GEM_AMC.TRIGGER.SBIT_MONITOR.RESET");
    uint32_t addrSbitL1ADelay = utils::getAddress("GEM_AMC.TRIGGER.SBIT_MONITOR.L1A_DELAY");
    uint32_t addrSbitCluster[nclusters];
    for (int iCluster = 0; iCluster < nclusters; ++iCluster) {
        addrSbitCluster[iCluster] = utils::getAddress("GEM_AMC.TRIGGER.SBIT_MONITOR.CLUSTER" + std::to_string(iCluster));
    }

    // Take the VFATs out of slow control only mode
    utils::writeReg("GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

    std::vector<uint32_t> storedSbits;

    time_t acquisitionTime,startTime;
    bool acquire=true;
    startTime=time(nullptr);
    uint32_t l1ADelay;
    while (acquire) {
        // Reset monitors
        utils::writeRawAddress(addrSbitMonReset, 0x1);

        // wait for 4095 clock cycles then read L1A delay
        std::this_thread::sleep_for (std::chrono::nanoseconds(0xfff*25));
        l1ADelay = utils::readRawAddress(addrSbitL1ADelay);
        if (l1ADelay > 0xfff) {
            l1ADelay = 0xfff;
        }

        // get s-bits
        bool anyValid=false;
        std::vector<uint32_t> tempSBits;
        for (int cluster = 0; cluster < nclusters; ++cluster) {
            uint32_t thisCluster = utils::readRawAddress(addrSbitCluster[cluster]);
            uint32_t sbitAddr = (thisCluster & 0x7ff);
            int clusterSize = (thisCluster >> 12) & 0x7;
            bool isValid = (sbitAddr < ((24*64)-1));

            if (isValid) {
                LOG4CPLUS_INFO(logger, "valid sbit data: "
                               << "this cluster 0x" << std::hex << std::setw(8) << std::setfill('0') << thisCluster << std::dec
                               << ", s-bit addr 0x" << std::hex << std::setw(8) << std::setfill('0') << sbitAddr    << std::dec);
                anyValid = true;
            }

            // Store the sbit
            tempSBits.push_back( ((l1ADelay & 0x1fff) << 14) + ((clusterSize & 0x7) << 11) + (sbitAddr & 0x7ff) );
        }

        if (anyValid) {
            storedSbits.insert(storedSbits.end(),tempSBits.begin(),tempSBits.end());
        }

        acquisitionTime = difftime(time(nullptr), startTime);
        if (static_cast<uint32_t>(acquisitionTime) > acquireTime) {
            acquire=false;
        }
    }

    return storedSbits;
}

std::map<std::string, uint32_t> amc::repeatedRegRead::operator()(const std::vector<std::string>& regList, const bool& breakOnFailure, const uint32_t& nReads) const
{
    utils::SlowCtrlErrCntVFAT vfatErrs;
    for (auto const & regIter : regList) {
      LOG4CPLUS_INFO(logger, "Attempting to repeatedly read register " << regIter << " for " << nReads << " times");
      vfatErrs = vfatErrs + utils::repeatedRegRead(regIter, breakOnFailure, nReads);
    }

    std::map<std::string, uint32_t> vfatErrors;
    vfatErrors["CRC_ERROR_CNT"]         = vfatErrs.crc;
    vfatErrors["PACKET_ERROR_CNT"]      = vfatErrs.packet;
    vfatErrors["BITSTUFFING_ERROR_CNT"] = vfatErrs.bitstuffing;
    vfatErrors["TIMEOUT_ERROR_CNT"]     = vfatErrs.timeout;
    vfatErrors["AXI_STROBE_ERROR_CNT"]  = vfatErrs.axi_strobe;
    vfatErrors["SUM"]                   = vfatErrs.sum;
    vfatErrors["TRANSACTION_CNT"]       = vfatErrs.nTransactions;

    return vfatErrors;
}

extern "C" {
    const char *module_version_key = "amc v1.0.1";
    const int module_activity_color = 4;

    void module_init(ModuleManager *modmgr)
    {
        utils::initLogging();

        if (memhub_open(&memsvc) != 0) {
            auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
            LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Unable to connect to memory service: ") << memsvc_get_last_error(memsvc));
            LOG4CPLUS_ERROR(logger, "Unable to load module");
            return;
        }

        xhal::common::rpc::registerMethod<amc::getOHVFATMask>(modmgr);
        xhal::common::rpc::registerMethod<amc::getOHVFATMaskMultiLink>(modmgr);
        xhal::common::rpc::registerMethod<amc::sbitReadOut>(modmgr);
        xhal::common::rpc::registerMethod<amc::repeatedRegRead>(modmgr);

        // DAQ module methods (from amc/daq)
        xhal::common::rpc::registerMethod<amc::daq::enableDAQLink>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::disableDAQLink>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::setZS>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::resetDAQLink>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::setDAQLinkInputTimeout>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::setDAQLinkRunType>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::setDAQLinkRunParameter>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::setDAQLinkRunParameters>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::configureDAQModule>(modmgr);
        xhal::common::rpc::registerMethod<amc::daq::enableDAQModule>(modmgr);

        // TTC module methods (from amc/ttc)
        xhal::common::rpc::registerMethod<amc::ttc::ttcModuleReset>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::ttcMMCMReset>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::ttcMMCMPhaseShift>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::checkPLLLock>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getMMCMPhaseMean>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getMMCMPhaseMedian>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getGTHPhaseMean>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getGTHPhaseMedian>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::ttcCounterReset>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getL1AEnable>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::setL1AEnable>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getTTCConfig>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::setTTCConfig>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getTTCStatus>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getTTCErrorCount>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getTTCCounter>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getL1AID>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getL1ARate>(modmgr);
        xhal::common::rpc::registerMethod<amc::ttc::getTTCSpyBuffer>(modmgr);

        // SCA module methods (from amc/sca)
        xhal::common::rpc::registerMethod<amc::sca::scaHardResetEnable>(modmgr);
        xhal::common::rpc::registerMethod<amc::sca::readSCAADCSensor>(modmgr);
        xhal::common::rpc::registerMethod<amc::sca::readSCAADCTemperatureSensors>(modmgr);
        xhal::common::rpc::registerMethod<amc::sca::readSCAADCVoltageSensors>(modmgr);
        xhal::common::rpc::registerMethod<amc::sca::readSCAADCSignalStrengthSensors>(modmgr);
        xhal::common::rpc::registerMethod<amc::sca::readAllSCAADCSensors>(modmgr);

        // BLASTER RAM module methods (from amc/blaster_ram)
        xhal::common::rpc::registerMethod<amc::blaster::writeConfRAM>(modmgr);
        xhal::common::rpc::registerMethod<amc::blaster::readConfRAM>(modmgr);
    }
}
