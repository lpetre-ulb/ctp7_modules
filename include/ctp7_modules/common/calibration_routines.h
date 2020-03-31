/*! \file calibration_routines.cpp
 *  \brief Calibration routines
 *  \author Cameron Bravo <cbravo135@gmail.com>
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#ifndef COMMON_CALIBRATION_ROUTINES_H
#define COMMON_CALIBRATION_ROUTINES_H

#include "xhal/common/rpc/common.h"

#include <map>
#include <unordered_map>
#include <string>
#include <tuple>
#include <vector>

namespace calibration {
  /*!
   *  \brief Maps the VFAT3 DACs and their ranges
   *
   *  \detail key is the monitoring select (dacSelect) value
   *          value is a tuple ("reg name", dacMin, dacMax)
   */
  const std::unordered_map<uint32_t, std::tuple<std::string, int, int>> vfat3DACAndSize {
    // ADC Measures Current
    // FIXME I wonder if this dacMin and dacMax info could be added to the LMDB...?
    {1,  std::make_tuple("CFG_CAL_DAC",         0, 0xff)},
    {2,  std::make_tuple("CFG_BIAS_PRE_I_BIT",  0, 0xff)},
    {3,  std::make_tuple("CFG_BIAS_PRE_I_BLCC", 0, 0x3f)},
    {4,  std::make_tuple("CFG_BIAS_PRE_I_BSF",  0, 0x3f)},
    {5,  std::make_tuple("CFG_BIAS_SH_I_BFCAS", 0, 0xff)},
    {6,  std::make_tuple("CFG_BIAS_SH_I_BDIFF", 0, 0xff)},
    {7,  std::make_tuple("CFG_BIAS_SD_I_BDIFF", 0, 0xff)},
    {8,  std::make_tuple("CFG_BIAS_SD_I_BFCAS", 0, 0xff)},
    {9,  std::make_tuple("CFG_BIAS_SD_I_BSF",   0, 0x3f)},
    {10, std::make_tuple("CFG_BIAS_CFD_DAC_1",  0, 0x3f)},
    {11, std::make_tuple("CFG_BIAS_CFD_DAC_2",  0, 0x3f)},
    {12, std::make_tuple("CFG_HYST",            0, 0x3f)},
    // {13, std::make_tuple("NOREG_CFGIREFLOCAL", 0, 0)}, //CFD Ireflocal
    {14, std::make_tuple("CFG_THR_ARM_DAC",     0, 0xff)},
    {15, std::make_tuple("CFG_THR_ZCC_DAC",     0, 0xff)},
    // {16, std::make_tuple("CFG_", 0,)},

    // ADC Measures Voltage
    // {32, std::make_tuple("NOREG_BGR", 0,)}, //Band gap reference
    {33, std::make_tuple("CFG_CAL_DAC",       0, 0xff)},
    {34, std::make_tuple("CFG_BIAS_PRE_VREF", 0, 0xff)},
    {35, std::make_tuple("CFG_THR_ARM_DAC",   0, 0xff)},
    {36, std::make_tuple("CFG_THR_ZCC_DAC",   0, 0xff)},
    // {37, std::make_tuple("NOREG_VTSENSEINT", 0, 0)}, // Internal temperature sensor
    // {38, std::make_tuple("NOREG_VTSENSEEXT", 0, 0)}, // External temperature sensor (only on HV3b_V3(4) hybrids)
    {39, std::make_tuple("CFG_VREF_ADC",      0, 0x3)},
    // {40, std::make_tuple("NOREG_ADCVinM", 0,)}, //ADC Vin M
    // {41, std::make_tuple("CFG_", 0,)},
  };

  /*!
   *  \brief Configures the calibration pulse for a specified channel
   *
   *  \detail Each channel will either be on (toggleOn==true) or off (toggleOn==false).
   *          If ch == 128 and toggleOn == False will write the CALPULSE_ENABLE bit for all
   *          channels of all VFATs that are not masked on ohN to 0x0.
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatMask VFAT mask
   *  \param \c ch Channel of interest
   *  \param \c toggleOn if true (false) enables (disables) the calibration pulse for the specified channel
   *  \param \c currentPulse Selects whether to use current injection or voltage pulse mode
   *  \param \c calScaleFactor Scale factor for the calibration pulse height (00 = 25%, 01 = 50%, 10 = 75%, 11 = 100%)
   */
  struct confCalPulse : xhal::common::rpc::Method
  {
    void operator()(const uint16_t& ohN,
                    const uint32_t& mask,
                    const uint8_t& ch,
                    const bool& toggleOn,
                    const bool& currentPulse,
                    const uint32_t& calScaleFactor) const;
  };

  /*!
   *  \brief Configures DAQ monitor
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c ch Channel of interest
   */
  struct dacMonConf : xhal::common::rpc::Method
  {
    void operator()(const uint16_t& ohN, const uint8_t& ch) const;
  };

  /*!
   *  \brief Toggles the TTC Generator
   *
   *  enable = true (false) turn on CTP7 internal TTC generator and ignore ttc commands from backplane for this AMC (turn off CTP7 internal TTC generator and take ttc commands from backplane link)
   *
   *  \param \c enable See detailed mehod description
   */
  struct ttcGenToggle : xhal::common::rpc::Method
  {
    void operator()(const bool& enable) const;
  };

  /*!
   *  \brief Configures TTC generator
   *
   *  * pulseDelay (only for enable = true), delay between CalPulse and L1A
   *  * L1Ainterval (only for enable = true), how often to repeat signals
   *  * enable = true (false) turn on CTP7 internal TTC generator and ignore ttc commands from backplane for this AMC (turn off CTP7 internal TTC generator and take ttc commands from backplane link)
   *
   *  \param \c pulseDelay Delay between CalPulse and L1A
   *  \param \c L1Ainterval How often to repeat signals (only for enable = true)
   *  \param \c enable If true (false) ignore (take) ttc commands from backplane for this AMC (affects all links)
   */
  struct ttcGenConf : xhal::common::rpc::Method
  {
    void operator()(const uint32_t& pulseDelay, const uint32_t& L1Ainterval,
                    const bool& enable) const;
  };

  /*!
   *  \brief Generic calibration routine
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatMask VFAT mask
   *  \param \c ch Channel of interest
   *  \param \c useCalPulse Use  calibration pulse if true
   *  \param \c currentPulse Selects whether to use current or volage pulse
   *  \param \c calScaleFactor
   *  \param \c nevts Number of events per calibration point
   *  \param \c dacMin Minimal value of scan variable
   *  \param \c dacMax Maximal value of scan variable
   *  \param \c dacStep Scan variable change step
   *  \param \c scanReg DAC register to scan over name
   *  \param \c useUltra Set to 1 in order to use the ultra scan
   *  \param \c useExtTrig Set to 1 in order to use the backplane triggers
   *
   *  \returns an \c std::vector containing the results
   */
  struct genScan : xhal::common::rpc::Method
  {
    std::vector<uint32_t> operator()(const uint16_t& ohN,
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
                                     const bool& useUltra=false,
                                     const bool& useExtTrig=false) const;
  };

  /*!
   *  \brief Generic per channel scan, options as in \c genScan
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatMask VFAT mask
   *  \param \c useCalPulse Use calibration pulse if true
   *  \param \c currentPulse Selects whether to use current or volage pulse
   *  \param \c calScaleFactor
   *  \param \c nevts Number of events per calibration point
   *  \param \c dacMin Minimal value of scan variable
   *  \param \c dacMax Maximal value of scan variable
   *  \param \c dacStep Scan variable change step
   *  \param \c scanReg DAC register to scan over name
   *  \param \c useUltra Set to 1 in order to use the ultra scan
   *  \param \c useExtTrig Set to 1 in order to use the backplane triggers
   *
   *  \returns an \c std::map key is the channel number, values are std::vectors as in \c genScan
   */
  struct genChannelScan : xhal::common::rpc::Method
  {
    std::map<uint32_t, std::vector<uint32_t>> operator()(const uint16_t& ohN,
                                                         const uint32_t& vfatMask,
                                                         const bool& useCalPulse,
                                                         const bool& currentPulse,
                                                         const uint32_t& calScaleFactor,
                                                         const uint32_t& nevts,
                                                         const uint32_t& dacMin,
                                                         const uint32_t& dacMax,
                                                         const uint32_t& dacStep,
                                                         const std::string& scanReg,
                                                         const bool& useUltra,
                                                         const bool& useExtTrig) const;
  };

  /*!
   *  \brief s-bit rate scan
   *
   *  \detail Measures the s-bit rate seen by an OHv3 for the non-masked VFAT as a function of a specified VFAT registers
   *          It is assumed that all other VFATs are masked in the OHv3 via \c vfatMask
   *          Will scan from \c dacMin to \c dacMax in steps of \c dacStep
   *          The x-values (e.g. scanReg values) will be stored as the output map keys
   *          The y-valued (e.g. rate) will be stored as the output map values
   *          Each measured point will take \c waitTime milliseconds (recommond between 1000->3000)
   *          The measurement is performed for all channels (ch=128) or a specific channel (0 <= ch <= 127)
   *          invertVFATPos is for FW backwards compatiblity; if true then the vfatN = 23 - map_vfatMask2vfatN[vfatMask]
   *
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatMask VFAT mask, should only have one unmasked chip
   *  \param \c ch Channel of interest
   *  \param \c dacMin Minimal value of scan variable
   *  \param \c dacMax Maximal value of scan variable
   *  \param \c dacStep Scan variable change step
   *  \param \c scanReg DAC register to scan over name
   *  \param \c waitTime Measurement duration per point in milliseconds
   *  \param \c invertVFATPos is for FW backwards compatiblity; if true then the vfatN =  23 - map_vfatMask2vfatN[vfatMask]
   *
   *  \returns an \c std::map of DAC value to s-bit rate
   */
  struct sbitRateScan : xhal::common::rpc::Method
  {
    std::map<uint32_t, uint32_t> operator()(const uint16_t& ohN,
                                            const uint32_t& vfatMask,
                                            const uint8_t& ch,
                                            const uint16_t& dacMin,
                                            const uint16_t& dacMax,
                                            const uint16_t& dacStep,
                                            const std::string& scanReg,
                                            const uint32_t& waitTime,
                                            const bool& invertVFATPos=false) const;
  };

  /*!
   *  \brief Parallel s-bit rate scan.
   *
   *  \detail Measures the s-bit rate seen by OHv3 ohN for the non-masked VFATs defined in
   *          vfatMask as a function of scanReg.
   *          Will scan from dacMin to dacMax in steps of dacStep.
   *          The x-values (e.g. scanReg values) will be keys in the output dictionary
   *          For each VFAT the y-values (e.g. rate) will be stored in a vector
   *          For the overall y-value (e.g. rate) will be stored in the output vector, as the final entry
   *          Each measured point will take one second
   *          The measurement is performed for all channels (ch=128) or a specific channel (0 <= ch <= 127)
   *
   *  \param \c ch Channel of interest
   *  \param \c dacMin Minimal value of scan variable
   *  \param \c dacMax Maximal value of scan variable
   *  \param \c dacStep Scan variable change step
   *  \param \c scanReg DAC register to scan over name
   *  \param \c waitTime Amount of time to collect data, applies to the per-VFAT rates not the overall rate,
   *         maximum is just more than 107 seconds
   *
   *  \returns an \c std::map of DAC value key to vector of rates; 1 for each VFAT, and the last one is the OR
   */
  struct sbitRateScanParallel : xhal::common::rpc::Method
  {
    std::map<uint32_t, std::vector<uint32_t>> operator()(const uint8_t& ch,
                                                         const uint16_t& dacMin,
                                                         const uint16_t& dacMax,
                                                         const uint16_t& dacStep,
                                                         const std::string& scanReg,
                                                         const uint16_t& ohMask=0xfff,
                                                         const uint32_t& waitTime=1) const;
  };

  /*!
   *  \brief Checks that the s-bit mapping is correct using the calibration pulse of the VFAT
   *
   *  \details With all but one channel masked
   *           1) pulse a given channel
   *           2) then check which s-bits are seen by the CTP7
   *           3) repeats for all channels on vfatN
   *           reports the (VFAT,chan) pulsed and (VFAT,s-bit) observed where s-bit=chan*2; additionally reports if the cluster was valid.
   *
   *  \details The s-bit Monitor stores the 8 s-bits that are sent from the OH (they are all sent at the same time and correspond to the same clock cycle).
               Each s-bit clusters readout from the s-bit Monitor is a 16 bit word with bits [0:10] being the s-bit address and bits [12:14] being the s-bit size, bits 11 and 15 are not used.
   *           The possible values of the s-bit Address are [0,1535].
   *           Clusters with address less than 1536 are considered valid (e.g. there was an s-bit); otherwise an invalid (no s-bit) cluster is returned.
   *           The s-bit address maps to a given trigger pad following the equation \f$s-bit = addr % 64\f$.
   *           There are 64 such trigger pads per VFAT.
   *           Each trigger pad corresponds to two VFAT channels.
   *           The s-bit to channel mapping follows \f$s-bit=floor(chan/2)\f$.
   *           You can determine the VFAT position of the s-bit via the equation \f$vfatPos=7-int(addr/192)+int((addr%192)/64)*8\f$.
   *           The s-bit size represents the number of adjacent trigger pads are part of this cluster.
   *           The s-bit address always reports the lowest trigger pad number in the cluster.
   *           The s-bit size takes values [0,7].
   *           So an s-bit cluster with address 13 and with size of 2 includes 3 trigger pads for a total of 6 VFAT channels and starts at channel \f$13*2=26\f$ and continues to channel \f$(2*15)+1=31\f$.
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatN specific VFAT position to be tested
   *  \param \c vfatMask VFATs to be excluded from the trigger
   *  \param \c useCalPulse true (false) checks s-bit mapping with calpulse on (off); useful for measuring noise
   *  \param \c currentPulse Selects whether to use current or volage pulse
   *  \param \c calScaleFactor
   *  \param \c nevts the number of cal pulses to inject per channel
   *  \param \c L1Ainterval How often to repeat signals (only for enable = true)
   *  \param \c pulseDelay delay between CalPulse and L1A
   *
   *  \returns an \c std::vector containing the results of the scan with the following format
   *           bits [0,7] channel pulsed
   *           bits [8:15] s-bit observed
   *           bits [16:20] VFAT pulsed
   *           bits [21,25] VFAT observed
   *           bit 26 isValid
   *           bits [27,29] cluster size

   */
  struct checkSbitMappingWithCalPulse : xhal::common::rpc::Method
  {
    std::vector<uint32_t> operator()(const uint16_t& ohN,
                                     const uint32_t& vfatN,
                                     const uint32_t& vfatMask,
                                     const bool& useCalPulse,
                                     const bool& currentPulse,
                                     const uint32_t& calScaleFactor,
                                     const uint32_t& nevts,
                                     const uint32_t& L1Ainterval,
                                     const uint32_t& pulseDelay) const;
  };

  /*!
   *  \brief With all but one channel masked, pulses a given channel, and then checks the rate of s-bits seen by the OH FPGA and CTP7, repeats for all channels; reports the rate observed
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c vfatN specific VFAT position to be tested
   *  \param \c vfatMask VFATs to be excluded from the trigger
   *  \param \c useCalPulse true (false) checks s-bit mapping with calpulse on (off); useful for measuring noise
   *  \param \c currentPulse Selects whether to use current or volage pulse
   *  \param \c calScaleFactor
   *  \param \c waitTime Measurement duration per point in milliseconds
   *  \param \c pulseRate rate of calpulses to be sent in Hz
   *  \param \c pulseDelay delay between CalPulse and L1A
   *
   *  \returns an \c std::map of \c std::string to \c std::vector with one entry for each channel, where the keys are:
   *           "CTP7" stores the value of GEM_AMC.TRIGGER.OHX.TRIGGER_RATE for X = ohN
   *           "FPGA" stores the value of GEM_AMC.OH.OHX.FPGA.TRIG.CNT.CLUSTER_COUNT N.B. not converted to rate!
   *           "VFAT" stores the value of GEM_AMC.OH.OHX.FPGA.TRIG.CNT.VFATY_SBITS for X = ohN and Y the vfat number
   *           N.B. FPGA and VFAT values are *not* converted to a rate, this can be done by *waitTime/1000.;

   */
  struct checkSbitRateWithCalPulse : xhal::common::rpc::Method
  {
    std::map<std::string, std::vector<uint32_t>> operator()(const uint16_t& ohN,
                                                            const uint32_t& vfatN,
                                                            const uint32_t& vfatMask,
                                                            const bool& useCalPulse,
                                                            const bool& currentPulse,
                                                            const uint32_t& calScaleFactor,
                                                            const uint32_t& waitTime,
                                                            const uint32_t& pulseRate,
                                                            const uint32_t& pulseDelay) const;
  };

  /*!
   *  \brief configures the VFAT3 DAC Monitoring and then scans the DAC and records the measured ADC values for all unmasked VFATs
   *
   *  \param \c ohN OptoHybrid optical link number
   *  \param \c dacSelect Monitor Sel for ADC monitoring in VFAT3, see documentation for GBL_CFG_CTR_4 in VFAT3 manual for more details
   *  \param \c dacStep step size to scan the dac in
   *  \param \c vfatMask VFAT mask to use, a value of 1 in the N^th bit indicates the N^th VFAT is masked
   *  \param \c useExtRefADC if (true) false use the (externally) internally referenced ADC on the VFAT3 for monitoring
   *
   *  \returns an \c std::vector<uint32_t> object of size 24*(dacMax-dacMin+1)/dacStep,
   *           where dacMax and dacMin are described in the VFAT3 manual.
   *           For each element
   *              bits [7:0] are the dacValue
   *              bits [17:8] are the ADC readback value in either current or voltage units depending on dacSelect (cf. VFAT3 manual)
   *              bits [22:18] are the VFAT position
   *              bits [26:23] are the OptoHybrid number.
   */
  struct dacScan : xhal::common::rpc::Method
  {
    std::vector<uint32_t> operator()(const uint16_t& ohN,
                                     const uint16_t& dacSelect,
                                     const uint16_t& dacStep=1,
                                     const uint32_t& vfatMask=0xff000000,
                                     const bool& useExtRefADC=false) const;
  };

  /*!
   *  \brief As \c dacScan, but for all OptoHybrids connected to the AMC
   *
   *  \param \c ohMask A 12 bit number which specifies which OptoHybrids to read from.
   *         Having a value of 1 in the n^th bit indicates that the n^th OptoHybrid should be considered.
   *
   *  \returns an \c std::map where the key is the OH ID and the value is the std::vector returned by \c dacScan
   */
  struct dacScanMultiLink : xhal::common::rpc::Method
  {
    std::map<uint32_t, std::vector<uint32_t>> operator()(const uint16_t& ohMask,
                                                         const uint16_t& dacSelect,
                                                         const uint16_t& dacStep=1,
                                                         const uint32_t& vfatMask=0xff000000,
                                                         const bool& useExtRefADC=false) const;
  };
}

#endif
