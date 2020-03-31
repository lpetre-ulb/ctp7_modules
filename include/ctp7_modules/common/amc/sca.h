/*!
 * \file amc/sca.h
 * \brief RPC module for AMC SCA methods, copied in from cmsgemos/gem::hw::HwGenericAMC
 * \author Jared Sturdy <sturdy@cern.ch>
 */

#ifndef COMMON_AMC_SCA_H
#define COMMON_AMC_SCA_H

#include "ctp7_modules/common/amc/sca_enums.h"

#include "xhal/common/rpc/common.h"

namespace amc {
  namespace sca {

    /*!
     * \defgroup sca SCA module functionality
     */

    /*!
     * \brief Execute a command using the SCA interface
     *
     * \details Generic command to drive commands to all SCA modules
     *
     * \param \c ch channel to communicate with
     * \param \c cmd command to send, \ref sca_enums.h
     * \param \c len length of the data to send, available 1,2,4
     * \param \c data to send to the CTRL command
     * \param \c ohMask bit list of \c OptoHybrids to send the commands to
     */
    struct sendSCACommand : public xhal::common::rpc::Method
    {
      void operator()(const uint8_t& ch, const uint8_t& cmd, const uint8_t& len, const uint32_t& data, const uint16_t& ohMask=0xfff) const;
    };

    /*!
     * \brief Execute a command using the SCA interface, and read the reply from the SCA
     *
     * \param \c ch channel to communicate with
     * \param \c cmd command to send, \ref sca_enums.h
     * \param \c len length of the data to send, available 1,2,4
     * \param \c data to send to the CTRL command
     * \param \c ohMask bit list of \c OptoHybrids to send the commands to
     */
    struct sendSCACommandWithReply : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const uint8_t& ch, const uint8_t& cmd, const uint8_t& len, const uint32_t&, const uint16_t& ohMask=0xfff) const;
    };

    /*!
     * \brief Execute a command using the SCA CTRL interface
     *
     * \details CTRL contains
     *          * generic SCA R/W control registers
     *          * Chip ID (on channel 0x14, ADC)
     *          * SEU counter and reset (on channel 0x13, JTAG)
     *
     * \param \c cmd which command to send, \ref sca_enums.h
     * \param \c ohMask bit list of \c OptoHybrids to send the commands to
     * \param \c len length of the data to send, default is 0x1
     * \param \c data to send to the CTRL command, default is 0x0
     */
    struct scaCTRLCommand : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const sca::CTRLCommand& cmd, const uint16_t& ohMask=0xfff, const uint8_t& len=0x1, const uint32_t& data=0x0) const;
    };

    /** Locally executed methods */
    /*!
     * \brief Execute a command using the SCA I2C interface
     *
     * \details I2C bus will be enabled
     *
     * \param \c ch I2C channel to communicate with
     * \param \c cmd which command to send, \ref sca_enums.h
     * \param \c len length of the data to send, available 1,2,4
     * \param \c data to send to the I2C command
     * \param \c ohMask bit list of \c OptoHybrids to send the commands to
     */
    struct scaI2CCommand : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const sca::I2CChannel& ch, const sca::I2CCommand& cmd, const uint8_t& len, const uint32_t& data, const uint16_t& ohMask=0xfff) const;
    };

    /*!
     * \brief Execute a command using the SCA GPIO interface
     *
     * \details GPIO bus will be enabled
     *
     * \param \c ch GPIO channel to communicate with
     * \param \c cmd which command to send, \ref sca_enums.h
     * \param \c len length of the data to send, available 1,2,4
     * \param \c data to send to the GPIO command
     * \param \c ohMask bit list of \c OptoHybrids to send the commands to
     */
    struct scaGPIOCommand : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const sca::GPIOCommand& cmd, const uint8_t& len, const uint32_t& data, const uint16_t& ohMask=0xfff) const;
    };

    /*!
     * \brief Execute a command using the SCA ADC interface
     *
     * \param \c ch ADC channel to communicate with
     * \param \c len length of the data to send, available 1,2,4
     * \param \c data to send to the ADC command
     * \param \c ohMask bit list of \c OptoHybrids to send the commands to
     */
    struct scaADCCommand : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const sca::ADCChannel& ch, const uint16_t& ohMask=0xfff) const;
    };

    /*** CTRL submodule ***/
    /*!
     * \brief Reset the SCA module
     */
    struct scaModuleReset : public xhal::common::rpc::Method
    {
      void operator()(const uint16_t& ohMask) const;
    };

    /*!
     * \brief Reset the SCA module
     *
     * \param \c en true to enable the TTC HardReset action
     */
    struct scaHardResetEnable : public xhal::common::rpc::Method
    {
      void operator()(const bool& en) const;
    };

    /*!
     * \brief Read the Chip ID from the SCA ASIC
     *
     * \param \c ohMask bit list of \c OptoHybrids to send the commands to
     * \param \c scaV1 true for V1 of the SCA ASIC
     */
    struct readSCAChipID : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const uint16_t& ohMask=0xfff, const bool& scaV1=false) const;
    };

    /*!
     * \brief Read the SEU coutner from the SCA ASIC
     *
     * \param \c ohMask bit list of \c OptoHybrids to send the commands to
     * \param \c reset true to reset the counter before reading
     */
    struct readSCASEUCounter : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const uint16_t& ohMask=0xfff, const bool& reset=false) const;
    };

    /*!
     * \brief Reset the SCA SEU counter
     *
     * \param \c ohMask bit list of \c OptoHybrids to send the reset command to
     */
    struct resetSCASEUCounter : public xhal::common::rpc::Method
    {
      void operator()(const uint16_t& ohMask=0xfff) const;
    };

    /* /\*** SCA ADC submodule ***\/ */
    /* /\*! */
    /*  * \brief Reset the SCA module */
    /*  *\/ */
    /* struct scaModuleReset : public xhal::common::rpc::Method */
    /* { */
    /*   void operator()() const; */
    /* }; */

    /* /\*** GPIO CTRL submodule ***\/ */
    /* /\*! */
    /*  * \brief Reset the SCA module */
    /*  *\/ */
    /* struct scaModuleReset : public xhal::common::rpc::Method */
    /* { */
    /*   void operator()(); */
    /* }; */

    /* /\*** I2C CTRL submodule ***\/ */
    /* /\*! */
    /*  * \brief Reset the SCA module */
    /*  *\/ */
    /* struct scaModuleReset : public xhal::common::rpc::Method */
    /* { */
    /*   void operator()() const; */
    /* }; */

    /*** JTAG CTRL submodule ***/
    /*!
     * \brief Low priority
     */

    /*!
     *  \brief Read individual SCA ADC sensor
     *
     *  \param \c ohMask specifies which \c OptoHybrids to read from
     *  \param \c ch name of SCA ADC sensor to read
     *
     *  \returns \c std::vector<uint32_t> containing a word for each read ADC with the following format
     *  	 ADC data is returned as an array of 32-bit words formatted as:
     *  	 bits [31:29]: constant 0's
     *  	 bit  [28]   : data present
     *  	 bits [27:24]: link ID
     *  	 bits [23:21]: constant 0's
     *  	 bits [20:16]: ADC channel ID
     *  	 bits [15:12]: constant 0's
     *  	 bits [11:0] : ADC data
     */
    struct readSCAADCSensor : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const sca::ADCChannel& ch, const uint16_t& ohMask=0xfff) const;
    };

    /*!
     *  \brief Read all SCA ADC temperature sensors. They are 0x00, 0x04, 0x07, and 0x08.
     *
     *  \param \c ohMask specifies which \c OptoHybrids to read from
     *
     *  \returns \c std::vector<uint32_t> containing a word for each read ADC with the following format
     *  	 ADC data is returned as an array of 32-bit words formatted as:
     *  	 bits [31:29]: constant 0's
     *  	 bit  [28]   : data present
     *  	 bits [27:24]: link ID
     *  	 bits [23:21]: constant 0's
     *  	 bits [20:16]: ADC channel ID
     *  	 bits [15:12]: constant 0's
     *  	 bits [11:0] : ADC data
     */
    struct readSCAADCTemperatureSensors : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
    };

    /*!
     *  \brief Read all SCA ADC voltages sensors. They are 1B, 1E, 11, 0E, 18 and 0F.
     *
     *  \param \c ohMask specifies which \c OptoHybrids to read from
     *
     *  \returns \c std::vector<uint32_t> containing a word for each read ADC with the following format
     *  	 ADC data is returned as an array of 32-bit words formatted as:
     *  	 bits [31:29]: constant 0's
     *  	 bit  [28]   : data present
     *  	 bits [27:24]: link ID
     *  	 bits [23:21]: constant 0's
     *  	 bits [20:16]: ADC channel ID
     *  	 bits [15:12]: constant 0's
     *  	 bits [11:0] : ADC data
     */
    struct readSCAADCVoltageSensors : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
    };

    /*!
     *  \brief Read the SCA ADC signal strength sensors. They are 15, 13 and 12.
     *
     *  \param \c ohMask specifies which \c OptoHybrids to read from
     *
     *  \returns \c std::vector<uint32_t> containing a word for each read ADC with the following format
     *  	 ADC data is returned as an array of 32-bit words formatted as:
     *  	 bits [31:29]: constant 0's
     *  	 bit  [28]   : data present
     *  	 bits [27:24]: link ID
     *  	 bits [23:21]: constant 0's
     *  	 bits [20:16]: ADC channel ID
     *  	 bits [15:12]: constant 0's
     *  	 bits [11:0] : ADC data
     */
    struct readSCAADCSignalStrengthSensors : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
    };

    /*!
     *  \brief Read all connected SCA ADC sensors.
     *
     *  \param \c ohMask specifies which \c OptoHybrids to read from
     *
     *  \returns \c std::vector<uint32_t> containing a word for each read ADC with the following format
     *  	 ADC data is returned as an array of 32-bit words formatted as:
     *  	 bits [31:29]: constant 0's
     *  	 bit  [28]   : data present
     *  	 bits [27:24]: link ID
     *  	 bits [23:21]: constant 0's
     *  	 bits [20:16]: ADC channel ID
     *  	 bits [15:12]: constant 0's
     *  	 bits [11:0] : ADC data
     */
    struct readAllSCAADCSensors : public xhal::common::rpc::Method
    {
      std::vector<uint32_t> operator()(const uint16_t& ohMask=0xfff) const;
    };

  }
}

#endif
