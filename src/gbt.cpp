/*! \file
 *  \brief RPC module for GBT methods
 *  \author Laurent Pétré <lpetre@ulb.ac.be>
 */

#include "ctp7_modules/common/gbt.h"
#include "ctp7_modules/server/gbt.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"
#include "ctp7_modules/common/hw_constants_checks.h"

#include "xhal/common/rpc/register.h"

#include <chrono>
#include <string>
#include <sstream>
#include <thread>

namespace gbt {
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
}

std::map<uint32_t, std::vector<uint32_t>> gbt::scanGBTPhases::operator()(const uint32_t& ohN,
                                                                         const uint32_t& nResets,
                                                                         const uint8_t& phaseMin,
                                                                         const uint8_t& phaseMax,
                                                                         const uint8_t& phaseStep,
                                                                         const uint32_t& nVerificationReads) const
{
    LOG4CPLUS_INFO(logger, "Scanning the phases for OH" << ohN);

    const uint32_t ohMax = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (ohN >= ohMax) {
        std::stringstream errmsg;
        errmsg << "The ohN parameter supplied (" << ohN << ") exceeds the number of OH's supported by the CTP7 (" << ohMax << ").";
        throw std::range_error(errmsg.str());
    }

    checkPhase(phaseMin);
    checkPhase(phaseMax);

    //(oh::VFATS_PER_OH, std::vector<uint32_t>(16));
    std::map<uint32_t, std::vector<uint32_t>> results;
    for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      results[vfatN] = std::vector<uint32_t>(16);
    }

    // Perform the scan
    for (size_t phase = phaseMin; phase <= phaseMax; phase += phaseStep) {
        // Set the new phases
        for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
            writeGBTPhase{}(ohN, vfatN, phase);
        }

        // Wait for the phases to be set
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        const std::string regBase = "GEM_AMC.OH.OH" + std::to_string(ohN);
        for (size_t repN = 0; repN < nResets; ++repN) {
            utils::writeReg("GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // Check the VFAT status
            utils::SlowCtrlErrCntVFAT vfatErrs;
            for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
                const std::string vfRegBase = regBase + ".GEB.VFAT" + std::to_string(vfatN);
                vfatErrs = utils::repeatedRegRead("GEM_AMC.OH_LINKS.OH" + std::to_string(ohN)
                                                  + ".VFAT" + std::to_string(vfatN)
                                                  + ".SYNC_ERR_CNT", true, nVerificationReads);

                if (vfatErrs.sum != 0) {
                    continue;
                }
                vfatErrs = utils::repeatedRegRead(vfRegBase + ".CFG_RUN", true, nVerificationReads);
                if (vfatErrs.sum != 0) {
                    continue;
                }
                vfatErrs = utils::repeatedRegRead(vfRegBase + ".HW_ID_VER", true, nVerificationReads);
                if (vfatErrs.sum != 0) {
                    continue;
                }
                vfatErrs = utils::repeatedRegRead(vfRegBase + ".HW_ID", true, nVerificationReads);
                if (vfatErrs.sum != 0) {
                    continue;
                }
                // If no errors, the phase is good
                ++results[vfatN][phase];
            }
        }
    }

    return results;

}

void gbt::writeGBTConfig::operator()(const uint32_t& ohN, const uint32_t& gbtN, const config_t& config) const
{
    LOG4CPLUS_INFO(logger, "Writing the configuration of OH #" << ohN << " - GBTX #" << gbtN << ".");

    // ohN check
    const uint32_t ohMax = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    std::stringstream errmsg;
    if (ohN >= ohMax) {
        errmsg << "The ohN parameter supplied (" << ohN << ") exceeds the number of OH's supported by the CTP7 (" << ohMax << ").";
        throw std::range_error(errmsg.str());
    }
    // gbtN check
    if (gbtN >= GBTS_PER_OH) {
        errmsg << "The gbtN parameter supplied (" << ohN << ") exceeds the number of GBT's per OH (" << GBTS_PER_OH << ").";
        throw std::range_error(errmsg.str());
    }
    // Write all the registers
    for (size_t address = 0; address < CONFIG_SIZE; ++address) {
        writeGBTReg(ohN, gbtN, static_cast<uint16_t>(address), config[address]);
    }
}

void gbt::writeGBTPhase::operator()(const uint32_t& ohN, const uint32_t& vfatN, const uint8_t& phase) const
{
    LOG4CPLUS_INFO(logger, "Writing phase " << phase << " to VFAT #" << vfatN << " of OH #" << ohN << ".");

    // ohN check
    const uint32_t ohMax = utils::readReg("GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    std::stringstream errmsg;
    if (ohN >= ohMax) {
        errmsg << "The ohN parameter supplied (" << ohN << ") exceeds the number of OH's supported by the CTP7 (" << ohMax << ").";
        throw std::range_error(errmsg.str());
    }
    // vfatN check
    if (vfatN >= oh::VFATS_PER_OH) {
        errmsg << "The vfatN parameter supplied (" << vfatN << ") exceeds the number of VFAT's per OH (" << oh::VFATS_PER_OH << ").";
        throw std::range_error(errmsg.str());
    }
    // phase check
    checkPhase(phase);

    // Write the triplicated phase registers
    const uint32_t gbtN = gbt::elinkMappings::VFAT_TO_GBT[vfatN];
    LOG4CPLUS_INFO(logger, "Writing " << phase << " to the VFAT" << vfatN
                   << " phase of GBT" << gbtN
                   << ", on OH" << ohN);

    for (size_t regN = 0; regN < 3; ++regN) {
        const uint16_t regAddress = elinkMappings::ELINK_TO_REGISTERS[elinkMappings::VFAT_TO_ELINK[vfatN]][regN];

        writeGBTReg(ohN, gbtN, regAddress, phase);
    }

}

void gbt::writeGBTReg(const uint32_t ohN, const uint32_t gbtN, const uint16_t address, const uint8_t value)
{
    // Check that the GBT exists
    std::stringstream errmsg;
    if (gbtN >= GBTS_PER_OH) {
        errmsg << "The gbtN parameter supplied (" << gbtN << ") is larger than the number of GBT's per OH (" << GBTS_PER_OH << ").";
        throw std::range_error(errmsg.str());
    }
    // Check that the address is writable
    if (address >= CONFIG_SIZE) {
        errmsg << "GBT has " << CONFIG_SIZE-1 << "writable addresses while the provided address is" << address << ".";
        throw std::range_error(errmsg.str());
    }
    // GBT registers are 8 bits long
    utils::writeReg("GEM_AMC.SLOW_CONTROL.IC.READ_WRITE_LENGTH", 1);

    // Select the link number
    const uint32_t linkN = ohN*GBTS_PER_OH + gbtN;
    utils::writeReg("GEM_AMC.SLOW_CONTROL.IC.GBTX_LINK_SELECT", linkN);

    // Write to the register
    utils::writeReg("GEM_AMC.SLOW_CONTROL.IC.ADDRESS", address);
    utils::writeReg("GEM_AMC.SLOW_CONTROL.IC.WRITE_DATA", value);
    utils::writeReg("GEM_AMC.SLOW_CONTROL.IC.EXECUTE_WRITE", 1);

}

extern "C" {
    const char *module_version_key = "gbt v1.0.1";
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

        xhal::common::rpc::registerMethod<gbt::writeGBTConfig>(modmgr);
        xhal::common::rpc::registerMethod<gbt::writeGBTPhase>(modmgr);
        xhal::common::rpc::registerMethod<gbt::scanGBTPhases>(modmgr);
    }
}
