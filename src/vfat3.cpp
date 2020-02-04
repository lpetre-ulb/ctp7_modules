/*! \file vfat3.cpp
 *  \brief RPC module for VFAT3 methods
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#include "vfat3.h"
#include <algorithm>
#include <chrono>
#include "optohybrid.h"
#include <thread>
#include "amc.h"
#include "reedmuller.h"
#include <iomanip>
#include <memory>
#include "hw_constants.h"

uint32_t vfatSyncCheckLocal(localArgs * la, uint32_t ohN)
{
    uint32_t goodVFATs = 0;
    for(unsigned int vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++)
    {
        char regBase [100];
        sprintf(regBase, "GEM_AMC.OH_LINKS.OH%i.VFAT%i",ohN, vfatN);
        bool linkGood = readReg(la, std::string(regBase)+".LINK_GOOD");
        uint32_t linkErrors = readReg(la, std::string(regBase)+".SYNC_ERR_CNT");
        goodVFATs = goodVFATs | ((linkGood && (linkErrors == 0)) << vfatN);
    }

    return goodVFATs;
}

void vfatSyncCheck(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");

    uint32_t goodVFATs = vfatSyncCheckLocal(&la, ohN);

    response->set_word("goodVFATs", goodVFATs);

    rtxn.abort();
}

void configureVFAT3DacMonitorLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t dacSelect){
    //Check if VFATs are sync'd
    uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
    uint32_t notmask = ~mask & 0xFFFFFF;
    if( (notmask & goodVFATs) != notmask)
    {
        la->response->set_string("error",stdsprintf("One of the unmasked VFATs is not Synced. goodVFATs: %x\tnotmask: %x",goodVFATs,notmask));
        return;
    }

    //Get ref voltage and monitor gain
    //These should have been set at time of configure
    uint32_t adcVRefValues[oh::VFATS_PER_OH];
    uint32_t monitorGainValues[oh::VFATS_PER_OH];
    broadcastReadLocal(la, adcVRefValues, ohN, "CFG_VREF_ADC", mask);
    broadcastReadLocal(la, monitorGainValues, ohN, "CFG_MON_GAIN", mask);

    //Loop over all vfats and set the dacSelect
    for(unsigned int vfatN=0; vfatN<oh::VFATS_PER_OH; ++vfatN){
        // Check if vfat is masked
        if(!((notmask >> vfatN) & 0x1)){
            continue;
        } //End check if VFAT is masked

        //Build global control 4 register
        uint32_t glbCtr4 = (adcVRefValues[vfatN] << 8) + (monitorGainValues[vfatN] << 7) + dacSelect;
        writeReg(la, stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.CFG_4",ohN,vfatN), glbCtr4);
    } //End loop over all VFATs

    return;
} //End configureVFAT3DacMonitorLocal(...)

void configureVFAT3DacMonitor(const RPCMsg *request, RPCMsg *response){
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t vfatMask = request->get_word("vfatMask");
    uint32_t dacSelect = request->get_word("dacSelect");

    LOGGER->log_message(LogManager::INFO, stdsprintf("Programming VFAT3 ADC Monitoring for Selection %i",dacSelect));
    configureVFAT3DacMonitorLocal(&la, ohN, vfatMask, dacSelect);

    rtxn.abort();
} //End configureVFAT3DacMonitor()

void configureVFAT3DacMonitorMultiLink(const RPCMsg *request, RPCMsg *response){
    GETLOCALARGS(response);

    uint32_t ohMask = request->get_word("ohMask");
    uint32_t dacSelect = request->get_word("dacSelect");

    unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (request->get_key_exists("NOH")){
        unsigned int NOH_requested = request->get_word("NOH");
        if (NOH_requested <= NOH)
            NOH = NOH_requested;
        else
            LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register value (%i), NOH request will be disregarded",NOH_requested,NOH));
    }
    for(unsigned int ohN=0; ohN<NOH; ++ohN){
        // If this Optohybrid is masked skip it
        if(!((ohMask >> ohN) & 0x1)){
            continue;
        }

        //Get VFAT Mask
        uint32_t vfatMask = getOHVFATMaskLocal(&la, ohN);

        LOGGER->log_message(LogManager::INFO, stdsprintf("Programming VFAT3 ADC Monitoring on OH%i for Selection %i",ohN,dacSelect));
        configureVFAT3DacMonitorLocal(&la, ohN, vfatMask, dacSelect);
    } //End Loop over all Optohybrids

    rtxn.abort();
} //End configureVFAT3DacMonitorMultiLink()

void configureVFAT3sLocal(localArgs * la, uint32_t ohN, uint32_t vfatMask) {
    std::string line, regName;
    uint32_t dacVal;
    std::string dacName;
    uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
    uint32_t notmask = ~vfatMask & 0xFFFFFF;
    if( (notmask & goodVFATs) != notmask)
    {
        char errBuf[200];
        sprintf(errBuf,"One of the unmasked VFATs is not Synced. goodVFATs: %x\tnotmask: %x",goodVFATs,notmask);
        la->response->set_string("error",errBuf);
        return;
    }

    LOGGER->log_message(LogManager::INFO, "Load configuration settings");
    for(uint32_t vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++) if((notmask >> vfatN) & 0x1)
    {
        std::string configFileBase = "/mnt/persistent/gemdaq/vfat3/config_OH"+std::to_string(ohN)+"_VFAT"+std::to_string(vfatN)+".txt";
        std::ifstream infile(configFileBase);
        if(!infile.is_open())
        {
            LOGGER->log_message(LogManager::ERROR, "could not open config file "+configFileBase);
            la->response->set_string("error", "could not open config file "+configFileBase);
            return;
        }
        std::getline(infile,line);// skip first line
        while (std::getline(infile,line))
        {
            std::string reg_basename = "GEM_AMC.OH.OH" + std::to_string(ohN) + ".GEB.VFAT"+std::to_string(vfatN)+".CFG_";
            std::stringstream iss(line);
            if (!(iss >> dacName >> dacVal)) {
                LOGGER->log_message(LogManager::ERROR, "ERROR READING SETTINGS");
                la->response->set_string("error", "Error reading settings");
                break;
            }
            else
            {
                regName = reg_basename + dacName;
                writeReg(la, regName, dacVal);
            }
        }
    }
}

void configureVFAT3s(const RPCMsg *request, RPCMsg *response) {
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t vfatMask = request->get_word("vfatMask");

    configureVFAT3sLocal(&la, ohN, vfatMask);
    rtxn.abort();
}

void getChannelRegistersVFAT3(const RPCMsg *request, RPCMsg *response){
    LOGGER->log_message(LogManager::INFO, "Getting VFAT3 Channel Registers");

    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t vfatMask = request->get_word("vfatMask");

    uint32_t chanRegData[oh::VFATS_PER_OH*128];

    getChannelRegistersVFAT3Local(&la, ohN, vfatMask, chanRegData);

    response->set_word_array("chanRegData",chanRegData,oh::VFATS_PER_OH*128);

    rtxn.abort();
} //End getChannelRegistersVFAT3()

void getChannelRegistersVFAT3Local(localArgs *la, uint32_t ohN, uint32_t vfatMask, uint32_t *chanRegData){
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~vfatMask & 0xFFFFFF;

    char regBuf[200];
    LOGGER->log_message(LogManager::INFO, "Read channel register settings");
    for(unsigned int vfatN=0; vfatN < oh::VFATS_PER_OH; ++vfatN){
        // Check if vfat is masked
        if(!((notmask >> vfatN) & 0x1)){
            continue;
        } //End check if VFAT is masked

        // Check if vfatN is sync'd
        uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
        if( !( (goodVFATs >> vfatN ) & 0x1 ) ){
            sprintf(regBuf,"The requested VFAT is not synced; goodVFATs: %x\t requested VFAT: %i; maskOh: %x", goodVFATs, vfatN, vfatMask);
            la->response->set_string("error",regBuf);
            return;
        }

        //Loop over the channels
        uint32_t chanAddr;
        for(unsigned int chan=0; chan < 128; ++chan){
            //Deterime the idx
            unsigned int idx = vfatN*128 + chan;

            //Get the address
            sprintf(regBuf,"GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i",ohN,vfatN,chan);
            chanAddr = getAddress(la, regBuf);

            //Build the channel register
            LOGGER->log_message(LogManager::DEBUG, stdsprintf("Reading channel register for VFAT%i chan %i",vfatN,chan));
            chanRegData[idx] = readRawAddress(chanAddr, la->response);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        } //End Loop over channels
    } //End Loop over VFATs

    return;
} //end getChannelRegistersVFAT3Local()

void readVFAT3ADCLocal(localArgs * la, uint32_t * outData, uint32_t ohN, bool useExtRefADC, uint32_t mask){
    if(useExtRefADC){ //Case: Use ADC with external reference
        broadcastReadLocal(la, outData, ohN, "ADC1_UPDATE", mask);
        std::this_thread::sleep_for(std::chrono::microseconds(20));
        broadcastReadLocal(la, outData, ohN, "ADC1_CACHED", mask);
    } //End Case: Use ADC with external reference
    else{ //Case: Use ADC with internal reference
        broadcastReadLocal(la, outData, ohN, "ADC0_UPDATE", mask);
        std::this_thread::sleep_for(std::chrono::microseconds(20));
        broadcastReadLocal(la, outData, ohN, "ADC0_CACHED", mask);
    } //End Case: Use ADC with internal reference

    return;
} //End readVFAT3ADCLocal

void readVFAT3ADC(const RPCMsg *request, RPCMsg *response){
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    bool useExtRefADC = request->get_word("useExtRefADC");
    uint32_t vfatMask = request->get_word("vfatMask");

    uint32_t adcData[oh::VFATS_PER_OH];

    LOGGER->log_message(LogManager::INFO, stdsprintf("Reading VFAT3 ADC's for OH%i with mask %x",ohN, vfatMask));
    readVFAT3ADCLocal(&la, adcData, ohN, useExtRefADC, vfatMask);

    response->set_word_array("adcData",adcData,oh::VFATS_PER_OH);

    rtxn.abort();
} //End getChannelRegistersVFAT3()

void readVFAT3ADCMultiLink(const RPCMsg *request, RPCMsg *response){
    GETLOCALARGS(response);

    uint32_t ohMask = request->get_word("ohMask");
    bool useExtRefADC = request->get_word("useExtRefADC");

    unsigned int NOH = readReg(&la, "GEM_AMC.GEM_SYSTEM.CONFIG.NUM_OF_OH");
    if (request->get_key_exists("NOH")){
        unsigned int NOH_requested = request->get_word("NOH");
        if (NOH_requested <= NOH)
            NOH = NOH_requested;
        else
            LOGGER->log_message(LogManager::WARNING, stdsprintf("NOH requested (%i) > NUM_OF_OH AMC register value (%i), NOH request will be disregarded",NOH_requested,NOH));
    }
    uint32_t adcData[oh::VFATS_PER_OH] = {0};
    uint32_t adcDataAll[amc::OH_PER_AMC*oh::VFATS_PER_OH] = {0};
    for(unsigned int ohN=0; ohN<NOH; ++ohN){
        // If this Optohybrid is masked skip it
        if(!((ohMask >> ohN) & 0x1)){
            continue;
        }

        LOGGER->log_message(LogManager::INFO, stdsprintf("Reading VFAT3 ADC Values for all chips on OH%i",ohN));

        //Get VFAT Mask
        uint32_t vfatMask = getOHVFATMaskLocal(&la, ohN);

        //Get all ADC values
        readVFAT3ADCLocal(&la, adcData, ohN, useExtRefADC, vfatMask);

        //Copy all ADC values
        std::copy(adcData, adcData+oh::VFATS_PER_OH, adcDataAll+(oh::VFATS_PER_OH*ohN));
    } //End Loop over all Optohybrids

    response->set_word_array("adcDataAll",adcDataAll,amc::OH_PER_AMC*oh::VFATS_PER_OH);

    rtxn.abort();
} //End readVFAT3ADCMultiLink()

void setChannelRegistersVFAT3SimpleLocal(localArgs * la, uint32_t ohN, uint32_t vfatMask, uint32_t *chanRegData){
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~vfatMask & 0xFFFFFF;

    char regBuf[200];
    LOGGER->log_message(LogManager::INFO, "Write channel register settings");
    for(unsigned int vfatN=0; vfatN < oh::VFATS_PER_OH; ++vfatN){
        // Check if vfat is masked
        if(!((notmask >> vfatN) & 0x1)){
            continue;
        } //End check if VFAT is masked

        // Check if vfatN is sync'd
        uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
        if( !( (goodVFATs >> vfatN ) & 0x1 ) ){
            sprintf(regBuf,"The requested VFAT is not synced; goodVFATs: %x\t requested VFAT: %i; maskOh: %x", goodVFATs, vfatN, vfatMask);
            la->response->set_string("error",regBuf);
            return;
        }

        //Loop over the channels
        uint32_t chanAddr;
        for(unsigned int chan=0; chan < 128; ++chan){
            //Deterime the idx
            unsigned int idx = vfatN*128 + chan;

            //Get the address
            sprintf(regBuf,"GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i",ohN,vfatN,chan);
            chanAddr = getAddress(la, regBuf);
            writeRawAddress(chanAddr, chanRegData[idx], la->response);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        } //End Loop over channels
    } //End Loop over VFATs

    return;
} //End setChannelRegistersVFAT3SimpleLocal()

void setChannelRegistersVFAT3Local(localArgs * la, uint32_t ohN, uint32_t vfatMask, uint32_t *calEnable, uint32_t *masks, uint32_t *trimARM, uint32_t *trimARMPol, uint32_t *trimZCC, uint32_t *trimZCCPol){
    //Determine the inverse of the vfatmask
    uint32_t notmask = ~vfatMask & 0xFFFFFF;

    char regBuf[200];
    LOGGER->log_message(LogManager::INFO, "Write channel register settings");
    for(unsigned int vfatN=0; vfatN < oh::VFATS_PER_OH; ++vfatN){
        // Check if vfat is masked
        if(!((notmask >> vfatN) & 0x1)){
            continue;
        } //End check if VFAT is masked

        // Check if vfatN is sync'd
        uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
        if( !( (goodVFATs >> vfatN ) & 0x1 ) ){
            sprintf(regBuf,"The requested VFAT is not synced; goodVFATs: %x\t requested VFAT: %i; maskOh: %x", goodVFATs, vfatN, vfatMask);
            la->response->set_string("error",regBuf);
            return;
        }

        //Loop over the channels
        uint32_t chanAddr;
        uint32_t chanRegVal;
        for(unsigned int chan=0; chan < 128; ++chan){
            //Deterime the idx
            unsigned int idx = vfatN*128 + chan;

            //Get the address
            sprintf(regBuf,"GEM_AMC.OH.OH%i.GEB.VFAT%i.VFAT_CHANNELS.CHANNEL%i",ohN,vfatN,chan);
            chanAddr = getAddress(la, regBuf);

            //Check trim values make sense
            if ( trimARM[idx] > 0x3F || trimARM[idx] < 0x0){
                sprintf(regBuf,"arming comparator trim value must be positive in range [0x0,0x3F]. Value given for VFAT%i chan %i: %x",vfatN,chan,trimARM[idx]);
                la->response->set_string("error",regBuf);
                return;
            }
            if ( trimZCC[idx] > 0x3F || trimZCC[idx] < 0x0){
                sprintf(regBuf,"zero crossing comparator trim value must be positive in range [0x0,0x3F]. Value given for VFAT%i chan %i: %x",vfatN,chan,trimZCC[idx]);
                la->response->set_string("error",regBuf);
                return;
            }

            //Build the channel register
            LOGGER->log_message(LogManager::INFO, stdsprintf("Setting channel register for VFAT%i chan %i",vfatN,chan));
            chanRegVal = (calEnable[idx] << 15) + (masks[idx] << 14) + \
                         (trimZCCPol[idx] << 13) + (trimZCC[idx] << 7) + \
                         (trimARMPol[idx] << 6) + (trimARM[idx]);
            writeRawAddress(chanAddr, chanRegVal, la->response);
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        } //End Loop over channels
    } //End Loop over VFATs

    return;
} //end setChannelRegistersVFAT3Local()

void setChannelRegistersVFAT3(const RPCMsg *request, RPCMsg *response){
    LOGGER->log_message(LogManager::INFO, "Setting VFAT3 Channel Registers");

    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    uint32_t vfatMask = request->get_word("vfatMask");

    if (request->get_key_exists("simple")){
        uint32_t chanRegData[3072];

        request->get_word_array("chanRegData",chanRegData);

        setChannelRegistersVFAT3SimpleLocal(&la, ohN, vfatMask, chanRegData);
    } //End Case: user provided a single array
    else{ //Case: user provided multiple arrays
        uint32_t calEnable[3072];
        uint32_t masks[3072];
        uint32_t trimARM[3072];
        uint32_t trimARMPol[3072];
        uint32_t trimZCC[3072];
        uint32_t trimZCCPol[3072];

        request->get_word_array("calEnable",calEnable);
        request->get_word_array("masks",masks);
        request->get_word_array("trimARM",trimARM);
        request->get_word_array("trimARMPol",trimARMPol);
        request->get_word_array("trimZCC",trimZCC);
        request->get_word_array("trimZCCPol",trimZCCPol);

        setChannelRegistersVFAT3Local(&la, ohN, vfatMask, calEnable, masks, trimARM, trimARMPol, trimZCC, trimZCCPol);
    } //End Case: user provided multiple arrays

    rtxn.abort();
} //End setChannelRegistersVFAT3()

void statusVFAT3sLocal(localArgs * la, uint32_t ohN)
{
    std::string regs [] = {"CFG_PULSE_STRETCH",
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
                           "CFG_RUN"};
    std::string regName;

    for(unsigned int vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++)
    {
        char regBase [100];
        sprintf(regBase, "GEM_AMC.OH.OH%i.GEB.VFAT%i.",ohN, vfatN);
        for (auto &reg : regs) {
            regName = std::string(regBase)+reg;
            la->response->set_word(regName,readReg(la,regName));
        }
    }
}

void statusVFAT3s(const RPCMsg *request, RPCMsg *response)
{
    GETLOCALARGS(response);

    uint32_t ohN = request->get_word("ohN");
    LOGGER->log_message(LogManager::INFO, "Reading VFAT3 status...");

    statusVFAT3sLocal(&la, ohN);

    LOGGER->log_message(LogManager::INFO, "Done VFAT3 status");
    rtxn.abort();
}

uint16_t decodeChipID(uint32_t encChipID)
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
    throw std::range_error(errmsg.str());
  }

  for (int j=0; j < rm->n; ++j)
    encoded.get()[(rm->n-j-1)] = (encChipID>>j) &0x1;

  int result = reedmuller_decode(rm, encoded.get(), decoded.get());

  if (result) {
    uint16_t decChipID = 0x0;

    char tmp_decoded[1024];
    char* dp = tmp_decoded;

    for (int j=0; j < rm->k; ++j)
      dp += sprintf(dp,"%d", decoded.get()[j]);

    char *p;
    errno = 0;

    uint32_t conv = strtoul(tmp_decoded, &p, 2);
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

void getVFAT3ChipIDsLocal(localArgs * la, uint32_t ohN, uint32_t vfatMask, bool rawID)
{
  //Determine the inverse of the vfatmask
  uint32_t notmask = ~vfatMask & 0xFFFFFF;

  //Check if VFATs are sync'd
  uint32_t goodVFATs = vfatSyncCheckLocal(la, ohN);
  if( (notmask & goodVFATs) != notmask)
  {
      char errBuf[200];
      sprintf(errBuf,"One of the unmasked VFATs is not Synced. goodVFATs: %x\tnotmask: %x",goodVFATs,notmask);
      la->response->set_string("error",errBuf);
      return;
  }

  std::string regName;

  for(unsigned int vfatN = 0; vfatN < oh::VFATS_PER_OH; vfatN++) {
    char regBase [100];
    sprintf(regBase, "GEM_AMC.OH.OH%i.GEB.VFAT%i.HW_CHIP_ID",ohN, vfatN);

    regName = std::string(regBase);
    // Check if vfat is masked
    if(!((notmask >> vfatN) & 0x1)){
        la->response->set_word(regName,0xdeaddead);
        continue;
    } //End check if VFAT is masked

    uint32_t id = readReg(la,regName);
    uint16_t decChipID = 0x0;
    try {
      decChipID = decodeChipID(id);
      std::stringstream msg;
      msg << "OH" << ohN << "::VFAT" << vfatN << ": chipID is:"
          << std::hex<<std::setw(8)<<std::setfill('0')<<id<<std::dec
          <<"(raw) or "
          << std::hex<<std::setw(8)<<std::setfill('0')<<decChipID<<std::dec
          << "(decoded)";
      LOGGER->log_message(LogManager::INFO, msg.str());

      if (rawID)
        la->response->set_word(regName,id);
      else
        la->response->set_word(regName,decChipID);
    } catch (std::runtime_error& e) {
      std::stringstream errmsg;
      errmsg << "Error decoding chipID: " << e.what()
             << ", returning raw chipID";
      LOGGER->log_message(LogManager::ERROR,errmsg.str());
      la->response->set_word(regName,id);
    }
  }
}

void getVFAT3ChipIDs(const RPCMsg *request, RPCMsg *response)
{
  // struct localArgs la = getLocalArgs(response);
  GETLOCALARGS(response);

  uint32_t ohN      = request->get_word("ohN");
  uint32_t vfatMask = request->get_word("vfatMask");
  bool rawID        = request->get_word("rawID");
  LOGGER->log_message(LogManager::DEBUG, "Reading VFAT3 chipIDs");

  getVFAT3ChipIDsLocal(&la, ohN, vfatMask, rawID);

  rtxn.abort();
}

void readDACValues(const RPCMsg *request, RPCMsg *response)
{
  GETLOCALARGS(response);

  uint32_t ohN      = request->get_word("ohN");
  uint32_t vfatMask = request->get_word("vfatMask");

  LOGGER->log_message(LogManager::DEBUG, "Reading VFAT3 DAC values");

  std::array<std::pair<uint32_t, std::string>, 27> dacNames = {
    std::make_pair<uint32_t, std::string>(0, "CFG_IREF"),
    std::make_pair<uint32_t, std::string>(1, "CFG_CAL_DAC"),
    std::make_pair<uint32_t, std::string>(2, "CFG_BIAS_PRE_I_BIT"),
    std::make_pair<uint32_t, std::string>(3, "CFG_BIAS_PRE_I_BLCC"),
    std::make_pair<uint32_t, std::string>(4, "CFG_BIAS_PRE_I_BSF"),
    std::make_pair<uint32_t, std::string>(5, "CFG_BIAS_SH_I_BFCAS"),
    std::make_pair<uint32_t, std::string>(6, "CFG_BIAS_SH_I_BDIFF"),
    std::make_pair<uint32_t, std::string>(7, "CFG_BIAS_SD_I_BDIFF"),
    std::make_pair<uint32_t, std::string>(8, "CFG_BIAS_SD_I_BFCAS"),
    std::make_pair<uint32_t, std::string>(9, "CFG_BIAS_SD_I_BSF"),
    std::make_pair<uint32_t, std::string>(10, "CFG_BIAS_CFD_DAC_1"),
    std::make_pair<uint32_t, std::string>(11, "CFG_BIAS_CFD_DAC_2"),
    std::make_pair<uint32_t, std::string>(12, "CFG_HYST"),
    std::make_pair<uint32_t, std::string>(13, "CFG_CFD_IREF_LOCAL"),
    std::make_pair<uint32_t, std::string>(14, "CFG_THR_ARM_DACI"),
    std::make_pair<uint32_t, std::string>(15, "CFG_THR_ZCC_DACI"),
    std::make_pair<uint32_t, std::string>(16, "CFG_SLVS_BIAS"),
    // std::make_pair<uint32_t, std::string>(17, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(18, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(19, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(20, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(21, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(22, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(23, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(24, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(25, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(26, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(27, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(28, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(29, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(10, "CFG_THR_ARM_DAC"),
    // std::make_pair<uint32_t, std::string>(31, "CFG_THR_ARM_DAC"),
    std::make_pair<uint32_t, std::string>(32, "CFG_VBG"),
    std::make_pair<uint32_t, std::string>(33, "CFG_CAL_DAC"),
    std::make_pair<uint32_t, std::string>(34, "CFG_BIAS_PRE_VREF"),
    std::make_pair<uint32_t, std::string>(35, "CFG_THR_ARM_DACV"),
    std::make_pair<uint32_t, std::string>(36, "CFG_THR_ZCC_DACV"),
    std::make_pair<uint32_t, std::string>(37, "CFG_VTEMP_INT"),
    std::make_pair<uint32_t, std::string>(38, "CFG_VTEMP_EXT"),
    std::make_pair<uint32_t, std::string>(39, "CFG_VREF_ADC"),
    std::make_pair<uint32_t, std::string>(40, "CFG_VIN_ADC"),
    std::make_pair<uint32_t, std::string>(41, "CFG_SLVS_VREF")
  };

  uint32_t adcAddr[oh::VFATS_PER_OH];
  uint32_t adcCacheUpdateAddr[oh::VFATS_PER_OH];
  bool useExtRefADC   = false;
  bool foundAdcCached = false;

  uint32_t notmask = ~vfatMask & 0xFFFFFF;

  for (size_t vfatN = 0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
    //                                   GEM_AMC.OH.OHYY.GEB.VFATXX.ADC1_CACHED
    //                                   GEM_AMC.OH.OHYY.GEB.VFATXX.ADC0_UPDATE
    std::string strRegBase = stdsprintf("GEM_AMC.OH.OH%i.GEB.VFAT%i.",ohN,vfatN);

    if (useExtRefADC) {
      if ((foundAdcCached = la.dbi.get(la.rtxn, strRegBase + "ADC1_CACHED"))) {
        adcAddr[vfatN]            = getAddress(&la, strRegBase + "ADC1_CACHED");
        adcCacheUpdateAddr[vfatN] = getAddress(&la, strRegBase + "ADC1_UPDATE");
      } else {
        adcAddr[vfatN] = getAddress(&la, strRegBase + "ADC1");
      }
    } else {
      if ((foundAdcCached = la.dbi.get(la.rtxn, strRegBase + "ADC0_CACHED"))) {
        adcAddr[vfatN]            = getAddress(&la, strRegBase + "ADC0_CACHED");
        adcCacheUpdateAddr[vfatN] = getAddress(&la, strRegBase + "ADC0_UPDATE");
      } else {
        adcAddr[vfatN] = getAddress(&la, strRegBase + "ADC0");
      }
    }
  }

  for (auto const& dac : dacNames) {
    LOGGER->log_message(LogManager::INFO, "Reading back DAC "+dac.second);
    configureVFAT3DacMonitorLocal(&la, ohN, vfatMask, dac.first);
    std::this_thread::sleep_for(std::chrono::seconds(1));

    for (size_t vfatN=0; vfatN < oh::VFATS_PER_OH; ++vfatN) {
      if (!((notmask >> vfatN) & 0x1)) {
        std::stringstream msg;
        msg << "Skipping VFAT" << vfatN;
        LOGGER->log_message(LogManager::INFO, msg.str());
        continue;
      }
      uint32_t val = 0;
      for (size_t c = 0; c < 100; ++c) {
        if (foundAdcCached) {
          readRawAddress(adcCacheUpdateAddr[vfatN], la.response);
          std::this_thread::sleep_for(std::chrono::microseconds(30));
        }
        val += readRawAddress(adcAddr[vfatN], la.response);
      }
      val = val/100;
      std::stringstream regName;
      regName << "OH" << ohN << ".VFAT" << vfatN << "." << dac.second;
      la.response->set_word(regName.str(), val);
    }
  }

  rtxn.abort();
}

extern "C" {
    const char *module_version_key = "vfat3 v1.0.1";
    int module_activity_color = 4;
    void module_init(ModuleManager *modmgr) {
        if (memhub_open(&memsvc) != 0) {
            LOGGER->log_message(LogManager::ERROR, stdsprintf("Unable to connect to memory service: %s", memsvc_get_last_error(memsvc)));
            LOGGER->log_message(LogManager::ERROR, "Unable to load module");
            return; // Do not register our functions, we depend on memsvc.
        }
        modmgr->register_method("vfat3", "configureVFAT3s", configureVFAT3s);
        modmgr->register_method("vfat3", "configureVFAT3DacMonitor", configureVFAT3DacMonitor);
        modmgr->register_method("vfat3", "configureVFAT3DacMonitorMultiLink", configureVFAT3DacMonitorMultiLink);
        modmgr->register_method("vfat3", "getChannelRegistersVFAT3", getChannelRegistersVFAT3);
        modmgr->register_method("vfat3", "getVFAT3ChipIDs", getVFAT3ChipIDs);
        modmgr->register_method("vfat3", "readVFAT3ADC", readVFAT3ADC);
        modmgr->register_method("vfat3", "readDACValues", readDACValues);
        modmgr->register_method("vfat3", "readVFAT3ADCMultiLink", readVFAT3ADCMultiLink);
        modmgr->register_method("vfat3", "setChannelRegistersVFAT3", setChannelRegistersVFAT3);
        modmgr->register_method("vfat3", "statusVFAT3s", statusVFAT3s);
        modmgr->register_method("vfat3", "vfatSyncCheck", vfatSyncCheck);
    }
}
