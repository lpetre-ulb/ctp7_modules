#include <algorithm>
#include "amc.h"
#include "calibration_routines.h"
#include <chrono>
#include <math.h>
#include <pthread.h>
#include "optohybrid.h"
#include <thread>
#include "vfat3.h"
#include "hw_constants.h"

using namespace std::string_literals;

std::unordered_map<std::string, uint32_t> setSingleChanMask(unsigned int ohN, unsigned int vfatN, unsigned int ch, localArgs *la)
{
    std::unordered_map<std::string, uint32_t> map_chanOrigMask;

    for (unsigned int chan=0; chan<128; ++chan) { //Loop Over All Channels
        uint32_t chMask = 1;
        if ( ch == chan) //Do not mask the channel of interest
            chMask = 0;

        //store the original channel mask
        const std::string reg = "GEM_AMC.OH.OH"s + std::to_string(ohN) + ".GEB.VFAT" + std::to_string(vfatN) + ".VFAT_CHANNELS.CHANNEL" + std::to_string(chan) + ".MASK";
        map_chanOrigMask[reg] = readReg(la, reg);

        //write the new channel mask
        writeReg(la, reg, chMask);
    }

    return map_chanOrigMask;
}

void applyChanMask(std::unordered_map<std::string, uint32_t> map_chanOrigMask, localArgs *la)
{
    for (auto chanPtr = map_chanOrigMask.begin(); chanPtr != map_chanOrigMask.end(); ++chanPtr) {
        writeReg(la, (*chanPtr).first, (*chanPtr).second);
    }
}

bool confCalPulseLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t ch, bool toggleOn, bool currentPulse, uint32_t calScaleFactor)
{
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~mask & 0xFFFFFF;

    char regBuf[200];
    if (ch >= 128 && toggleOn == true) { //Case: Bad Config, asked for OR of all channels
        la->response->set_string("error","confCalPulseLocal(): I was told to calpulse all channels which doesn't make sense");
        return false;
    } //End Case: Bad Config, asked for OR of all channels
    else if (ch == 128 && toggleOn == false) { //Case: Turn cal pusle off for all channels
        for (unsigned int vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++) { //Loop over all VFATs
            if ((notmask >> vfatN) & 0x1) { //End VFAT is not masked
                for (unsigned int chan=0; chan < 128; ++chan) { //Loop Over all Channels
                    sprintf(regBuf,"GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i.CALPULSE_ENABLE", ohN, vfatN, chan);
                    writeReg(la, regBuf, 0x0);
                } //End Loop Over all Channels
                writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_CAL_MODE", ohN, vfatN), 0x0);
            } //End VFAT is not masked
        } //End Loop over all VFATs
    } //End Case: Turn cal pulse off for all channels
    else if (ch >= 128) { //Case: Bad Config, asked for OR of all channels
        la->response->set_string("error","confCalPulseLocal(): I was told to calpulse (on/off) channels > 128.");
        return false;
    } //End Case: Bad Config, asked for OR of all channels
    else{ //Case: Pulse a specific channel
        for (unsigned int vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++) { //Loop over all VFATs
            if ((notmask >> vfatN) & 0x1) { //End VFAT is not masked
                sprintf(regBuf,"GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i.CALPULSE_ENABLE", ohN, vfatN, ch);
                if (toggleOn == true) { //Case: turn calpulse on
                    writeReg(la, regBuf, 0x1);
                    if (currentPulse) { //Case: cal mode current injection
                        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_CAL_MODE", ohN, vfatN), 0x2);

                        //Set cal current pulse scale factor. Q = CAL DUR[s] * CAL DAC * 10nA * CAL FS[%] (00 = 25%, 01 = 50%, 10 = 75%, 11 = 100%)
                        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_CAL_FS", ohN, vfatN), calScaleFactor);
                        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_CAL_DUR", ohN, vfatN), 0x0);
                    } //End Case: cal mode current injection
                    else { //Case: cal mode voltage injection
                        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_CAL_MODE", ohN, vfatN), 0x1);
                    } //Case: cal mode voltage injection
                } //End Case: Turn calpulse on
                else{ //Case: Turn calpulse off
                    writeReg(la, regBuf, 0x0);
                    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_CAL_MODE", ohN, vfatN), 0x0);
                } //End Case: Turn calpulse off
            } //End VFAT is not masked
        } //End Loop over all VFATs
    } //End Case: Pulse a specific channel

    return true;
} //End confCalPulseLocal

void dacMonConfLocal(localArgs * la, uint32_t ohN, uint32_t ch)
{
    //Check the firmware version
    char regBuf[200];
    switch (fw_version_check("dacMonConf", la)) {
        case 3:
        {
            writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.ENABLE", 0x0);
            writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.RESET", 0x1);
            writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.OH_SELECT", ohN);
            if (ch>127)
            {
                writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.VFAT_CHANNEL_GLOBAL_OR", 0x1);
            }
            else
            {
                writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.VFAT_CHANNEL_SELECT", ch);
                writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.VFAT_CHANNEL_GLOBAL_OR", 0x0);
            }
            break;
        }// end v3 electronics behavior
        default:
        {
            LOGGER->log_message(LogManager::ERROR, "dacMonConf is only supported in V3 electronics");
            sprintf(regBuf,"dacMonConf is only supported in V3 electronics");
            la->response->set_string("error",regBuf);
            break;
        }
    }
    return;
}

void ttcGenToggleLocal(localArgs * la, uint32_t ohN, bool enable)
{
    //Check firmware version
    switch(fw_version_check("ttcGenToggle", la)) {
        case 3: //v3 electronics behavior
        {
            if (enable) {
                writeReg(la, "GEM_AMC.TTC.GENERATOR.ENABLE", 0x1); //Internal TTC generator enabled, TTC cmds from backplane are ignored
            }
            else{
                writeReg(la, "GEM_AMC.TTC.GENERATOR.ENABLE", 0x0); //Internal TTC generator disabled, TTC cmds from backplane
            }
            break;
        }//End v3 electronics behavior
        case 1: //v2b electronics behavior
        {
            //base reg
            std::stringstream sstream;
            sstream<<ohN;
            std::string contBase = "GEM_AMC.OH.OH" + sstream.str() + ".T1Controller";

            if (enable) { //Start
                if ( !(readReg(la, contBase + ".MONITOR"))) {
                    writeReg(la, contBase + ".TOGGLE", 0x1);   //Enable
                }
            }
            else { //Stop
                if ( readReg(la, contBase + ".MONITOR")) {
                    writeReg(la, contBase + ".TOGGLE", 0x0);   //Disable
                }
            }
            break;
        }//End v2b electronics behavior
        default:
        {
            LOGGER->log_message(LogManager::ERROR, "Unexpected value for system release major, do nothing");
            break;
        }
    }
    return;
} //End ttcGenToggleLocal(...)

void ttcGenToggle(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    bool enable = request->get_word("enable");

    ttcGenToggleLocal(&la, ohN, enable);
    rtxn.abort();
} //End ttcGenToggle(...)

void ttcGenConfLocal(localArgs * la, uint32_t ohN, uint32_t mode, uint32_t type, uint32_t pulseDelay, uint32_t L1Ainterval, uint32_t nPulses, bool enable)
{
    //Check firmware version
    LOGGER->log_message(LogManager::INFO, "Entering ttcGenConfLocal");
    switch(fw_version_check("ttcGenConf", la)) {
        case 0x3: //v3 electronics behavior
        {
            LOGGER->log_message(LogManager::INFO, "ttcGenConfLocal: V3 behavior");
            writeReg(la, "GEM_AMC.TTC.GENERATOR.RESET", 0x1);
            writeReg(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_L1A_GAP", L1Ainterval);
            writeReg(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_CALPULSE_TO_L1A_GAP", pulseDelay);
            break;
        }//End v3 electronics behavior
        case 0x1: //v2b electronics behavior
        {
            //base reg
            std::stringstream sstream;
            sstream<<ohN;
            std::string contBase = "GEM_AMC.OH.OH" + sstream.str() + ".T1Controller";

            //reset the controller
            writeReg(la, contBase + ".RESET",0x1);

            //Set the mode
            writeReg(la, contBase + ".MODE",mode);
            LOGGER->log_message(LogManager::DEBUG, stdsprintf("OH%i : Configuring T1 Controller for mode 0x%x (0x%x)",
                        ohN,mode,
                        readReg(la, contBase + ".MODE")
                        )
                    );

            if (mode == 0) {
                writeReg(la, contBase + ".TYPE", type);
                LOGGER->log_message(LogManager::DEBUG, stdsprintf("OH%i : Configuring T1 Controller for type 0x%x (0x%x)",
                            ohN,type,
                            readReg(la, contBase + ".TYPE")
                            )
                        );
            }
            if (mode == 1) {
                writeReg(la, contBase + ".DELAY", pulseDelay);
                LOGGER->log_message(LogManager::DEBUG, stdsprintf("OH%i : Configuring T1 Controller for delay %i (%i)",
                            ohN,pulseDelay,
                            readReg(la, contBase + ".DELAY")
                            )
                        );
            }
            if (mode != 2) {
                writeReg(la, contBase + ".INTERVAL", L1Ainterval);
                LOGGER->log_message(LogManager::DEBUG, stdsprintf("OH%i : Configuring T1 Controller for interval %i (%i)",
                            ohN,L1Ainterval,
                            readReg(la, contBase + ".INTERVAL")
                            )
                        );
            }

            writeReg(la, contBase + ".NUMBER", nPulses);
            LOGGER->log_message(LogManager::DEBUG, stdsprintf("OH%i : Configuring T1 Controller for nsignals %i (%i)",
                        ohN,nPulses,
                        readReg(la, contBase + ".NUMBER")
                        )
                    );
            break;
        }//End v2b electronics behavior
        default:
        {
            LOGGER->log_message(LogManager::ERROR, "Unexpected value for system release major, do nothing");
            break;
        }
    }
    //start or stop
    LOGGER->log_message(LogManager::INFO, "ttcGenConfLocal: call ttcGenToggleLocal");
    ttcGenToggleLocal(la, ohN, enable);
    return;
}

void ttcGenConf(const RPCMsg *request, RPCMsg *response)
{
    LOGGER->log_message(LogManager::INFO, "Entering ttcGenConf");
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t mode = request->get_word("mode");
    uint32_t type = request->get_word("type");
    uint32_t pulseDelay = request->get_word("pulseDelay");
    uint32_t L1Ainterval = request->get_word("L1Ainterval");
    uint32_t nPulses = request->get_word("nPulses");
    bool enable = request->get_word("enable");

    LOGGER->log_message(LogManager::INFO, stdsprintf("Calling ttcGenConfLocal with ohN : %i, mode : %i, type : %i, pulse delay : %i, L1A interval : %i, number of pulses : %i", ohN,mode,type,pulseDelay,L1Ainterval,nPulses));
    ttcGenConfLocal(&la, ohN, mode, type, pulseDelay, L1Ainterval, nPulses, enable);

    rtxn.abort();
}

void genScanLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t mask, uint32_t ch, bool useCalPulse, bool currentPulse, uint32_t calScaleFactor, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg, bool useUltra, bool useExtTrig)
{
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~mask & 0xFFFFFF;

    //Check firmware version
    switch(fw_version_check("genScanLocal", la)) {
        case 3: //v3 electronics behavior
        {
            uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
            char regBuf[200];
            if ( (notmask & goodVFATs) != notmask)
            {
                sprintf(regBuf,"One of the unmasked VFATs is not Synced. goodVFATs: %x\tnotmask: %x",goodVFATs,notmask);
                la->response->set_string("error",regBuf);
                return;
            }

            if (currentPulse && calScaleFactor > 3) {
                sprintf(regBuf,"Bad value for CFG_CAL_FS: %x, Possible values are {0b00, 0b01, 0b10, 0b11}. Exiting.",calScaleFactor);
                la->response->set_string("error",regBuf);
                return;
            }

            //Do we turn on the calpulse for the channel = ch?
            if (useCalPulse) {
                if (confCalPulseLocal(la, ohN, mask, ch, true, currentPulse, calScaleFactor) == false) {
                    la->response->set_string("error",stdsprintf("Unable to configure calpulse ON for ohN %i mask %x chan %i", ohN, mask, ch));
                    return; //Calibration pulse is not configured correctly
                }
            } //End use calibration pulse

            //Get addresses
            uint32_t daqMonAddr[oh::VFATS_PER_OH];
            uint32_t l1CntAddr = getAddress(la, "GEM_AMC.TTC.CMD_COUNTERS.L1A");
            for (unsigned int vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++)
            {
                sprintf(regBuf,"GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.VFAT%i.GOOD_EVENTS_COUNT",vfatN);
                daqMonAddr[vfatN] = getAddress(la, regBuf);
            }

            //TTC Config
            if (useExtTrig) {
                writeReg(la, "GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x0);
                writeReg(la, "GEM_AMC.TTC.CTRL.CNT_RESET", 0x1);
            }
            else{
                writeReg(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_L1A_COUNT", nevts);
                writeReg(la, "GEM_AMC.TTC.GENERATOR.SINGLE_RESYNC", 0x1);
            }

            //Configure VFAT_DAQ_MONITOR
            dacMonConfLocal(la, ohN, ch);

            //Scan over DAC values
            for (uint32_t dacVal = dacMin; dacVal <= dacMax; dacVal += dacStep)
            {
                //Write the scan reg value
                for (unsigned int vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++) if ((notmask >> vfatN) & 0x1)
                {
                    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_%s",ohN,vfatN,scanReg.c_str()), dacVal);
                }

                //Reset and enable the VFAT_DAQ_MONITOR
                writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.RESET", 0x1);
                writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.ENABLE", 0x1);

                //Start the triggers
                if (useExtTrig) {
                    writeReg(la, "GEM_AMC.TTC.CTRL.CNT_RESET", 0x1);
                    writeReg(la, "GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x1);

                    uint32_t l1aCnt = 0;
                    while(l1aCnt < nevts) {
                        l1aCnt = readRawAddress(l1CntAddr, la->response);
                        std::this_thread::sleep_for(std::chrono::microseconds(200));
                    }

                    writeReg(la, "GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x0);
                    l1aCnt = readRawAddress(l1CntAddr, la->response);
                }
                else{
                    writeReg(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_START", 0x1);
                    if (readReg(la, "GEM_AMC.TTC.GENERATOR.ENABLE")) { //TTC Commands from TTC.GENERATOR
                        while(readReg(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_RUNNING")) {
                            std::this_thread::sleep_for(std::chrono::microseconds(50));
                        }
                    } //End TTC Commands from TTC.GENERATOR
                }

                //Stop the DAQ monitor counters from incrementing
                writeReg(la, "GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.CTRL.ENABLE", 0x0);

                //Read the DAQ Monitor counters
                for (unsigned int vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++) {
                    if ( !( (notmask >> vfatN) & 0x1)) continue;

                    unsigned int idx = vfatN*(dacMax-dacMin+1)/dacStep+(dacVal-dacMin)/dacStep;
                    outData[idx] = readRawAddress(daqMonAddr[vfatN], la->response);

                    LOGGER->log_message(LogManager::DEBUG, stdsprintf("%s Value: %i; Readback Val: %i; Nhits: %i; Nev: %i; CFG_THR_ARM: %i",
                                 scanReg.c_str(),
                                 dacVal,
                                 readReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_%s",ohN,vfatN,scanReg.c_str())),
                                 readReg(la, stdsprintf("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.VFAT%i.CHANNEL_FIRE_COUNT",vfatN)),
                                 readReg(la, stdsprintf("GEM_AMC.GEM_TESTS.VFAT_DAQ_MONITOR.VFAT%i.GOOD_EVENTS_COUNT",vfatN)),
                                 readReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_THR_ARM_DAC",ohN,vfatN,scanReg.c_str()))
                        )
                    );
                } //End Loop over vfats
            } //End Loop from dacMin to dacMax

            //If the calpulse for channel ch was turned on, turn it off
            if (useCalPulse) {
                if (confCalPulseLocal(la, ohN, mask, ch, false, currentPulse, calScaleFactor) == false) {
                    la->response->set_string("error",stdsprintf("Unable to configure calpulse OFF for ohN %i mask %x chan %i", ohN, mask, ch));
                    return; //Calibration pulse is not configured correctly
                }
            }
            break;
        }//End v3 electronics behavior
        case 1: //v2b electronics behavior
        {
            //Determine scanmode
            std::map<int, std::string> map_strKnownRegs; //Key -> scanmode; val -> register

            map_strKnownRegs[0] = "VThreshold1";
            map_strKnownRegs[1] = "VThreshold1PerChan";
            map_strKnownRegs[2] = "Latency";
            map_strKnownRegs[3] = "VCal";
            map_strKnownRegs[4] = "VThreshold1Trk";

            uint32_t scanmode = 1000;

            for (auto knownRegIter = map_strKnownRegs.begin(); knownRegIter != map_strKnownRegs.end(); ++knownRegIter) {
                //Comparison code goes here
                if ( (*knownRegIter).second.compare(scanReg) == 0) {
                    scanmode = (*knownRegIter).first;
                    break;
                }
            }

            //scanmode not understood
            if (scanmode == 1000) {
                std::string strError = "scanReg: " + scanReg + " not understood.  Supported values are:\n";
                for (auto knownRegIter = map_strKnownRegs.begin(); knownRegIter != map_strKnownRegs.end(); ++knownRegIter) {
                    scanReg += ((*knownRegIter).second + "\n");
                }
                la->response->set_string("error",strError);
            }

            //Configure scan module
            uint32_t vfatN = 0;
            if (!useUltra) {
                //If we are not performing an ultraScan, take the first non-masked VFAT
                for (unsigned int vfat=0; vfat<oh::VFATS_PER_OH; ++vfat) {
                    if ((notmask >> vfat) & 0x1) {
                        vfatN=vfat;
                        break;
                    }
                }
            }

            configureScanModuleLocal(la, ohN, vfatN, scanmode, useUltra, mask, ch, nevts, dacMin, dacMax, dacStep);

            //Print scan configuration
            printScanConfigurationLocal(la, ohN, useUltra);

            //Do we turn on the calpulse for the channel = ch?
            uint32_t trimVal=0;
            if (useCalPulse) {
                if (ch >= 128) {
                    la->response->set_string("error","It doesn't make sense to calpulse all channels");
                    return;
                }
                else{
                    for (unsigned int vfat=0; vfat<oh::VFATS_PER_OH; ++vfat) {
                        if ( (notmask >> vfat) & 0x1) {
                            trimVal = (0x3f & readReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFATS.VFAT%i.VFATChannels.ChanReg%i",ohN,vfat,ch)));
                            writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFATS.VFAT%i.VFATChannels.ChanReg%i",ohN,vfat,ch),trimVal+64);
                        }
                    }
                }
            }

            //Start scan configuration
            startScanModuleLocal(la, ohN, useUltra);

            //If the calpulse for channel ch was turned on, turn it off
            if (useCalPulse) {
                for (unsigned int vfat=0; vfat<oh::VFATS_PER_OH; ++vfat) {
                    if ( (notmask >> vfat) & 0x1) {
                        trimVal = (0x3f & readReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFATS.VFAT%i.VFATChannels.ChanReg%i",ohN,vfat,ch)));
                        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFATS.VFAT%i.VFATChannels.ChanReg%i",ohN,vfat,ch),trimVal);
                    }
                }
            }

            //Get scan results
            getUltraScanResultsLocal(la, outData, ohN, nevts, dacMin, dacMax, dacStep);
            break;
        }//End v2b electronics behavior
        default:
        {
            LOGGER->log_message(LogManager::ERROR, "Unexpected value for system release major, do nothing");
            break;
        }
    }
    return;
} //End genScanLocal(...)

void genScan(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t nevts = request->get_word("nevts");
    uint32_t ohN = request->get_word("ohN");
    uint32_t ch = request->get_word("ch");
    uint32_t mask = request->get_word("mask");
    uint32_t dacMin = request->get_word("dacMin");
    uint32_t dacMax = request->get_word("dacMax");
    uint32_t dacStep = request->get_word("dacStep");
    bool useCalPulse = request->get_word("useCalPulse");
    bool currentPulse = request->get_word("currentPulse");
    uint32_t calScaleFactor = request->get_word("calScaleFactor");
    std::string scanReg = request->get_string("scanReg");

    bool useUltra = false;
    if (request->get_key_exists("useUltra")) {
        useUltra = true;
    }
    bool useExtTrig = request->get_word("useExtTrig");

    uint32_t outData[oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep];
    genScanLocal(&la, outData, ohN, mask, ch, useCalPulse, currentPulse, calScaleFactor, nevts, dacMin, dacMax, dacStep, scanReg, useUltra, useExtTrig);
    response->set_word_array("data",outData,oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep);

    rtxn.abort();
}

void sbitRateScanLocal(localArgs *la, uint32_t *outDataDacVal, uint32_t *outDataTrigRate, uint32_t ohN, uint32_t maskOh, bool invertVFATPos, uint32_t ch, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg, uint32_t waitTime)
{
    char regBuf[200];
    switch (fw_version_check("SBIT Rate Scan", la)) {
        case 3:
        {
            //Hard code possible maskOh values and how they map to vfatN
            std::unordered_map<uint32_t,uint32_t> map_maskOh2vfatN;
            map_maskOh2vfatN[0xfffffe] = 0;
            map_maskOh2vfatN[0xfffffd] = 1;
            map_maskOh2vfatN[0xfffffb] = 2;
            map_maskOh2vfatN[0xfffff7] = 3;
            map_maskOh2vfatN[0xffffef] = 4;
            map_maskOh2vfatN[0xffffdf] = 5;
            map_maskOh2vfatN[0xffffbf] = 6;
            map_maskOh2vfatN[0xffff7f] = 7;
            map_maskOh2vfatN[0xfffeff] = 8;
            map_maskOh2vfatN[0xfffdff] = 9;
            map_maskOh2vfatN[0xfffbff] = 10;
            map_maskOh2vfatN[0xfff7ff] = 11;
            map_maskOh2vfatN[0xffefff] = 12;
            map_maskOh2vfatN[0xffdfff] = 13;
            map_maskOh2vfatN[0xffbfff] = 14;
            map_maskOh2vfatN[0xff7fff] = 15;
            map_maskOh2vfatN[0xfeffff] = 16;
            map_maskOh2vfatN[0xfdffff] = 17;
            map_maskOh2vfatN[0xfbffff] = 18;
            map_maskOh2vfatN[0xf7ffff] = 19;
            map_maskOh2vfatN[0xefffff] = 20;
            map_maskOh2vfatN[0xdfffff] = 21;
            map_maskOh2vfatN[0xbfffff] = 22;
            map_maskOh2vfatN[0x7fffff] = 23;

            //Determine vfatN based on input maskOh
            auto vfatNptr = map_maskOh2vfatN.find(maskOh);
            if ( vfatNptr == map_maskOh2vfatN.end() ) {
                sprintf(regBuf,"Input maskOh: %x not recgonized. Please make sure all but one VFAT is unmasked and then try again", maskOh);
                la->response->set_string("error",regBuf);
                return;
            }
            uint32_t vfatN = (invertVFATPos) ? 23 - (*vfatNptr).second : (*vfatNptr).second;

            uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
            if ( !( (goodVFATs >> vfatN ) & 0x1 ) ) {
                sprintf(regBuf,"The requested VFAT is not synced; goodVFATs: %x\t requested VFAT: %i; maskOh: %x", goodVFATs, vfatN, maskOh);
                la->response->set_string("error",regBuf);
                return;
            }

            //If ch!=128 store the original channel mask settings
            //Then mask all other channels except for channel ch
            std::unordered_map<std::string, uint32_t> map_chanOrigMask; //key -> reg addr; val -> reg value
            if ( ch != 128) map_chanOrigMask = setSingleChanMask(ohN,vfatN,ch,la);

            //Get the OH Rate Monitor Address
            sprintf(regBuf,"GEM_AMC.TRIGGER.OH%i.TRIGGER_RATE",ohN);
            uint32_t ohTrigRateAddr = getAddress(la, regBuf);

            //Store the original OH VFAT Mask, and then reset it
            sprintf(regBuf,"GEM_AMC.OH.OH%i.FPGA.TRIG.CTRL.VFAT_MASK",ohN);
            uint32_t ohVFATMaskAddr = getAddress(la, regBuf);
            uint32_t maskOhOrig = readRawAddress(ohVFATMaskAddr, la->response);   //We'll write this later
            writeRawAddress(ohVFATMaskAddr, maskOh, la->response);

            //Take the VFATs out of slow control only mode
            writeReg(la, "GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

            //Loop from dacMin to dacMax in steps of dacStep
            for (uint32_t dacVal = dacMin; dacVal <= dacMax; dacVal += dacStep) {
                sprintf(regBuf,"GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_%s",ohN,vfatN,scanReg.c_str());
                writeReg(la, regBuf, dacVal);
                std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));

                unsigned int idx = (dacVal-dacMin)/dacStep;
                outDataDacVal[idx] = dacVal;
                outDataTrigRate[idx] = readRawAddress(ohTrigRateAddr, la->response);
            } //End Loop from dacMin to dacMax

            //Restore the original channel masks if specific channel was requested
            if ( ch != 128) applyChanMask(map_chanOrigMask, la);

            //Restore the original maskOh
            writeRawAddress(ohVFATMaskAddr, maskOhOrig, la->response);

            break;
        }//End v3 electronics behavior
        default:
        {
            LOGGER->log_message(LogManager::ERROR, "sbitRateScan is only supported in V3 electronics");
            sprintf(regBuf,"sbitRateScan is only supported in V3 electronics");
            la->response->set_string("error",regBuf);
            break;
        }
    }
    return;
} //End sbitRateScanLocal(...)

void sbitRateScanParallelLocal(localArgs *la, uint32_t *outDataDacVal, uint32_t *outDataTrigRatePerVFAT, uint32_t *outDataTrigRateOverall, uint32_t ch, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg, uint32_t ohMask=0xFFF, uint32_t waitTime=1)
{
    char regBuf[200];
    // Check that OH mask does not exceeds 0xFFF
    if (ohMask > 0xFFF) {
         LOGGER->log_message(LogManager::ERROR, "sbitRateScan supports only up to 12 optohybrids per CTP7");
         sprintf(regBuf,"sbitRateScan supports only up to 12 optohybrids per CTP7");
         la->response->set_string("error",regBuf);
         return;
    }
    uint32_t vfatmask[amc::OH_PER_AMC] = {0};
    std::unordered_map<std::string, uint32_t> origVFATmasks[amc::OH_PER_AMC][oh::VFATS_PER_OH];
    switch (fw_version_check("SBIT Rate Scan", la)){
        case 3:
        {
            // Loop over ohMask and retrieve VFAT masks
            for (unsigned int ohN = 0; ohN < amc::OH_PER_AMC; ++ohN)
            {
                if ((ohMask >> ohN) & 0x1)
                {
                    vfatmask[ohN] = getOHVFATMaskLocal(la, ohN);
                    LOGGER->log_message(LogManager::INFO, stdsprintf("VFAT Mask for OH%i Determined to be 0x%x",ohN,vfatmask[ohN]));

                    //If ch!=128 store the original channel mask settings
                    if( ch != 128)
                    {
                        //Then mask all other channels except for channel ch
                        uint32_t notmask = ~vfatmask[ohN] & 0xFFFFFF;
                        for(unsigned int vfat = 0; vfat < oh::VFATS_PER_OH; ++vfat)
                        {
                            //Skip this vfat if it's masked
                            if ( !( (notmask >> vfat) & 0x1)) continue;
                            origVFATmasks[ohN][vfat] = setSingleChanMask(ohN,vfat,ch,la);
                        } //End loop over all vfats
                    } //End loop over vfat channels
                } //End case this OH is not masked
            } //End loop over optohybrids

            //Get the SBIT Rate Monitor Address
            uint32_t ohTrigRateAddr[amc::OH_PER_AMC][oh::VFATS_PER_OH + 1]; //idx 0->oh::VFATS_PER_OH VFAT counters; last idx - overall rate
            for (unsigned int ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
                if ((ohMask >> ohN) & 0x1) {
                    sprintf(regBuf,"GEM_AMC.TRIGGER.OH%i.TRIGGER_RATE",ohN);
                    ohTrigRateAddr[ohN][oh::VFATS_PER_OH] = getAddress(la, regBuf);
                    for(unsigned int vfat=0; vfat<oh::VFATS_PER_OH; ++vfat){
                        sprintf(regBuf,"GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.VFAT%i_SBITS",ohN,vfat);
                        ohTrigRateAddr[ohN][vfat] = getAddress(la, regBuf);
                    } //End loop over all VFATs
                }
            } //End loop over optohybrids

            //Take the VFATs out of slow control only mode
            writeReg(la, "GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

            //Prep the SBIT counters
            std::unordered_map<uint32_t,uint32_t> map_origSBITPersist;
            std::unordered_map<uint32_t,uint32_t> map_origSBITTimeMax;            
            for (unsigned int ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
                if ((ohMask >> ohN) & 0x1) {
                    map_origSBITPersist[ohN] = readReg(la,  stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.SBIT_CNT_PERSIST",ohN));
                    map_origSBITTimeMax[ohN] = readReg(la,  stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.SBIT_CNT_TIME_MAX",ohN));
                    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.SBIT_CNT_PERSIST",ohN), 0x0); //reset all counters after SBIT_CNT_TIME_MAX
                    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.SBIT_CNT_TIME_MAX",ohN), uint32_t(0x02638e98*waitTime) ); //count for a number of BX's specified by waitTime
                }
            }

            //Loop from dacMin to dacMax in steps of dacStep
            for (uint32_t dacVal = dacMin; dacVal <= dacMax; dacVal += dacStep) {
                LOGGER->log_message(LogManager::INFO, stdsprintf("Setting %s to %i for all optohybrids in 0x%x",scanReg.c_str(),dacVal,ohMask));

                //Set the scan register value
                for (unsigned int ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
                    if ((ohMask >> ohN) & 0x1) {
                        uint32_t notmask = ~vfatmask[ohN] & 0xFFFFFF;
                        for(unsigned int vfat=0; vfat<oh::VFATS_PER_OH; ++vfat){
                            if ( !( (notmask >> vfat) & 0x1)) continue;
                            sprintf(regBuf,"GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_%s",ohN,vfat,scanReg.c_str());
                            writeReg(la, regBuf, dacVal);
                        } //End Loop Over all VFATs
                    } // End checking whether the OH is masked
                } // End loop over optohybrids

                //Reset the counters
                for (unsigned int ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
                    if ((ohMask >> ohN) & 0x1) {
                        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.RESET",ohN), 0x1);
                    } // End checking whether the OH is masked
                } // End loop over optohybrids

                std::this_thread::sleep_for(std::chrono::seconds(waitTime));

                //Read the counters
                for (unsigned int ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
                    if ((ohMask >> ohN) & 0x1) {
                        uint32_t notmask = ~vfatmask[ohN] & 0xFFFFFF;
                        unsigned int idx = ohN*(dacMax-dacMin+1)/dacStep + (dacVal-dacMin)/dacStep;
                        outDataDacVal[idx] = dacVal;
                        outDataTrigRateOverall[idx] = readRawAddress(ohTrigRateAddr[ohN][oh::VFATS_PER_OH], la->response);
                        for(unsigned int vfat=0; vfat<oh::VFATS_PER_OH; ++vfat){
                            if ( !( (notmask >> vfat) & 0x1)) continue;

                            idx = ohN*oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep + vfat*(dacMax-dacMin+1)/dacStep+(dacVal-dacMin)/dacStep;
                            outDataTrigRatePerVFAT[idx] = readRawAddress(ohTrigRateAddr[ohN][vfat], la->response);
                        } //End Loop Over all VFATs
                    } // End checking whether the OH is masked
                } // End loop over optohybrids
            } //End Loop from dacMin to dacMax

            //Restore the original SBIT counter persist setting
            for (unsigned int ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
                if ((ohMask >> ohN) & 0x1) {
                    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.SBIT_CNT_PERSIST",ohN), map_origSBITPersist[ohN]);
                    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.SBIT_CNT_TIME_MAX",ohN), map_origSBITTimeMax[ohN]);                    
                }
            }

            //Restore the original channel masks if specific channel was requested
            if( ch != 128) {
                for (unsigned int ohN = 0; ohN < amc::OH_PER_AMC; ++ohN) {
                    if ((ohMask >> ohN) & 0x1) {
                        uint32_t notmask = ~vfatmask[ohN] & 0xFFFFFF;
                        for(unsigned int vfat=0; vfat<oh::VFATS_PER_OH; ++vfat){
                            if ( !( (notmask >> vfat) & 0x1)) continue;
                            applyChanMask(origVFATmasks[ohN][vfat], la);
                        }
                    }
                }
            }
            break;
        }//End v3 electronics behavior
        default:
        {
            LOGGER->log_message(LogManager::ERROR, "sbitRateScan is only supported in V3 electronics");
            sprintf(regBuf,"sbitRateScan is only supported in V3 electronics");
            la->response->set_string("error",regBuf);
            break;
        }
    }
return;
} //End sbitRateScanParallel(...)

void sbitRateScan(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohMask = request->get_word("ohMask");
    uint32_t ch = request->get_word("ch");
    uint32_t dacMin = request->get_word("dacMin");
    uint32_t dacMax = request->get_word("dacMax");
    uint32_t dacStep = request->get_word("dacStep");
    uint32_t waitTime = request->get_word("waitTime");    
    std::string scanReg = request->get_string("scanReg");

    uint32_t outDataTrigRatePerVFAT[amc::OH_PER_AMC*oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep];
    uint32_t outDataDacValPerOH[amc::OH_PER_AMC*(dacMax-dacMin+1)/dacStep];
    uint32_t outDataTrigRatePerOH[amc::OH_PER_AMC*(dacMax-dacMin+1)/dacStep];
    sbitRateScanParallelLocal(&la, outDataDacValPerOH, outDataTrigRatePerVFAT, outDataTrigRatePerOH, ch, dacMin, dacMax, dacStep, scanReg, ohMask, waitTime);

    response->set_word_array("outDataVFATRate", outDataTrigRatePerVFAT, amc::OH_PER_AMC*oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep);
    response->set_word_array("outDataDacValue", outDataDacValPerOH, amc::OH_PER_AMC*(dacMax-dacMin+1)/dacStep);
    response->set_word_array("outDataCTP7Rate", outDataTrigRatePerOH, amc::OH_PER_AMC*(dacMax-dacMin+1)/dacStep);

    return;
} //End sbitRateScan(...)

void checkSbitMappingWithCalPulseLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t vfatN, uint32_t mask, bool useCalPulse, bool currentPulse, uint32_t calScaleFactor, uint32_t nevts, uint32_t L1Ainterval, uint32_t pulseDelay)
{
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~mask & 0xFFFFFF;

    char regBuf[200];
    if ( fw_version_check("checkSbitMappingWithCalPulse", la) < 3) {
        LOGGER->log_message(LogManager::ERROR, "checkSbitMappingWithCalPulse is only supported in V3 electronics");
        sprintf(regBuf,"checkSbitMappingWithCalPulse is only supported in V3 electronics");
        la->response->set_string("error",regBuf);
        return;
    }

    uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
    if ( (notmask & goodVFATs) != notmask) {
        sprintf(regBuf,"One of the unmasked VFATs is not Synced. goodVFATs: %x\tnotmask: %x",goodVFATs,notmask);
        la->response->set_string("error",regBuf);
        return;
    }

    if (currentPulse && calScaleFactor > 3) {
        sprintf(regBuf,"Bad value for CFG_CAL_FS: %x, Possible values are {0b00, 0b01, 0b10, 0b11}. Exiting.",calScaleFactor);
        la->response->set_string("error",regBuf);
        return;
    }

    //Get current channel register data, mask all channels and disable calpulse
    uint32_t chanRegData_orig[oh::CHANNELS_PER_OH]; //original channel register data
    uint32_t chanRegData_tmp[oh::CHANNELS_PER_OH]; //temporary channel register data
    getChannelRegistersVFAT3Local(la, ohN, mask, chanRegData_orig);
    for (unsigned int idx=0; idx < oh::CHANNELS_PER_OH; ++idx) {
        chanRegData_tmp[idx]=chanRegData_orig[idx]+(0x1 << 14); //set channel mask to true
        chanRegData_tmp[idx]=chanRegData_tmp[idx]-(0x1 << 15); //disable calpulse
    }
    setChannelRegistersVFAT3SimpleLocal(la, ohN, mask, chanRegData_tmp);

    //Setup TTC Generator
    ttcGenConfLocal(la, ohN, 0, 0, pulseDelay, L1Ainterval, nevts, true);
    writeReg(la, "GEM_AMC.TTC.GENERATOR.SINGLE_RESYNC", 0x1);
    writeReg(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_L1A_COUNT", 0x1); //One pulse at a time
    uint32_t addrTtcStart = getAddress(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_START");

    //Set all chips out of run mode
    broadcastWriteLocal(la, ohN, "CFG_RUN", 0x0, mask);

    //Take the VFATs out of slow control only mode
    writeReg(la, "GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

    //Setup the sbit monitor
    const unsigned int nclusters = 8;
    writeReg(la, "GEM_AMC.TRIGGER.SBIT_MONITOR.OH_SELECT", ohN);
    uint32_t addrSbitMonReset=getAddress(la, "GEM_AMC.TRIGGER.SBIT_MONITOR.RESET");
    uint32_t addrSbitCluster[nclusters];
    for (unsigned int iCluster=0; iCluster < nclusters; ++iCluster) {
        sprintf(regBuf, "GEM_AMC.TRIGGER.SBIT_MONITOR.CLUSTER%i",iCluster);
        addrSbitCluster[iCluster] = getAddress(la, regBuf);
    }

    if (!((notmask >> vfatN) & 0x1)) {
        la->response->set_string("error",stdsprintf("The vfat of interest %i should not be part of the vfats to be masked: %x",vfatN, mask));
        return;
    }

    //mask all other vfats from trigger
    writeReg(la,stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CTRL.VFAT_MASK",ohN), 0xffffff & ~(1 << (vfatN)));

    //Place this vfat into run mode
    writeReg(la,stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_RUN",ohN, vfatN), 0x1);

    for (unsigned int chan=0; chan < 128; ++chan) { //Loop over all channels
        //unmask this channel
        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i.MASK",ohN,vfatN,chan), 0x0);

        //Turn on the calpulse for this channel
        if (confCalPulseLocal(la, ohN, ~((0x1)<<vfatN) & 0xFFFFFF, chan, useCalPulse, currentPulse, calScaleFactor) == false) {
            la->response->set_string("error",stdsprintf("Unable to configure calpulse %b for ohN %i mask %x chan %i", useCalPulse, ohN, ~((0x1)<<vfatN) & 0xFFFFFF, chan));
            return; //Calibration pulse is not configured correctly
        }

        //Start Pulsing
        for (unsigned int iPulse=0; iPulse < nevts; ++iPulse) { //Pulse this channel
            //Reset monitors
            writeRawAddress(addrSbitMonReset, 0x1, la->response);

            //Start the TTC Generator
            if (useCalPulse) {
                writeRawAddress(addrTtcStart, 0x1, la->response);
            }

            //Sleep for 200 us + pulseDelay * 25 ns * (0.001 us / ns)
            std::this_thread::sleep_for(std::chrono::microseconds(200+int(ceil(pulseDelay*25*0.001))));

            //Check clusers
            for (unsigned int cluster=0; cluster<nclusters; ++cluster) {
                //int idx = vfatN * posPerVFAT + chan * posPerChan + iPulse * posPerEvt + cluster; //Array index
                unsigned int idx = chan * (nevts*nclusters) + (iPulse*nclusters+cluster);

                //bits [10:0] is the address of the cluster
                //bits [14:12] is the cluster size
                //bits 15 and 11 are not used
                uint32_t thisCluster = readRawAddress(addrSbitCluster[cluster], la->response);
                unsigned int clusterSize = (thisCluster >> 12) & 0x7;
                uint32_t sbitAddress = (thisCluster & 0x7ff);
                bool isValid = (sbitAddress < oh::SBITS_PER_OH); //Possible values are [0,(oh::SBITS_PER_OH)-1]
                int vfatObserved = 7-int(sbitAddress/192)+int((sbitAddress%192)/64)*8;
                int sbitObserved = sbitAddress % 64;

                outData[idx] = ((clusterSize & 0x7 ) << 27) + ((isValid & 0x1) << 26) + ((vfatObserved & 0x1f) << 21) + ((vfatN & 0x1f) << 16) + ((sbitObserved & 0xff) << 8) + (chan & 0xff);

                if (isValid) {
                    LOGGER->log_message(
                            LogManager::INFO,
                            stdsprintf(
                                "valid sbit data: useCalPulse %i; thisClstr %x; clstrSize %x; sbitAddr %x; isValid %x; vfatN %i; vfatObs %i; chan %i; sbitObs %i",
                                useCalPulse, thisCluster, clusterSize, sbitAddress, isValid, vfatN, vfatObserved, chan, sbitObserved));
                }
            } //End Loop over clusters
        } //End Pulses for this channel

        //Turn off the calpulse for this channel
        if (confCalPulseLocal(la, ohN, ~((0x1)<<vfatN) & 0xFFFFFF, chan, false, currentPulse, calScaleFactor) == false) {
            la->response->set_string("error",stdsprintf("Unable to configure calpulse OFF for ohN %i mask %x chan %i", ohN, ~((0x1)<<vfatN) & 0xFFFFFF, chan));
            return; //Calibration pulse is not configured correctly
        }

        //mask this channel
        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i.MASK",ohN,vfatN,chan), 0x1);
    } //End Loop over all channels

    //Place this vfat out of run mode
    writeReg(la,stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_RUN",ohN, vfatN), 0x0);
    //} //End Loop over all VFATs

    //turn off TTC Generator
    ttcGenToggleLocal(la, ohN, false);

    //Return channel register settings to their original values
    setChannelRegistersVFAT3SimpleLocal(la, ohN, mask, chanRegData_orig);

    //Set trigger vfat mask for this OH back to 0
    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CTRL.VFAT_MASK",ohN), 0x0);

    return;
} //End checkSbitMappingWithCalPulseLocal(...)

void checkSbitMappingWithCalPulse(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t vfatN = request->get_word("vfatN");
    uint32_t mask = request->get_word("mask");
    bool useCalPulse = request->get_word("useCalPulse");
    bool currentPulse = request->get_word("currentPulse");
    uint32_t calScaleFactor = request->get_word("calScaleFactor");
    uint32_t nevts = request->get_word("nevts");
    uint32_t L1Ainterval = request->get_word("L1Ainterval");
    uint32_t pulseDelay = request->get_word("pulseDelay");

    uint32_t outData[128*8*nevts];
    checkSbitMappingWithCalPulseLocal(&la, outData, ohN, vfatN, mask, useCalPulse, currentPulse, calScaleFactor, nevts, L1Ainterval, pulseDelay);

    response->set_word_array("data",outData,128*8*nevts);

    rtxn.abort();
} //End checkSbitMappingWithCalPulse()

void checkSbitRateWithCalPulseLocal(localArgs *la, uint32_t *outDataCTP7Rate, uint32_t *outDataFPGAClusterCntRate, uint32_t *outDataVFATSBits, uint32_t ohN, uint32_t vfatN, uint32_t mask, bool useCalPulse, bool currentPulse, uint32_t calScaleFactor, uint32_t waitTime, uint32_t pulseRate, uint32_t pulseDelay)
{
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~mask & 0xFFFFFF;

    char regBuf[200];
    if ( fw_version_check("checkSbitRateWithCalPulse", la) < 3) {
        LOGGER->log_message(LogManager::ERROR, "checkSbitRateWithCalPulse is only supported in V3 electronics");
        sprintf(regBuf,"checkSbitRateWithCalPulse is only supported in V3 electronics");
        la->response->set_string("error",regBuf);
        return;
    }

    uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
    if ( (notmask & goodVFATs) != notmask) {
        sprintf(regBuf,"One of the unmasked VFATs is not Synced. goodVFATs: %x\tnotmask: %x",goodVFATs,notmask);
        la->response->set_string("error",regBuf);
        return;
    }

    if (currentPulse && calScaleFactor > 3) {
        sprintf(regBuf,"Bad value for CFG_CAL_FS: %x, Possible values are {0b00, 0b01, 0b10, 0b11}. Exiting.",calScaleFactor);
        la->response->set_string("error",regBuf);
        return;
    }

    //Get current channel register data, mask all channels and disable calpulse
    LOGGER->log_message(LogManager::INFO, stdsprintf("Storing vfat3 channel registers on ohN %i", ohN));
    uint32_t chanRegData_orig[oh::CHANNELS_PER_OH]; //original channel register data
    uint32_t chanRegData_tmp[oh::CHANNELS_PER_OH]; //temporary channel register data
    LOGGER->log_message(LogManager::INFO, stdsprintf("Masking all channels and disabling calpulse for vfats on ohN %i", ohN));
    getChannelRegistersVFAT3Local(la, ohN, mask, chanRegData_orig);
    for (unsigned int idx=0; idx < oh::CHANNELS_PER_OH; ++idx) {
        chanRegData_tmp[idx]=chanRegData_orig[idx]+(0x1 << 14); //set channel mask to true
        chanRegData_tmp[idx]=chanRegData_tmp[idx]-(0x1 << 15); //disable calpulse
    }
    setChannelRegistersVFAT3SimpleLocal(la, ohN, mask, chanRegData_tmp);

    //Setup TTC Generator
    uint32_t L1Ainterval;
    if (pulseRate > 0) {
        L1Ainterval = int(40079000 / pulseRate);
    }
    else{
        L1Ainterval = 0;
    }
    uint32_t addrTtcReset = getAddress(la, "GEM_AMC.TTC.GENERATOR.RESET");
    uint32_t addrTtcStart = getAddress(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_START");

    //Get Trigger addresses
    uint32_t ohTrigRateAddr[oh::VFATS_PER_OH + 2]; //idx 0->oh::VFATS_PER_OH VFAT counters; idx oh::VFATS_PER_OH+1 - rate measured by OH FPGA; last idx - rate measured by CTP7
    for (unsigned int vfat=0; vfat<oh::VFATS_PER_OH; ++vfat) {
        ohTrigRateAddr[vfat] = getAddress(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.VFAT%i_SBITS",ohN,vfat));
    } //End Loop over all VFATs
    ohTrigRateAddr[oh::VFATS_PER_OH] = getAddress(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.CLUSTER_COUNT",ohN));
    ohTrigRateAddr[oh::VFATS_PER_OH + 1] = getAddress(la, stdsprintf("GEM_AMC.TRIGGER.OH%i.TRIGGER_RATE",ohN));
    uint32_t addTrgCntResetOH = getAddress(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.RESET",ohN));
    uint32_t addTrgCntResetCTP7 = getAddress(la,"GEM_AMC.TRIGGER.CTRL.CNT_RESET");

    //Set all chips out of run mode
    LOGGER->log_message(LogManager::INFO, stdsprintf("Writing CFG_RUN to 0x0 for all VFATs on ohN %i using mask %x",ohN, mask));
    broadcastWriteLocal(la, ohN, "CFG_RUN", 0x0, mask);

    //Take the VFATs out of slow control only mode
    LOGGER->log_message(LogManager::INFO, "Taking VFAT3s out of slow control only mode");
    writeReg(la, "GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);

    //Prep the SBIT counters
    LOGGER->log_message(LogManager::INFO, stdsprintf("Preping SBIT Counters for ohN %i", ohN));
    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.SBIT_CNT_PERSIST",ohN), 0x0); //reset all counters after SBIT_CNT_TIME_MAX
    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CNT.SBIT_CNT_TIME_MAX",ohN), uint32_t(0x02638e98*waitTime/1000.) ); //count for a number of BX's specified by waitTime

    if (!((notmask >> vfatN) & 0x1)) {
        la->response->set_string("error",stdsprintf("The vfat of interest %i should not be part of the vfats to be masked: %x",vfatN, mask));
        return;
    }

    //mask all other vfats from trigger
    LOGGER->log_message(LogManager::INFO, stdsprintf("Masking VFATs %x from trigger in ohN %i", 0xffffff & ~(1 << (vfatN)), ohN));
    writeReg(la,stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CTRL.VFAT_MASK",ohN), 0xffffff & ~(1 << (vfatN)));

    //Place this vfat into run mode
    LOGGER->log_message(LogManager::INFO, stdsprintf("Placing vfatN %i on ohN %i in run mode", vfatN, ohN));
    writeReg(la,stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_RUN",ohN, vfatN), 0x1);

    LOGGER->log_message(LogManager::INFO, stdsprintf("Looping over all channels of vfatN %i on ohN %i", vfatN, ohN));
    for (unsigned int chan=0; chan < 128; ++chan) { //Loop over all channels
        //unmask this channel
        LOGGER->log_message(LogManager::INFO, stdsprintf("Unmasking channel %i on vfat %i of OH %i", chan, vfatN, ohN));
        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i.MASK",ohN,vfatN,chan), 0x0);

        //Turn on the calpulse for this channel
        LOGGER->log_message(LogManager::INFO, stdsprintf("Enabling calpulse for channel %i on vfat %i of OH %i", chan, vfatN, ohN));
        if (confCalPulseLocal(la, ohN, ~((0x1)<<vfatN) & 0xFFFFFF, chan, useCalPulse, currentPulse, calScaleFactor) == false) {
            la->response->set_string("error",stdsprintf("Unable to configure calpulse %b for ohN %i mask %x chan %i", useCalPulse, ohN, ~((0x1)<<vfatN) & 0xFFFFFF, chan));
            return; //Calibration pulse is not configured correctly
        }

        //Reset counters
        LOGGER->log_message(LogManager::INFO, "Reseting trigger counters on OH & CTP7");
        writeRawAddress(addTrgCntResetOH, 0x1, la->response);
        writeRawAddress(addTrgCntResetCTP7, 0x1, la->response);
        //also use daq monitor here...?

        //Start the TTC Generator
        LOGGER->log_message(LogManager::INFO, stdsprintf("Configuring TTC Generator to use OH %i with pulse delay %i and L1Ainterval %i",ohN,pulseDelay,L1Ainterval));
        ttcGenConfLocal(la, ohN, 0, 0, pulseDelay, L1Ainterval, 0, true);
        writeReg(la, "GEM_AMC.TTC.GENERATOR.SINGLE_RESYNC", 0x1);
        writeReg(la, "GEM_AMC.TTC.GENERATOR.CYCLIC_L1A_COUNT", 0x0); //Continue until stopped
        LOGGER->log_message(LogManager::INFO, "Starting TTC Generator");
        writeRawAddress(addrTtcStart, 0x1, la->response);

        //Sleep for waitTime of milliseconds
        std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));

        //Read All Trigger Registers
        LOGGER->log_message(LogManager::INFO, "Reading trigger counters");
        outDataCTP7Rate[chan]=readRawAddress(ohTrigRateAddr[oh::VFATS_PER_OH + 1], la->response);
        outDataFPGAClusterCntRate[chan]=readRawAddress(ohTrigRateAddr[oh::VFATS_PER_OH], la->response)*waitTime/1000.;
        outDataVFATSBits[chan]=readRawAddress(ohTrigRateAddr[vfatN], la->response)*waitTime/1000.;

        //Reset the TTC Generator
        LOGGER->log_message(LogManager::INFO, "Stopping TTC Generator");
        writeRawAddress(addrTtcReset, 0x1, la->response);

        //Turn off the calpulse for this channel
        LOGGER->log_message(LogManager::INFO, stdsprintf("Disabling calpulse for channel %i on vfat %i of OH %i", chan, vfatN, ohN));
        if (confCalPulseLocal(la, ohN, ~((0x1)<<vfatN) & 0xFFFFFF, chan, false, currentPulse, calScaleFactor) == false) {
            la->response->set_string("error",stdsprintf("Unable to configure calpulse OFF for ohN %i mask %x chan %i", ohN, ~((0x1)<<vfatN) & 0xFFFFFF, chan));
            return; //Calibration pulse is not configured correctly
        }

        //mask this channel
        LOGGER->log_message(LogManager::INFO, stdsprintf("Masking channel %i on vfat %i of OH %i", chan, vfatN, ohN));
        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i.MASK",ohN,vfatN,chan), 0x1);
    } //End Loop over all channels

    //Place this vfat out of run mode
    LOGGER->log_message(LogManager::INFO, stdsprintf("Finished looping over all channels.  Taking vfatN %i on ohN %i out of run mode", vfatN, ohN));
    writeReg(la,stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_RUN",ohN, vfatN), 0x0);

    //turn off TTC Generator
    LOGGER->log_message(LogManager::INFO, "Disabling TTC Generator");
    ttcGenToggleLocal(la, ohN, false);

    //Return channel register settings to their original values
    LOGGER->log_message(LogManager::INFO, stdsprintf("Reverting vfat3 channel registers on ohN %i to original values",ohN));
    setChannelRegistersVFAT3SimpleLocal(la, ohN, mask, chanRegData_orig);

    //Set trigger vfat mask for this OH back to 0
    LOGGER->log_message(LogManager::INFO, stdsprintf("Reverting GEM_AMC.OH.OH%i.FPGA.TRIG.CTRL.VFAT_MASK to 0x0", ohN));
    writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.FPGA.TRIG.CTRL.VFAT_MASK",ohN), 0x0);

    return;
} //End checkSbitRateWithCalPulseLocal()

void checkSbitRateWithCalPulse(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t vfatN = request->get_word("vfatN");
    uint32_t mask = request->get_word("mask");
    bool useCalPulse = request->get_word("useCalPulse");
    bool currentPulse = request->get_word("currentPulse");
    uint32_t calScaleFactor = request->get_word("calScaleFactor");
    uint32_t waitTime = request->get_word("waitTime");
    uint32_t pulseRate = request->get_word("pulseRate");
    uint32_t pulseDelay = request->get_word("pulseDelay");

    uint32_t outDataCTP7Rate[128];
    uint32_t outDataFPGAClusterCntRate[128];
    uint32_t outDataVFATSBits[128];
    checkSbitRateWithCalPulseLocal(&la, outDataCTP7Rate, outDataFPGAClusterCntRate, outDataVFATSBits, ohN, vfatN, mask, useCalPulse, currentPulse, calScaleFactor, waitTime, pulseRate, pulseDelay);

    response->set_word_array("outDataCTP7Rate",outDataCTP7Rate,128);
    response->set_word_array("outDataFPGAClusterCntRate",outDataFPGAClusterCntRate,128);
    response->set_word_array("outDataVFATSBits",outDataVFATSBits,128);

    rtxn.abort();
} //End checkSbitRateWithCalPulse()

std::vector<uint32_t> dacScanLocal(localArgs *la, uint32_t ohN, uint32_t dacSelect, uint32_t dacStep, uint32_t mask, bool useExtRefADC)
{
    //Ensure VFAT3 Hardware
    if (fw_version_check("dacScanLocal", la) < 3) {
        LOGGER->log_message(LogManager::ERROR, "dacScanLocal is only supported in V3 electronics");
        la->response->set_string("error","dacScanLocal is only supported in V3 electronics");
        std::vector<uint32_t> emptyVec;
        return emptyVec;
    }

    vfat3DACAndSize dacInfo;
    auto map_dacSelect = dacInfo.map_dacInfo;

    // Check if dacSelect is valid
    if (map_dacSelect.count(dacSelect) == 0) { //Case: dacSelect not found, exit
        std::string errMsg = "Monitoring Select value " + std::to_string(dacSelect) + " not found, possible values are:\n";

        for (auto iterDacSel = map_dacSelect.begin(); iterDacSel != map_dacSelect.end(); ++iterDacSel) {
            errMsg+="\t" + std::to_string((*iterDacSel).first) + "\t" + std::get<0>((*iterDacSel).second) + "\n";
        }
        la->response->set_string("error",errMsg);
        std::vector<uint32_t> emptyVec;
        return emptyVec;
    } //End Case: dacSelect not found, exit

    //Check which VFATs are sync'd
    uint32_t notmask = ~mask & 0xFFFFFF; //Inverse of the vfatmask
    uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
    if ( (notmask & goodVFATs) != notmask) {
        la->response->set_string("error",stdsprintf("One of the unmasked VFATs is not Synced. goodVFATs: %x\tnotmask: %x",goodVFATs,notmask));
        std::vector<uint32_t> emptyVec;
        return emptyVec;
    }

    //Determine the addresses
    std::string regName = std::get<0>(map_dacSelect[dacSelect]);
    LOGGER->log_message(LogManager::INFO, stdsprintf("Scanning DAC: %s",regName.c_str()));
    uint32_t adcAddr[oh::VFATS_PER_OH];
    uint32_t adcCacheUpdateAddr[oh::VFATS_PER_OH];
    bool foundAdcCached = false;
    for (unsigned int vfatN=0; vfatN<oh::VFATS_PER_OH; ++vfatN) {
        //Skip Masked VFATs
        if ( !( (notmask >> vfatN) & 0x1)) continue;

        //Determine Register Base string
        std::string strRegBase = stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.",ohN,vfatN);

        //Get ADC address
        if (useExtRefADC) { //Case: Use ADC with external reference
            //for backward compatibility, use ADC1 instead of ADC1_CACHED if it exists
            if ((foundAdcCached = la->dbi.get(la->rtxn, strRegBase + "ADC1_CACHED"))) {
                adcAddr[vfatN] = getAddress(la, strRegBase + "ADC1_CACHED");
                adcCacheUpdateAddr[vfatN] = getAddress(la, strRegBase + "ADC1_UPDATE");
            }
            else
                adcAddr[vfatN] = getAddress(la, strRegBase + "ADC1");
        } //End Case: Use ADC with external reference
        else{ //Case: Use ADC with internal reference
            //for backward compatibility, use ADC0 instead of ADC0_CACHED if it exists
            if ((foundAdcCached = la->dbi.get(la->rtxn, strRegBase + "ADC0_CACHED"))) {
                adcAddr[vfatN] = getAddress(la, strRegBase + "ADC0_CACHED");
                adcCacheUpdateAddr[vfatN] = getAddress(la, strRegBase + "ADC0_UPDATE");
            }
            else
                adcAddr[vfatN] = getAddress(la, strRegBase + "ADC0");
        } //End Case: Use ADC with internal reference
    } //End Loop over VFATs

    //make the output container and correctly size it
    uint32_t dacMax = std::get<2>(map_dacSelect[dacSelect]);
    uint32_t dacMin = std::get<1>(map_dacSelect[dacSelect]);
    unsigned int nDacValues = (dacMax-dacMin+1)/dacStep;
    std::vector<uint32_t> vec_dacScanData(oh::VFATS_PER_OH*nDacValues); //Each element has bits [0:7] as the current dacValue, and bits [8:17] as the ADC read back value

    //Block L1A's then take VFATs out of run mode
    writeReg(la, "GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x0);
    broadcastWriteLocal(la, ohN, "CFG_RUN", 0x0, mask);

    //Block L1A's then take VFATs out of run mode
    writeReg(la, "GEM_AMC.TTC.CTRL.L1A_ENABLE", 0x0);
    broadcastWriteLocal(la, ohN, "CFG_RUN", 0x0, mask);

    //Configure the DAC Monitoring on all the VFATs
    configureVFAT3DacMonitorLocal(la, ohN, mask, dacSelect);

    //Set the VFATs into Run Mode
    writeReg(la, "GEM_AMC.GEM_SYSTEM.VFAT3.SC_ONLY_MODE", 0x0);
    broadcastWriteLocal(la, ohN, "CFG_RUN", 0x1, mask);
    LOGGER->log_message(LogManager::INFO, stdsprintf("VFATs not in 0x%x were set to run mode", mask));
    std::this_thread::sleep_for(std::chrono::seconds(1)); //I noticed that DAC values behave weirdly immediately after VFAT is placed in run mode (probably voltage/current takes a moment to stabalize)

    //Scan the DAC

    uint32_t nReads=100;
    for (uint32_t dacVal=dacMin; dacVal<=dacMax; dacVal += dacStep) { //Loop over DAC values
        for (unsigned int vfatN=0; vfatN<oh::VFATS_PER_OH; ++vfatN) { //Loop over VFATs
            //Skip masked VFATs
            if ( !( (notmask >> vfatN) & 0x1)) { //Case: VFAT is masked, skip
                //Store word, but with adcVal = 0
                unsigned int idx = vfatN*(dacMax-dacMin+1)/dacStep+(dacVal-dacMin)/dacStep;
                vec_dacScanData[idx] = ((ohN & 0xf) << 23) + ((vfatN & 0x1f) << 18) + (dacVal & 0xff);

                //skip
                continue;
            } //End Case: VFAT is masked, skip
            else{ //Case: VFAT is not masked
            //Set DAC value
                std::string strDacReg = stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.",ohN,vfatN) + regName;
                writeReg(la, strDacReg, dacVal);
                //Read nReads times and take avg value
                uint32_t adcVal=0;
                for (uint32_t i=0; i<nReads; ++i) {
                    //Read the ADC
                    if (foundAdcCached) {
                        //either reading or writing this register will trigger a cache update
                        readRawAddress(adcCacheUpdateAddr[vfatN], la->response);
                        //updating the cache takes 20 us, including a 50% safety factor
                        std::this_thread::sleep_for(std::chrono::microseconds(20));
                    }
                    adcVal += readRawAddress(adcAddr[vfatN], la->response);
                }
                adcVal = adcVal/nReads;
                //Store value
                unsigned int idx = vfatN*(dacMax-dacMin+1)/dacStep+(dacVal-dacMin)/dacStep;
                vec_dacScanData[idx] = ((ohN & 0xf) << 23) + ((vfatN & 0x1f) << 18) + ((adcVal & 0x3ff) << 8) + (dacVal & 0xff);
            } //End Case: VFAT is not masked
        } //End Loop over VFATs
    } //End Loop over DAC values


    //Take the VFATs out of Run Mode
    broadcastWriteLocal(la, ohN, "CFG_RUN", 0x0, mask);

    return vec_dacScanData;
} //End dacScanLocal(...)

void dacScan(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t dacSelect = request->get_word("dacSelect");
    uint32_t dacStep = request->get_word("dacStep");
    uint32_t mask = request->get_word("mask");
    bool useExtRefADC = request->get_word("useExtRefADC");

    std::vector<uint32_t> dacScanResults = dacScanLocal(&la, ohN, dacSelect, dacStep, mask, useExtRefADC);
    response->set_word_array("dacScanResults",dacScanResults);

    rtxn.abort();
} //End dacScan(...)

void dacScanMultiLink(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohMask = request->get_word("ohMask");
    uint32_t dacSelect = request->get_word("dacSelect");
    uint32_t dacStep = request->get_word("dacStep");
    bool useExtRefADC = request->get_word("useExtRefADC");

    unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (request->get_key_exists("NOH")) {
        unsigned int NOH_requested = request->get_word("NOH");
        if (NOH_requested <= NOH)
            NOH = NOH_requested;
        else
            LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register value (%i), NOH request will be disregarded",NOH_requested,NOH));
    }

    vfat3DACAndSize dacInfo;
    std::vector<uint32_t> dacScanResultsAll;
    for (unsigned int ohN=0; ohN<NOH; ++ohN) {
        std::vector<uint32_t> dacScanResults;

        // If this Optohybrid is masked skip it
        if (!((ohMask >> ohN) & 0x1)) {
            int dacMax = std::get<2>(dacInfo.map_dacInfo[dacSelect]);
            dacScanResults.assign( (dacMax+1)*oh::VFATS_PER_OH/dacStep, 0xdeaddead );
            std::copy(dacScanResults.begin(), dacScanResults.end(), std::back_inserter(dacScanResultsAll));
            continue;
        }

        //Get vfatmask for this OH
        LOGGER->log_message(LogManager::INFO, stdsprintf("Getting VFAT Mask for OH%i", ohN));
        uint32_t vfatMask = getOHVFATMaskLocal(&la, ohN);

        //Get dac scan results for this optohybrid
        LOGGER->log_message(LogManager::INFO, stdsprintf("Performing DAC Scan for OH%i", ohN));
        dacScanResults = dacScanLocal(&la, ohN, dacSelect, dacStep, vfatMask, useExtRefADC);

        //Copy the results into the final container
        LOGGER->log_message(LogManager::INFO, stdsprintf("Storing results of DAC scan for OH%i", ohN));
        std::copy(dacScanResults.begin(), dacScanResults.end(), std::back_inserter(dacScanResultsAll));

        LOGGER->log_message(LogManager::INFO, stdsprintf("Finished DAC scan for OH%i", ohN));
    } //End Loop over all Optohybrids

    response->set_word_array("dacScanResultsAll",dacScanResultsAll);
    LOGGER->log_message(LogManager::INFO, stdsprintf("Finished DAC scans for OH Mask 0x%x", ohMask));

    rtxn.abort();
} //End dacScanMultiLink(...)

void genChannelScan(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t nevts = request->get_word("nevts");
    uint32_t ohN = request->get_word("ohN");
    uint32_t mask = request->get_word("mask");
    uint32_t dacMin = request->get_word("dacMin");
    uint32_t dacMax = request->get_word("dacMax");
    uint32_t dacStep = request->get_word("dacStep");
    bool useCalPulse = request->get_word("useCalPulse");
    bool currentPulse = request->get_word("currentPulse");
    uint32_t calScaleFactor = request->get_word("calScaleFactor");
    bool useExtTrig = request->get_word("useExtTrig");
    std::string scanReg = request->get_string("scanReg");

    bool useUltra = false;
    if (request->get_key_exists("useUltra")) {
        useUltra = true;
    }

    uint32_t outData[128*oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep];
    for (uint32_t ch = 0; ch < 128; ch++) {
        genScanLocal(&la, &(outData[ch*oh::VFATS_PER_OH*(dacMax-dacMin+1)/dacStep]), ohN, mask, ch, useCalPulse, currentPulse, calScaleFactor, nevts, dacMin, dacMax, dacStep, scanReg, useUltra, useExtTrig);
    }
    response->set_word_array("data",outData,oh::VFATS_PER_OH*128*(dacMax-dacMin+1)/dacStep);

    rtxn.abort();
}

void confCalPulse(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t mask = request->get_word("mask");
    uint32_t ch = request->get_word("ch");
    bool toggleOn = request->get_word("toggleOn");
    bool currentPulse = request->get_word("currentPulse");
    uint32_t calScaleFactor = request->get_word("calScaleFactor");

    confCalPulseLocal(&la, ohN, mask, ch, toggleOn, currentPulse, calScaleFactor);
    LOGGER->log_message(LogManager::DEBUG, stdsprintf("Finished configuring the calibration pulse."));

    rtxn.abort();
}

extern "C" {
    const char *module_version_key = "calibration_routines v1.0.1";
    int module_activity_color = 4;
    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s", memsvc_get_last_error(memsvc)));
            LOGGER->log_message(LogManager::ERROR, "Unable to load module");
            return; // Do not register our functions, we depend on memsvc.
        }
        modmgr->register_method("calibration_routines", "checkSbitMappingWithCalPulse", checkSbitMappingWithCalPulse);
        modmgr->register_method("calibration_routines", "checkSbitRateWithCalPulse", checkSbitRateWithCalPulse);
        modmgr->register_method("calibration_routines", "dacScan", dacScan);
        modmgr->register_method("calibration_routines", "dacScanMultiLink", dacScanMultiLink);
        modmgr->register_method("calibration_routines", "genScan", genScan);
        modmgr->register_method("calibration_routines", "genChannelScan", genChannelScan);
        modmgr->register_method("calibration_routines", "sbitRateScan", sbitRateScan);
        modmgr->register_method("calibration_routines", "ttcGenConf", ttcGenConf);
        modmgr->register_method("calibration_routines", "ttcGenToggle", ttcGenToggle);
        modmgr->register_method("calibration_routines", "confCalPulse", confCalPulse);
    }
}
