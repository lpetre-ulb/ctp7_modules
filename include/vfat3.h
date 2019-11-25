/*! \file vfat3.h
 *  \brief RPC module for VFAT3 methods
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#ifndef VFAT3_H
#define VFAT3_H

#include "utils.h"
#include <string>

/*! \fn uint32_t vfatSyncCheckLocal(localArgs * la, uint32_t ohN)
 *  \brief Local callable version of vfatSyncCheck
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \return Bitmask of sync'ed VFATs
 */
uint32_t vfatSyncCheckLocal(localArgs * la, uint32_t ohN);

/*! \fn void vfatSyncCheck(const RPCMsg *request, RPCMsg *response)
 *  \brief Returns a list of synchronized VFAT chips
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void vfatSyncCheck(const RPCMsg *request, RPCMsg *response);

/*! \fn void configureVFAT3DacMonitorLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t dacSelect)
 *  \brief configures the VFAT3s on optohybrid ohN to use their ADCs to monitor the DAC provided by dacSelect.
 *  \param la Local arguments structure
 *  \param ohN Optical link
 *  \param mask VFAT mask
 *  \param dacSelect the monitoring selection for the VFAT3 ADC, possible values are [0,16] and [32,41].  See VFAT3 manual for details
 */
void configureVFAT3DacMonitorLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t dacSelect);

/*! \fn void configureVFAT3DacMonitor(const RPCMsg *request, RPCMsg *response)
 *  \brief Allows the host machine to configure the VFAT3s on optohybrid ohN to use their ADCs to monitor a given DAC
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void configureVFAT3DacMonitor(const RPCMsg *request, RPCMsg *response);

/*! \fn void configureVFAT3DacMonitorMultiLink(const RPCMsg *request, RPCMsg *response)
 *  \brief As configureVFAT3DacMonitor(...) but for all optical links specified in ohMask on the AMC
 *  \details Here the RPCMsg request should have a "ohMask" word which specifies which OH's to read from, this is a 12 bit number where a 1 in the n^th bit indicates that the n^th OH should be read back.  Additionally there should be a "ohVfatMaskArray" which is an array of size 12 where each element is the standard vfatMask for OH specified by the array index.
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void configureVFAT3DacMonitorMultiLink(const RPCMsg *request, RPCMsg *response);

/*! \fn void configureVFAT3sLocal(localArgs * la, uint32_t ohN, uint32_t vfatMask)
 *  \brief Local callable version of configureVFAT3s
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 *  \param vfatMask Bitmask of chip positions determining which chips to use
 */
void configureVFAT3sLocal(localArgs * la, uint32_t ohN, uint32_t vfatMask);

/*! \fn void configureVFAT3s(const RPCMsg *request, RPCMsg *response)
 *  \brief Configures VFAT3 chips
 *
 *  VFAT configurations are sored in files under /mnt/persistent/gemdaq/vfat3/config_OHX_VFATY.txt. Has to be updated later.
 *
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void configureVFAT3s(const RPCMsg *request, RPCMsg *response);

/*! \fn void getChannelRegistersVFAT3Local(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t *chanRegData)
 *  \brief reads all channel registers for unmasked vfats and stores values in chanRegData
 *  \param la Local arguments structure
 *  \param ohN Optical link
 *  \param mask VFAT mask
 *  \param chanRegData pointer to the container holding channel registers; expected to be an array of 3072 channels with idx = vfatN * 128 + chan
 */
void getChannelRegistersVFAT3Local(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t *chanRegData);

/*! \fn void setChannelRegistersVFAT3(const RPCMsg *request, RPCMsg *response);
 *  \brief reads all vfat3 channel registers from host machine
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void getChannelRegistersVFAT3(const RPCMsg *request, RPCMsg *response);

/*! \fn void readVFAT3ADCLocal(localArgs * la, uint32_t * outData, uint32_t ohN, bool useExtRefADC=false, uint32_t mask=0xFF000000)
 *  \brief reads the ADC of all unmasked VFATs
 *  \param la Local arguments structure
 *  \param outData pointer to the array containing the ADC results
 *  \param ohN Optical link
 *  \param useExtRefADC true (false) read the ADC1 (ADC0) which uses an external (internal) reference
 *  \param mask VFAT mask
 */
void readVFAT3ADCLocal(localArgs * la, uint32_t * outData, uint32_t ohN, bool useExtRefADC=false, uint32_t mask=0xFF000000);

/*! \fn readVFAT3ADC(const RPCMsg *request, RPCMsg *response)
 *  \brief Allows the host machine to read the ADC value from all unmasked VFATs
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void readVFAT3ADC(const RPCMsg *request, RPCMsg *response);

/*! \fn void readVFAT3ADCMultiLink(const RPCMsg *request, RPCMsg *response);
 *  \brief As readVFAT3ADC(...) but for all optical links specified in ohMask on the AMC
 *  \details Here the RPCMsg request should have a "ohMask" word which specifies which OH's to read from, this is a 12 bit number where a 1 in the n^th bit indicates that the n^th OH should be read back.  Additionally there should be a "ohVfatMaskArray" which is an array of size 12 where each element is the standard vfatMask for OH specified by the array index.
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void readVFAT3ADCMultiLink(const RPCMsg *request, RPCMsg *response);

/*! \fn void setChannelRegistersVFAT3SimpleLocal(localArgs * la, uint32_t ohN, uint32_t vfatMask, uint32_t *chanRegData)
 *  \brief writes all vfat3 channel registers from AMC
 *  \param la Local arguments structure
 *  \param ohN Optical link
 *  \param vfatMask VFAT mask
 *  \param chanRegData pointer to the container holding channel registers; expected to be an array of 3072 channels with idx = vfatN * 128 + chan
 */
void setChannelRegistersVFAT3SimpleLocal(localArgs * la, uint32_t ohN, uint32_t vfatMask, uint32_t *chanRegData);

/*! \fn void setChannelRegistersVFAT3Local(localArgs * la, uint32_t ohN, uint32_t vfatMask, uint32_t *calEnable, uint32_t *masks, uint32_t *trimARM, uint32_t *trimARMPol, uint32_t *trimZCC, uint32_t *trimZCCPol);
 *  \brief writes all vfat3 channel registers from AMC
 *  \param ohN Optohybrid optical link number
 *  \param vfatMask Bitmask of chip positions determining which chips to use
 *  \param calEnable array pointer for calEnable with 3072 entries, the (vfat,chan) pairing determines the array index via: idx = vfat*128 + chan
 *  \param masks as calEnable but for channel masks
 *  \param trimARM as calEnable but for arming comparator trim value
 *  \param trimARMPol as calEnable but for arming comparator trim polarity
 *  \param trimZCC as calEnable but for zero crossing comparator trim value
 *  \param trimZCCPol as calEnable but for zero crossing comparator trim polarity
 */
void setChannelRegistersVFAT3Local(localArgs * la, uint32_t ohN, uint32_t vfatMask, uint32_t *calEnable, uint32_t *masks, uint32_t *trimARM, uint32_t *trimARMPol, uint32_t *trimZCC, uint32_t *trimZCCPol);

/*! \fn void setChannelRegistersVFAT3(const RPCMsg *request, RPCMsg *response);
 *  \brief writes all vfat3 channel registers from host machine
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void setChannelRegistersVFAT3(const RPCMsg *request, RPCMsg *response);

/*! \fn void statusVFAT3sLocal(localArgs * la, uint32_t ohN)
 *  \brief Local callable version of statusVFAT3s
 *  \param la Local arguments structure
 *  \param ohN Optohybrid optical link number
 */
void statusVFAT3sLocal(localArgs * la, uint32_t ohN);

/*! \fn void statusVFAT3s(const RPCMsg *request, RPCMsg *response)
 *  \brief Returns list of values of the most important VFAT3 register
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void statusVFAT3s(const RPCMsg *request, RPCMsg *response);

/*!
 *  \brief Decode a Reed--Muller encoded VFAT3 ChipID
 *  \param encChipID 32-bit encoded chip ID to decode
 *  \return decoded VFAT3 chip ID
 */
uint16_t decodeChipID(uint32_t encChipID);

/*! \fn void statusVFAT3s(const RPCMsg *request, RPCMsg *response)
 *  \brief Returns list of values of the most important VFAT3 register
 *  \param request RPC request message
 *  \param response RPC responce message
 */
void getVFAT3ChipIDsLocal(localArgs * la, uint32_t ohN, uint32_t vfatMask=0xFF000000, bool rawID=false);
void getVFAT3ChipIDs(const RPCMsg *request, RPCMsg *response);
void readDACValues(const RPCMsg *request, RPCMsg *response);

#endif
