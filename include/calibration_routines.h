/*! \file calibration_routines.cpp
 *  \brief Calibration routines
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#ifndef CALIBRATION_ROUTINES_H
#define CALIBRATION_ROUTINES_H

#include <map>
#include <string>
#include <tuple>
#include "utils.h"
#include <vector>

//This could be imported from xhal legacyPreBoost branch...but I expect ctp7_modules develop to outlive that
struct vfat3DACAndSize{
    //key is the monitoring select (dacSelect) value
    //value is a tuple ("reg name", dacMin, dacMax)
    std::unordered_map<uint32_t, std::tuple<std::string,int,int> > map_dacInfo;

    vfat3DACAndSize(){
        //ADC Measures Current
        //I wonder if this dacMin and dacMax info could be added to the LMDB...?
        map_dacInfo[1] = std::make_tuple("CFG_CAL_DAC", 0, 0xff);
        map_dacInfo[2] = std::make_tuple("CFG_BIAS_PRE_I_BIT", 0, 0xff);
        map_dacInfo[3] = std::make_tuple("CFG_BIAS_PRE_I_BLCC", 0, 0x3f);
        map_dacInfo[4] = std::make_tuple("CFG_BIAS_PRE_I_BSF", 0,0x3f);
        map_dacInfo[5] = std::make_tuple("CFG_BIAS_SH_I_BFCAS", 0, 0xff);
        map_dacInfo[6] = std::make_tuple("CFG_BIAS_SH_I_BDIFF", 0, 0xff);
        map_dacInfo[7] = std::make_tuple("CFG_BIAS_SD_I_BDIFF", 0, 0xff);
        map_dacInfo[8] = std::make_tuple("CFG_BIAS_SD_I_BFCAS", 0, 0xff);
        map_dacInfo[9] = std::make_tuple("CFG_BIAS_SD_I_BSF", 0, 0x3f);
        map_dacInfo[10] = std::make_tuple("CFG_BIAS_CFD_DAC_1", 0, 0x3f);
        map_dacInfo[11] = std::make_tuple("CFG_BIAS_CFD_DAC_2", 0, 0x3f);
        map_dacInfo[12] = std::make_tuple("CFG_HYST", 0, 0x3f);
        //map_dacInfo[13] = std::make_tuple("NOREG_CFGIREFLOCAL", 0, 0); //CFD Ireflocal
        map_dacInfo[14] = std::make_tuple("CFG_THR_ARM_DAC", 0, 0xff);
        map_dacInfo[15] = std::make_tuple("CFG_THR_ZCC_DAC", 0, 0xff);
        //map_dacInfo[16] = std::make_tuple("CFG_", 0,);

        //ADC Measures Voltage
        //map_dacInfo[32] = std::make_tuple("NOREG_BGR", 0,); //Band gap reference
        map_dacInfo[33] = std::make_tuple("CFG_CAL_DAC", 0, 0xff);
        map_dacInfo[34] = std::make_tuple("CFG_BIAS_PRE_VREF", 0, 0xff);
        map_dacInfo[35] = std::make_tuple("CFG_THR_ARM_DAC", 0, 0xff);
        map_dacInfo[36] = std::make_tuple("CFG_THR_ZCC_DAC", 0, 0xff);
        //map_dacInfo[37] = std::make_tuple("NOREG_VTSENSEINT", 0, 0); //Internal temperature sensor
        //map_dacInfo[38] = std::make_tuple("NOREG_VTSENSEEXT", 0, 0); //External temperature sensor (only on HV3b_V3(4) hybrids)
        map_dacInfo[39] = std::make_tuple("CFG_VREF_ADC", 0, 0x3);
        //map_dacInfo[40] = std::make_tuple("NOREG_ADCVinM", 0,); //ADC Vin M
        //map_dacInfo[41] = std::make_tuple("CFG_", 0,);
    }
};

/*! \fn std::unordered_map<std::string, uint32_t> setSingleChanMask(unsigned int ohN, int vfatN, unsigned int ch, localArgs *la)
 *  \brief Unmask the channel of interest and masks all the other
 *  \param ohN Optical link number
 *  \param vfatN VFAT position
 *  \param ch Channel of interest
 *  \param la Local arguments structure
 *  \return Original channel mask in a form of an unordered map <chanMaskRegName, mask>
 */
std::unordered_map<std::string, uint32_t> setSingleChanMask(unsigned int ohN, int vfatN, unsigned int ch, localArgs *la);

/*! \fn void applyChanMask(std::unordered_map<std::string, uint32_t> map_chanOrigMask, localArgs *la)
 *  \brief Applies channel mask
 *  \param map_chanOrigMask Original channel mask as obtained from setSingleChanMask mehod
 *  \param la Local arguments structure
 */
void applyChanMask(std::unordered_map<std::string, uint32_t> map_chanOrigMask, localArgs *la);

/*! \fn void confCalPulseLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t ch, bool toggleOn, bool currentPulse, uint32_t calScaleFactor)
 *  \brief Configures the calibration pulse for channel ch on all VFATs of ohN that are not in mask to either be on (toggleOn==true) or off (toggleOn==false).  If ch == 128 and toggleOn == False will write the CALPULSE_ENABLE bit for all channels of all vfats that are not masked on ohN to 0x0.
 *  \param la Local arguments structure
 *  \param ohN Optical link number
 *  \param mask VFAT mask
 *  \param ch Channel of interest
 *  \param toggleOn if true (false) turns the calibration pulse on (off) for channel ch
 *  \param currentPulse Selects whether to use current or volage pulse
 *  \param calScaleFactor Scale factor for the calibration pulse height (00 = 25%, 01 = 50%, 10 = 75%, 11 = 100%)
 */
bool confCalPulseLocal(localArgs *la, uint32_t ohN, uint32_t mask, uint32_t ch, bool toggleOn, bool currentPulse, uint32_t calScaleFactor);

/*! \fn void dacMonConfLocal(localArgs * la, uint32_t ohN, uint32_t ch)
 *  \brief Configures DAQ monitor. Local version only
 *  \param la Local arguments structure
 *  \param ohN Optical link number
 *  \param ch Channel of interest
 */
void dacMonConfLocal(localArgs * la, uint32_t ohN, uint32_t ch);

/*! \fn void ttcGenToggleLocal(localArgs * la, uint32_t ohN, bool enable)
 *  \brief Toggles the TTC Generator. Local callable version of ttcGenToggle
 *
 *  * v3  electronics: enable = true (false) turn on CTP7 internal TTC generator and ignore ttc commands from backplane for this AMC (turn off CTP7 internal TTC generator and take ttc commands from backplane link)
 *  * v2b electronics: enable = true (false) start (stop) the T1Controller for link ohN
 *
 *  \param la Local arguments structure
 *  \param ohN Optical link
 *  \param enable See detailed mehod description
 */
void ttcGenToggleLocal(localArgs * la, uint32_t ohN, bool enable);

/*! \fn void ttcGenToggle(const RPCMsg *request, RPCMsg *response)
 *  \brief Toggles the TTC Generator
 *
 *  * v3  electronics: enable = true (false) turn on CTP7 internal TTC generator and ignore ttc commands from backplane for this AMC (turn off CTP7 internal TTC generator and take ttc commands from backplane link)
 *  * v2b electronics: enable = true (false) start (stop) the T1Controller for link ohN
 *
 *  \param request RPC request message
 *  \param response RPC response message
 */
void ttcGenToggle(const RPCMsg *request, RPCMsg *response);

/*! \fn void ttcGenConfLocal(localArgs * la, uint32_t ohN, uint32_t mode, uint32_t type, uint32_t pulseDelay, uint32_t L1Ainterval, uint32_t nPulses, bool enable)
 *  \brief Configures TTC generator. Local callable version of ttcGenConf
 *
 *  - **v3**  electronics behavior:
 *    * pulseDelay (only for enable = true), delay between CalPulse and L1A
 *    * L1Ainterval (only for enable = true), how often to repeat signals
 *    * enable = true (false) turn on CTP7 internal TTC generator and ignore ttc commands from backplane for this AMC (turn off CTP7 internal TTC generator and take ttc commands from backplane link)
 *  - **v2b** electronics behavior:
 *    * Configure the T1 controller
 *    * mode:
 *      * 0 (Single T1 signal),
 *      * 1 (CalPulse followed by L1A),
 *      * 2 (pattern)
 *    * type (only for mode 0, type of T1 signal to send):
 *      * 0 L1A
 *      * 1 CalPulse
 *      * 2 Resync
 *      * 3 BC0
 *    * pulseDelay (only for mode 1), delay between CalPulse and L1A
 *    * L1Ainterval (only for mode 0,1), how often to repeat signals
 *    * nPulses how many signals to send (0 is continuous)
 *    * enable = true (false) start (stop) the T1Controller for link ohN
 *
 *  \param la Local arguments structure
 *  \param ohN Optical link
 *  \param mode T1 controller mode
 *  \param type Type of T1 signal to send
 *  \param pulseDelay Delay between CalPulse and L1A
 *  \param L1Ainterval How often to repeat signals (only for enable = true)
 *  \param nPulses Number of calibration pulses to generate
 *  \param enable If true (false) ignore (take) ttc commands from backplane for this AMC (affects all links)
 */
void ttcGenConfLocal(localArgs * la, uint32_t ohN, uint32_t mode, uint32_t type, uint32_t pulseDelay, uint32_t L1Ainterval, uint32_t nPulses, bool enable);

/*! \fn void ttcGenConf(const RPCMsg *request, RPCMsg *response)
 *  \brief Configures TTC generator
 *
 *  - **v3**  electronics behavior:
 *    * pulseDelay (only for enable = true), delay between CalPulse and L1A
 *    * L1Ainterval (only for enable = true), how often to repeat signals
 *    * enable = true (false) ignore (take) ttc commands from backplane for this AMC (affects all links)
 *  - **v2b** electronics behavior:
 *    * Configure the T1 controller
 *    * mode:
 *      * 0 (Single T1 signal),
 *      * 1 (CalPulse followed by L1A),
 *      * 2 (pattern)
 *    * type (only for mode 0, type of T1 signal to send):
 *      * 0 L1A
 *      * 1 CalPulse
 *      * 2 Resync
 *      * 3 BC0
 *    * pulseDelay (only for mode 1), delay between CalPulse and L1A
 *    * L1Ainterval (only for mode 0,1), how often to repeat signals
 *    * nPulses how many signals to send (0 is continuous)
 *    * enable = true (false) start (stop) the T1Controller for link ohN
 *
 *  \param request RPC request message
 *  \param response RPC response message
 */
void ttcGenConf(const RPCMsg *request, RPCMsg *response);

/*! \fn void genScanLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t mask, uint32_t ch, bool useCalPulse, bool currentPulse, uint32_t calScaleFactor, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg, bool useUltra, bool useExtTrig)
 *  \brief Generic calibration routine. Local callable version of genScan
 *  \param la Local arguments structure
 *  \param outData pointer to the results of the scan
 *  \param ohN Optical link
 *  \param mask VFAT mask
 *  \param ch Channel of interest
 *  \param useCalPulse Use  calibration pulse if true
 *  \param currentPulse Selects whether to use current or volage pulse
 *  \param calScaleFactor
 *  \param nevts Number of events per calibration point
 *  \param dacMin Minimal value of scan variable
 *  \param dacMax Maximal value of scan variable
 *  \param dacStep Scan variable change step
 *  \param scanReg DAC register to scan over name
 *  \param useUltra Set to 1 in order to use the ultra scan
 *  \param useExtTrig Set to 1 in order to use the backplane triggers
 */
void genScanLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t mask, uint32_t ch, bool useCalPulse, bool currentPulse, uint32_t calScaleFactor, uint32_t nevts, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg, bool useUltra, bool useExtTrig);

/*! \fn void genScan(const RPCMsg *request, RPCMsg *response)
 *  \brief Generic calibration routine
 *  \param request RPC request message
 *  \param response RPC response message
 */
void genScan(const RPCMsg *request, RPCMsg *response);

/*! \fn void sbitRateScanLocal(localArgs *la, uint32_t *outDataDacVal, uint32_t *outDataTrigRate, uint32_t ohN, uint32_t maskOh, bool invertVFATPos, uint32_t ch, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg, uint32_t waitTime)
 *  \brief SBIT rate scan. Local version of sbitRateScan
 *
 *  * Measures the SBIT rate seen by OHv3 ohN for the non-masked VFAT found in maskOh as a function of scanReg
 *  * It is assumed that all other VFATs are masked in the OHv3 via maskOh
 *  * Will scan from dacMin to dacMax in steps of dacStep
 *  * The x-values (e.g. scanReg values) will be stored in outDataDacVal
 *  * The y-valued (e.g. rate) will be stored in outDataTrigRate
 *  * Each measured point will take waitTime milliseconds (recommond between 1000->3000)
 *  * The measurement is performed for all channels (ch=128) or a specific channel (0 <= ch <= 127)
 *  * invertVFATPos is for FW backwards compatiblity; if true then the vfatN =  23 - map_maskOh2vfatN[maskOh]
 *
 *  \param la Local arguments structure
 *  \param outDataDacVal
 *  \param outDataTrigRate
 *  \param ohN Optohybrid optical link number
 *  \param maskOh VFAT mask, should only have one unmasked chip
 *  \param invertVFATPos is for FW backwards compatiblity; if true then the vfatN =  23 - map_maskOh2vfatN[maskOh]
 *  \param ch Channel of interest
 *  \param dacMin Minimal value of scan variable
 *  \param dacMax Maximal value of scan variable
 *  \param dacStep Scan variable change step
 *  \param scanReg DAC register to scan over name
 *  \waitTime Measurement duration per point in milliseconds
 */
void sbitRateScanLocal(localArgs *la, uint32_t *outDataDacVal, uint32_t *outDataTrigRate, uint32_t ohN, uint32_t maskOh, bool invertVFATPos, uint32_t ch, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg, uint32_t waitTime);

/*! \fn void sbitRateScanParallelLocal(localArgs *la, uint32_t *outDataDacVal, uint32_t *outDataTrigRatePerVFAT, uint32_t *outDataTrigRateOverall, uint32_t ohN, uint32_t vfatmask, uint32_t ch, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg, uint32_t waitTime)
 *  \brief Parallel SBIT rate scan. Local version of sbitRateScan
 *
 *  * Measures the SBIT rate seen by OHv3 ohN for the non-masked VFATs defined in vfatmask as a function of scanReg
 *  * Will scan from dacMin to dacMax in steps of dacStep
 *  * The x-values (e.g. scanReg values) will be stored in outDataDacVal
 *  * For each VFAT the y-valued (e.g. rate) will be stored in outDataTrigRatePerVFAT
 *  * For the overall y-value (e.g. rate) will be stored in outDataTrigRateOverall
 *  * Each measured point will take one second
 *  * The measurement is performed for all channels (ch=128) or a specific channel (0 <= ch <= 127)
 *
 *  \param la Local arguments structure
 *  \param outDataDacVal
 *  \param outDataTrigRatePerVFAT
 *  \param outDataTrigRateOverall
 *  \param ohN Optohybrid optical link number
 *  \param vfatMask VFAT mask
 *  \param ch Channel of interest
 *  \param dacMin Minimal value of scan variable
 *  \param dacMax Maximal value of scan variable
 *  \param dacStep Scan variable change step
 *  \param scanReg DAC register to scan over name
 *  \param waitTime Amount of time to collect data, applies to the per-VFAT rates not the overall rate, maximum is just more than 107 seconds
 */
void sbitRateScanParallelLocal(localArgs *la, uint32_t *outDataDacVal, uint32_t *outDataTrigRatePerVFAT, uint32_t *outDataTrigRateOverall, uint32_t ohN, uint32_t vfatmask, uint32_t ch, uint32_t dacMin, uint32_t dacMax, uint32_t dacStep, std::string scanReg);

/*! \fn void sbitRateScan(const RPCMsg *request, RPCMsg *response)
 *  \brief SBIT rate scan. See the local callable methods documentation for details
 *  \param request RPC response message
 *  \param response RPC response message
 */
void sbitRateScan(const RPCMsg *request, RPCMsg *response);

/*! \fn void checkSbitMappingWithCalPulseLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t mask, bool currentPulse, uint32_t calScaleFactor, uint32_t nevts, uint32_t L1Ainterval, uint32_t pulseDelay)
 *  \brief With all but one channel masked, pulses a given channel, and then checks which sbits are seen by the CTP7, repeats for all channels on vfatN; reports the (vfat,chan) pulsed and (vfat,sbit) observed where sbit=chan*2; additionally reports if the cluster was valid.
 *  \details The SBIT Monitor stores the 8 SBITs that are sent from the OH (they are all sent at the same time and correspond to the same clock cycle). Each SBIT clusters readout from the SBIT Monitor is a 16 bit word with bits [0:10] being the sbit address and bits [12:14] being the sbit size, bits 11 and 15 are not used.
 *  \details The possible values of the SBIT Address are [0,1535].  Clusters with address less than 1536 are considered valid (e.g. there was an sbit); otherwise an invalid (no sbit) cluster is returned.  The SBIT address maps to a given trigger pad following the equation \f$sbit = addr % 64\f$.  There are 64 such trigger pads per VFAT.  Each trigger pad corresponds to two VFAT channels.  The SBIT to channel mapping follows \f$sbit=floor(chan/2)\f$.  You can determine the VFAT position of the sbit via the equation \f$vfatPos=7-int(addr/192)+int((addr%192)/64)*8\f$.
 *  \details The SBIT size represents the number of adjacent trigger pads are part of this cluster.  The SBIT address always reports the lowest trigger pad number in the cluster.  The sbit size takes values [0,7].  So an sbit cluster with address 13 and with size of 2 includes 3 trigger pads for a total of 6 vfat channels and starts at channel \f$13*2=26\f$ and continues to channel \f$(2*15)+1=31\f$.
 *  \param la Local arguments structure
 *  \param outData pointer to an array of size (24*128*8*nevts) which stores the results of the scan, bits [0,7] channel pulsed; bits [8:15] sbit observed; bits [16:20] vfat pulsed; bits [21,25] vfat observed; bit 26 isValid; bits [27,29] are the cluster size
 *  \param ohN Optical link
 *  \param vfatN specific vfat position to be tested
 *  \param mask VFATs to be excluded from the trigger
 *  \param useCalPulse true (false) checks sbit mapping with calpulse on (off); useful for measuring noise
 *  \param currentPulse Selects whether to use current or volage pulse
 *  \param calScaleFactor
 *  \param nevts the number of cal pulses to inject per channel
 *  \param L1Ainterval How often to repeat signals (only for enable = true)
 *  \param pulseDelay delay between CalPulse and L1A
 */
void checkSbitMappingWithCalPulseLocal(localArgs *la, uint32_t *outData, uint32_t ohN, uint32_t vfatN, uint32_t mask, bool useCalPulse, bool currentPulse, uint32_t calScaleFactor, uint32_t nevts, uint32_t L1Ainterval, uint32_t pulseDelay);

/*! \fn void checkSbitMappingWithCalPulse(const RPCMsg *request, RPCMsg *response)
 *  \brief Checks the sbit mapping using the calibration pulse. See the local callable methods documentation for details
 *  \param request RPC response message
 *  \param response RPC response message
 */
void checkSbitMappingWithCalPulse(const RPCMsg *request, RPCMsg *response);

/*! \fn void checkSbitRateWithCalPulseLocal(localArgs *la, uint32_t *outDataCTP7Rate, uint32_t *outDataFPGAClusterCntRate, uint32_t *outDataVFATSBits, uint32_t ohN, uint32_t mask, bool currentPulse, uint32_t calScaleFactor, uint32_t waitTime, uint32_t pulseRate, uint32_t pulseDelay)
 * \brief With all but one channel masked, pulses a given channel, and then checks the rate of sbits seen by the OH FPGA and CTP7, repeats for all channels; reports the rate observed
 *  \param la Local arguments structure
 *  \param outDataCTP7Rate pointer to an array storing the value of GEM_AMC.TRIGGER.OHX.TRIGGER_RATE for X = ohN; array size 3072 elements, idx = 128 * vfat + chan
 *  \param outDataFPGAClusterCntRate as outDataCTP7Rate but for the value of GEM_AMC.OH.OHX.FPGA.TRIG.CNT.CLUSTER_COUNT
 *  \param outDataVFATSBits as outDataCTP7Rate but for the value of GEM_AMC.OH.OHX.FPGA.TRIG.CNT.VFATY_SBITS for X = ohN and Y the vfat number (following the array idx rule above)
 *  \param ohN Optical link
 *  \param vfatN specific vfat position to be tested
 *  \param mask VFATs to be excluded from the trigger
 *  \param useCalPulse true (false) checks sbit mapping with calpulse on (off); useful for measuring noise
 *  \param currentPulse Selects whether to use current or volage pulse
 *  \param calScaleFactor
 *  \waitTime Measurement duration per point in milliseconds
 *  \param pulseRate rate of calpulses to be sent in Hz
 *  \param pulseDelay delay between CalPulse and L1A
 */
void checkSbitRateWithCalPulseLocal(localArgs *la, uint32_t *outDataCTP7Rate, uint32_t *outDataFPGAClusterCntRate, uint32_t *outDataVFATSBits, uint32_t ohN, uint32_t vfatN, uint32_t mask, bool useCalPulse, bool currentPulse, uint32_t calScaleFactor, uint32_t waitTime, uint32_t pulseRate, uint32_t pulseDelay);

/*! \fn void checkSbitRateWithCalPulse(const RPCMsg *request, RPCMsg *response)
 *  \brief Checks the sbit rate using the calibration pulse. See the local callable methods documentation for details
 *  \param request RPC response message
 *  \param response RPC response message
 */
void checkSbitRateWithCalPulse(const RPCMsg *request, RPCMsg *response);

/*! \fn std::vector<uint32_t> dacScanLocal(localArgs *la, uint32_t ohN, uint32_t dacSelect, uint32_t dacStep=1, uint32_t mask=0xFF000000, bool useExtRefADC=false)
 *  \brief configures the VFAT3 DAC Monitoring and then scans the DAC and records the measured ADC values for all unmasked VFATs
 *  \param la Local arguments structure
 *  \param ohN Optical link
 *  \param dacSelect Monitor Sel for ADC monitoring in VFAT3, see documentation for GBL_CFG_CTR_4 in VFAT3 manual for more details
 *  \param dacStep step size to scan the dac in
 *  \param mask VFAT mask to use, a value of 1 in the N^th bit indicates the N^th VFAT is masked
 *  \param useExtRefADC if (true) false use the (externally) internally referenced ADC on the VFAT3 for monitoring
 *  \return Returns a std::vector<uint32_t> object of size 24*(dacMax-dacMin+1)/dacStep where dacMax and dacMin are described in the VFAT3 manual.  For each element bits [7:0] are the dacValue, bits [17:8] are the ADC readback value in either current or voltage units depending on dacSelect (again, see VFAT3 manual), bits [22:18] are the VFAT position, and bits [26:23] are the optohybrid number.
 */
std::vector<uint32_t> dacScanLocal(localArgs *la, uint32_t ohN, uint32_t dacSelect, uint32_t dacStep=1, uint32_t mask=0xFF000000, bool useExtRefADC=false);

/*! \fn void dacScan(const RPCMsg *request, RPCMsg *response)
 *  \brief allows the host machine to perform a dacScan for all unmasked VFATs on a given optohybrid, see Local version for details.
 *  \param request rpc request message
 *  \param response rpc responce message
 */
void dacScan(const RPCMsg *request, RPCMsg *response);

/*! \fn void dacScanMultiLink(const RPCMsg *request, RPCMsg *response)
 *  \brief As dacScan(...) but for all optohybrids on the AMC
 *  \details Here the RPCMsg request should have a "ohMask" word which specifies which OH's to read from, this is a 12 bit number where a 1 in the n^th bit indicates that the n^th OH should be read back.
 *  \param request rpc request message
 *  \param response rpc responce message
 */
void dacScanMultiLink(const RPCMsg *request, RPCMsg *response);

/*! \fn void genChannelScan(const RPCMsg *request, RPCMsg *response)
 *  \brief Generic per channel scan. See the local callable methods documentation for details
 *  \param request RPC response message
 *  \param response RPC response message
 */
void genChannelScan(const RPCMsg *request, RPCMsg *response);

/*! \fn void confCalPulse(const RPCMsg *request, RPCMsg *response)
 *  \brief Generic method for calibration pulse configuration. Local callable version of confCalPulseLocal.
 *  \param[in] request RPC response message with the following keys:
 *     - `word ohN` : optical link number
 *     - `word mask`: VFAT mask
 *     - `word ch`  : Channel of interest
 *     - `word toggleOn`: if true (false) turns the calibration pulse on (off) for channel ch
 *     - `word currentPulse`: Selects whether to use current or volage pulse
 *     - `word calScaleFactor`: Scale factor for the calibration pulse height (00 = 25%, 01 = 50%, 10 = 75%, 11 = 100%)
 *  \param response RPC response message
 */
void confCalPulse(const RPCMsg *request, RPCMsg *response);

#endif
