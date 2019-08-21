/*! \file amc.cpp
 *  \brief AMC methods for RPC modules
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#include "amc.h"
#include "amc/ttc.h"
#include "amc/daq.h"
#include "amc/sca.h"
#include "amc/blaster_ram.h"
#include "hw_constants.h"
#include "amc/sca.h"

#include "hw_constants.h"

#include <chrono>
#include <string>
#include <time.h>
#include <thread>
#include <vector>

unsigned int fw_version_check(const char* caller_name, localArgs *la)
{
    int iFWVersion = readReg(la, "GEM_AMC.GEM_SYSTEM.RELEASE.MAJOR");
    char regBuf[200];
    switch (iFWVersion) {
        case 1:
        {
            LOGGER->log_message(LogManager::INFO, "System release major is 1, v2B electronics behavior");
            break;
        }
        case 3:
        {
            LOGGER->log_message(LogManager::INFO, "System release major is 3, v3 electronics behavior");
            break;
        }
        default:
        {
            LOGGER->log_message(LogManager::ERROR, "Unexpected value for system release major!");
            sprintf(regBuf,"Unexpected value for system release major!");
            la->response->set_string("error",regBuf);
            break;
        }
    }
    return iFWVersion;
}

uint32_t getOHVFATMaskLocal(localArgs * la, uint32_t ohN)
{
    uint32_t mask = 0xffffff; //Start with all vfats masked for max VFAT/GEB amount
    for (unsigned int vfatN=0; vfatN<oh::VFATS_PER_OH; ++vfatN) { //Loop over all vfats
        uint32_t syncErrCnt = readReg(la, stdsprintf("GEM_AMC.OH_LINKS.OH%i.VFAT%i.SYNC_ERR_CNT",ohN,vfatN));

        if (syncErrCnt == 0x0) { //Case: zero sync errors, unmask this vfat
            mask = mask - (0x1 << vfatN);
        } //End Case: nonzero sync errors, mask this vfat
    } //End loop over all vfats

    return mask;
} //End getOHVFATMaskLocal()

void getOHVFATMask(const RPCMsg *request, RPCMsg *response) {
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");

    uint32_t vfatMask = getOHVFATMaskLocal(&la, ohN);
    LOGGER->log_message(LogManager::INFO, stdsprintf("Determined VFAT Mask for OH%i to be 0x%x",ohN,vfatMask));

    response->set_word("vfatMask",vfatMask);

    rtxn.abort();
} //End getOHVFATMask(...)

void getOHVFATMaskMultiLink(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    int ohMask = 0xfff;
    if (request->get_key_exists("ohMask")) {
        ohMask = request->get_word("ohMask");
    }

    unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (request->get_key_exists("NOH")) {
        unsigned int NOH_requested = request->get_word("NOH");
        if (NOH_requested <= NOH)
            NOH = NOH_requested;
        else
            LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register value (%i), NOH request will be disregarded",NOH_requested,NOH));
    }

    uint32_t ohVfatMaskArray[amc::OH_PER_AMC];
    for (unsigned int ohN=0; ohN<NOH; ++ohN) {
        // If this Optohybrid is masked skip it
        if (!((ohMask >> ohN) & 0x1)) {
            ohVfatMaskArray[ohN] = 0xffffff;
            continue;
        }
        else{
            ohVfatMaskArray[ohN] = getOHVFATMaskLocal(&la, ohN);
            LOGGER->log_message(LogManager::INFO, stdsprintf("Determined VFAT Mask for OH%i to be 0x%x",ohN,ohVfatMaskArray[ohN]));
        }
    } //End Loop over all Optohybrids

    //Debugging
    LOGGER->log_message(LogManager::DEBUG, "All VFAT Masks found, listing:");
    for (unsigned int ohN=0; ohN<amc::OH_PER_AMC; ++ohN) {
        LOGGER->log_message(LogManager::DEBUG, stdsprintf("VFAT Mask for OH%i to be 0x%x",ohN,ohVfatMaskArray[ohN]));
    }

    response->set_word_array("ohVfatMaskArray",ohVfatMaskArray,amc::OH_PER_AMC);

    rtxn.abort();
} //End getOHVFATMaskMultiLink(...)

uint32_t readFPGADone(localArgs *la, const uint32_t ohMask)
{
    const auto reply = sendSCACommandWithReply(la, 0x2, 0x1, 0x1, 0x0, ohMask);

    uint32_t FPGADone = 0;
    for (uint32_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN)
        FPGADone |= ((reply.at(ohN) >> 6) & 1) << ohN;

    return FPGADone;
}

void programAllOptohybridFPGAsLocal(localArgs *la, uint32_t ohMask, const uint32_t nOfIterations, const uint8_t mode)
{
    const bool stopOnError = mode & 0x1;
    const bool checkCSC = (mode >> 1) & 0x1;

    std::array<uint32_t, amc::OH_PER_AMC> hardResetFails{};
    std::array<uint32_t, amc::OH_PER_AMC> progFails{};
    std::array<uint32_t, amc::OH_PER_AMC> commFails{};
    std::array<uint32_t, amc::OH_PER_AMC> triggerFails{};

    // Enable manual controls
    writeReg(la, "GEM_AMC.TTC.GENERATOR.ENABLE", 0x1);
    writeReg(la, "GEM_AMC.SLOW_CONTROL.SCA.CTRL.TTC_HARD_RESET_EN", ohMask);

/*
        writeReg(la, "GEM_AMC.TTC.GENERATOR.SINGLE_HARD_RESET", 0x1);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
*/
        /*for (uint8_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
            if ( ~(ohMask >> ohN) & 0x1 )
                continue;

            writeReg(la, stdsprintf("GEM_AMC.OH.OH%hhu.FPGA.TRIG.ENABLE_RESET", ohN), 0x0);
        }*/


    for (uint32_t i = 0; i < nOfIterations; ++i)
    {
        LOGGER->log_message(LogManager::INFO, stdsprintf("Iteration %d", i));
        bool error = false;

        // Program the FPGA
        writeReg(la, "GEM_AMC.TTC.GENERATOR.SINGLE_HARD_RESET", 0x1);

/*
        for (uint8_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
            if ( ~(ohMask >> ohN) & 0x1 )
                continue;

            writeReg(la, stdsprintf("GEM_AMC.OH.OH%hhu.FPGA.TRIG.LINKS.RESET", ohN), 0x1);
        }
*/

        // FPGA done must be low after hard reset
        const uint32_t FPGADoneAfterReset = readFPGADone(la, ohMask);

        // Wait for FPGA to be programmed ~70 ms
        std::this_thread::sleep_for(std::chrono::milliseconds(80));

        // FPGA done goes high once the FPGA is programmed
        const uint32_t FPGADoneAfterProgramming = readFPGADone(la, ohMask);

        // Wait FPGA initialization
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Resets
        writeReg(la, "GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 0x1);

        // Need to wait after a link reset; not clear why...
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        for (uint8_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
            if ( !( ((ohMask >> ohN) & 0x1) || (ohN ? (checkCSC && ((ohMask >> (ohN-1)) & 0x1)) : 0) ) )
                continue;

            for (const uint8_t channel : oh::triggerLinkMappings::OH_TO_CHANNEL[ohN])
                writeReg(la, stdsprintf("GEM_AMC.OPTICAL_LINKS.MGT_CHANNEL_%hhu.CTRL.RX_ERROR_CNT_RESET", channel), 0x1);
        }
        writeReg(la, "GEM_AMC.TRIGGER.CTRL.MODULE_RESET", 0x1);

        // Wait for errors to build up
        // std::this_thread::sleep_for(std::chrono::milliseconds(10));
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Check programming
        if ( (FPGADoneAfterReset & ohMask) != 0)
            LOGGER->log_message(LogManager::INFO, "Hard reset failed.");
        if ( (~FPGADoneAfterProgramming & ohMask) != 0)
            LOGGER->log_message(LogManager::INFO, "Programming failed.");

        // Check communication with FPGA
        for (uint8_t ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
            if ( ~(ohMask >> ohN) & 0x1 )
                continue;

            if (readReg(la, stdsprintf("GEM_AMC.OH.OH%hhu.FPGA.CONTROL.RELEASE.DATE", ohN)) == 0xdeaddead)
            {
                LOGGER->log_message(LogManager::INFO, stdsprintf("Cannot communicate with the FPGA. (OH%hhu)", ohN));
                ++commFails[ohN];
                error = true;
            }

            const uint32_t sotReady = readReg(la, stdsprintf("GEM_AMC.OH.OH%hhu.FPGA.TRIG.CTRL.SBIT_SOT_READY", ohN));
            const uint32_t sotUnstable = readReg(la, stdsprintf("GEM_AMC.OH.OH%hhu.FPGA.TRIG.CTRL.SBIT_SOT_UNSTABLE", ohN));
            const uint32_t sotInvalidBitskip = readReg(la, stdsprintf("GEM_AMC.OH.OH%hhu.FPGA.TRIG.CTRL.SBIT_SOT_INVALID_BITSKIP", ohN));

            if ( (sotReady != 0xffffff) or (sotUnstable != 0x0) or (sotInvalidBitskip != 0x0) )
            {
                LOGGER->log_message(LogManager::INFO, stdsprintf("Incorrect Sbits initialization. (OH%hhu, %u, %u, %u)", ohN, sotReady, sotUnstable, sotInvalidBitskip));
                error = true;
            }
        }

        // Check the trigger links
        for (uint8_t ohN = 0; ohN < amc::OH_PER_AMC; ++ ohN) {
            if ( !( ((ohMask >> ohN) & 0x1) || (ohN ? (checkCSC && ((ohMask >> (ohN-1)) & 0x1)) : 0) ) )
                continue;

            for (uint8_t linkIdx = 0; linkIdx < 2; ++linkIdx)
            {
                const uint32_t notinTable = readReg(la, stdsprintf("GEM_AMC.OPTICAL_LINKS.MGT_CHANNEL_%hhu.STATUS.RX_NOT_IN_TABLE_CNT", 
                                         oh::triggerLinkMappings::OH_TO_CHANNEL[ohN][linkIdx]));
                const uint32_t missedComma = readReg(la, stdsprintf("GEM_AMC.TRIGGER.OH%hhu.LINK%hhu_MISSED_COMMA_CNT", ohN, linkIdx));
                const uint32_t overflow = readReg(la, stdsprintf("GEM_AMC.TRIGGER.OH%hhu.LINK%hhu_OVERFLOW_CNT", ohN, linkIdx));
                const uint32_t underflow = readReg(la, stdsprintf("GEM_AMC.TRIGGER.OH%hhu.LINK%hhu_UNDERFLOW_CNT", ohN, linkIdx));

                if ( (notinTable > 0) || (missedComma > 0) || (overflow > 0) || (underflow > 0) )
                {
                    LOGGER->log_message(LogManager::INFO, stdsprintf("Bad trigger link : OH%hhu - link%hhu", ohN, linkIdx));
                    LOGGER->log_message(LogManager::INFO, stdsprintf("Not in table : %d - Missed comma : %d - Overflow : %d - Underflow : %d", notinTable, missedComma, overflow, underflow));
                    ++triggerFails[ohN];
                    //if ( (notinTable > 0) || (missedComma > 1) || (overflow > 2) || (underflow > 2) )
                    error = true;
                }
            }
        }

        if (error && stopOnError) break;
    }

    for (uint8_t ohN = 0; ohN < amc::OH_PER_AMC; ++ ohN) {
        LOGGER->log_message(LogManager::INFO, stdsprintf("== Summary == OH%hhu - Comm failures: %d - Trigger failures: %d", ohN, commFails[ohN], triggerFails[ohN]));
    }

    // Disable manual controls
    writeReg(la, "GEM_AMC.TTC.GENERATOR.ENABLE", 0x0);
    writeReg(la, "GEM_AMC.SLOW_CONTROL.SCA.CTRL.TTC_HARD_RESET_EN", 0x0);

} // End programAllOptohybridFPGAsLocal(...)

void programAllOptohybridFPGAs(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    const uint16_t ohMask = request->get_word("ohMask");
    const uint32_t nOfIterations = request->get_word("nOfIterations");
    const uint8_t mode = request->get_word("mode");

    programAllOptohybridFPGAsLocal(&la, ohMask, nOfIterations, mode);

    rtxn.abort();
} // End programAllOptohybridFPGAs

void repeatedRegRead(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    bool breakOnFailure = request->get_word("breakOnFailure");
    uint32_t nReads     = request->get_word("nReads");

    const std::vector<std::string> vec_regList = request->get_string_array("regList");
    slowCtrlErrCntVFAT vfatErrs;
    for (auto const & regIter : vec_regList){
        LOGGER->log_message(LogManager::INFO,stdsprintf("attempting to repeatedly reading register %s for %i times",regIter.c_str(), nReads));
        vfatErrs = vfatErrs + repeatedRegReadLocal(&la, regIter, breakOnFailure, nReads);
    } //End loop over registers in vec_regList

    response->set_word("CRC_ERROR_CNT",          vfatErrs.crc);
    response->set_word("PACKET_ERROR_CNT",       vfatErrs.packet);
    response->set_word("BITSTUFFING_ERROR_CNT",  vfatErrs.bitstuffing);
    response->set_word("TIMEOUT_ERROR_CNT",      vfatErrs.timeout);
    response->set_word("AXI_STROBE_ERROR_CNT",   vfatErrs.axi_strobe);
    response->set_word("SUM",                    vfatErrs.sum);
    response->set_word("TRANSACTION_CNT",        vfatErrs.nTransactions);

    rtxn.abort();
} //End repeatedRegRead

std::vector<uint32_t> sbitReadOutLocal(localArgs *la, uint32_t ohN, uint32_t acquireTime, bool *maxNetworkSizeReached)
{
    //Setup the sbit monitor
    const int nclusters = 8;
    writeReg(la, "GEM_AMC.TRIGGER.SBIT_MONITOR.OH_SELECT", ohN);
    uint32_t addrSbitMonReset=getAddress(la, "GEM_AMC.TRIGGER.SBIT_MONITOR.RESET");
    uint32_t addrSbitL1ADelay=getAddress(la, "GEM_AMC.TRIGGER.SBIT_MONITOR.L1A_DELAY");
    uint32_t addrSbitCluster[nclusters];
    for (int iCluster=0; iCluster < nclusters; ++iCluster) {
        addrSbitCluster[iCluster] = getAddress(la, stdsprintf("GEM_AMC.TRIGGER.SBIT_MONITOR.CLUSTER%i",iCluster) );
    }

    //Take the VFATs out of slow control only mode
    writeReg(la, "GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

    //[0:10] address of sbit cluster
    //[11:13] cluster size
    //[14:26] L1A Delay (consider anything over 4095 as overflow)
    std::vector<uint32_t> storedSbits;

    //readout sbits
    time_t acquisitionTime,startTime;
    bool acquire=true;
    startTime=time(NULL);
    (*maxNetworkSizeReached) = false;
    uint32_t l1ADelay;
    while(acquire) {
        if ( sizeof(uint32_t) * storedSbits.size() > 65000 ) { //Max TCP/IP message is 65535
            (*maxNetworkSizeReached) = true;
            break;
        }

        //Reset monitors
        writeRawAddress(addrSbitMonReset, 0x1, la->response);

        //wait for 4095 clock cycles then read L1A delay
        std::this_thread::sleep_for (std::chrono::nanoseconds(4095*25));
        l1ADelay = readRawAddress(addrSbitL1ADelay, la->response);
        if (l1ADelay > 4095) { //Anything larger than this consider as overflow
            l1ADelay = 4095; //(0xFFF in hex)
        }

        //get sbits
        bool anyValid=false;
        std::vector<uint32_t> tempSBits; //will only be stored into storedSbits if anyValid is true
        for (int cluster=0; cluster<nclusters; ++cluster) {
            //bits [10:0] is the address of the cluster
            //bits [14:12] is the cluster size
            //bits 15 and 11 are not used
            uint32_t thisCluster = readRawAddress(addrSbitCluster[cluster], la->response);
            uint32_t sbitAddress = (thisCluster & 0x7ff);
            int clusterSize = (thisCluster >> 12) & 0x7;
            bool isValid = (sbitAddress < 1536); //Possible values are [0,(24*64)-1]

            if (isValid) {
                LOGGER->log_message(LogManager::INFO,stdsprintf("valid sbit data: thisClstr %x; sbitAddr %x;",thisCluster,sbitAddress));
                anyValid=true;
            }

            //Store the sbit
            tempSBits.push_back( ((l1ADelay & 0x1fff) << 14) + ((clusterSize & 0x7) << 11) + (sbitAddress & 0x7ff) );
        } //End Loop over clusters

        if (anyValid) {
            storedSbits.insert(storedSbits.end(),tempSBits.begin(),tempSBits.end());
        }

        acquisitionTime=difftime(time(NULL),startTime);
        if (uint32_t(acquisitionTime) > acquireTime) {
            acquire=false;
        }
    } //End readout sbits

    return storedSbits;
} //End sbitReadOutLocal(...)

void sbitReadOut(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t acquireTime = request->get_word("acquireTime");

    bool maxNetworkSizeReached = false;

    time_t startTime=time(NULL);
    std::vector<uint32_t> storedSbits = sbitReadOutLocal(&la, ohN, acquireTime, &maxNetworkSizeReached);
    time_t approxLivetime=difftime(time(NULL),startTime);

    if (maxNetworkSizeReached) {
        response->set_word("maxNetworkSizeReached", maxNetworkSizeReached);
        response->set_word("approxLiveTime",approxLivetime);
    }
    response->set_word_array("storedSbits",storedSbits);

    rtxn.abort();
} //End sbitReadOut()

extern "C" {
    const char *module_version_key = "amc v1.0.1";
    int module_activity_color = 4;
    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s", memsvc_get_last_error(memsvc)));
            LOGGER->log_message(LogManager::ERROR, "Unable to load module");
            return; // Do not register our functions, we depend on memsvc.
        }

        modmgr->register_method("amc", "getOHVFATMask",             getOHVFATMask);
        modmgr->register_method("amc", "getOHVFATMaskMultiLink",    getOHVFATMaskMultiLink);
        modmgr->register_method("amc", "repeatedRegRead",           repeatedRegRead);
        modmgr->register_method("amc", "sbitReadOut",               sbitReadOut);
        modmgr->register_method("amc", "programAllOptohybridFPGAs", programAllOptohybridFPGAs);

        // DAQ module methods (from amc/daq)
        modmgr->register_method("amc", "enableDAQLink",           enableDAQLink);
        modmgr->register_method("amc", "disableDAQLink",          disableDAQLink);
        modmgr->register_method("amc", "setZS",                   setZS);
        modmgr->register_method("amc", "resetDAQLink",            resetDAQLink);
        modmgr->register_method("amc", "setDAQLinkInputTimeout",  setDAQLinkInputTimeout);
        modmgr->register_method("amc", "setDAQLinkRunType",       setDAQLinkRunType);
        modmgr->register_method("amc", "setDAQLinkRunParameter",  setDAQLinkRunParameter);
        modmgr->register_method("amc", "setDAQLinkRunParameters", setDAQLinkRunParameters);

        modmgr->register_method("amc", "configureDAQModule",   configureDAQModule);
        modmgr->register_method("amc", "enableDAQModule",      enableDAQModule);

        // TTC module methods (from amc/ttc)
        modmgr->register_method("amc", "ttcModuleReset",     ttcModuleReset);
        modmgr->register_method("amc", "ttcMMCMReset",       ttcMMCMReset);
        modmgr->register_method("amc", "ttcMMCMPhaseShift",  ttcMMCMPhaseShift);
        modmgr->register_method("amc", "checkPLLLock",       checkPLLLock);
        modmgr->register_method("amc", "getMMCMPhaseMean",   getMMCMPhaseMean);
        modmgr->register_method("amc", "getMMCMPhaseMedian", getMMCMPhaseMedian);
        modmgr->register_method("amc", "getGTHPhaseMean",    getGTHPhaseMean);
        modmgr->register_method("amc", "getGTHPhaseMedian",  getGTHPhaseMedian);
        modmgr->register_method("amc", "ttcCounterReset",    ttcCounterReset);
        modmgr->register_method("amc", "getL1AEnable",       getL1AEnable);
        modmgr->register_method("amc", "setL1AEnable",       setL1AEnable);
        modmgr->register_method("amc", "getTTCConfig",       getTTCConfig);
        modmgr->register_method("amc", "setTTCConfig",       setTTCConfig);
        modmgr->register_method("amc", "getTTCStatus",       getTTCStatus);
        modmgr->register_method("amc", "getTTCErrorCount",   getTTCErrorCount);
        modmgr->register_method("amc", "getTTCCounter",      getTTCCounter);
        modmgr->register_method("amc", "getL1AID",           getL1AID);
        modmgr->register_method("amc", "getL1ARate",         getL1ARate);
        modmgr->register_method("amc", "getTTCSpyBuffer",    getTTCSpyBuffer);

        // SCA module methods (from amc/sca)
        // modmgr->register_method("amc", "scaHardResetEnable", scaHardResetEnable);
        modmgr->register_method("amc", "readSCAADCSensor", readSCAADCSensor);
        modmgr->register_method("amc", "readSCAADCTemperatureSensors", readSCAADCTemperatureSensors);
        modmgr->register_method("amc", "readSCAADCVoltageSensors", readSCAADCVoltageSensors);
        modmgr->register_method("amc", "readSCAADCSignalStrengthSensors", readSCAADCSignalStrengthSensors);
        modmgr->register_method("amc", "readAllSCAADCSensors", readAllSCAADCSensors);

        // BLASTER RAM module methods (from amc/blaster_ram)
        modmgr->register_method("amc", "writeConfRAM", writeConfRAM);
        modmgr->register_method("amc", "readConfRAM",  readConfRAM);
    }
}
