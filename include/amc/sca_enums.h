#ifndef SCA_ENUMS_H
#define SCA_ENUMS_H

class SCASettings {
 public:

  /**
   * \brief SCA Resets
   * Types of resets able to be sent via the SCA
   */
  struct SCAResetType {
    enum ESCAResetType {
      SCAReset  = 0x0, ///< Reset the SCA
      HardReset = 0x1, ///< Use the SCA to send a TTC-style HardReset command
    } SCAResetType;
  };

  /**
   * \brief SCA channels
   * List of channels with which the SCA can be controlled
   */
  struct SCAChannel {
    enum ESCAChannel {
      CTRL  = 0x00, ///< SCA configuration registers
      SPI   = 0x01, ///< Serial peripheral master interface, disconnected in GEM OH
      GPIO  = 0x02, ///< General purpose parallel I/O interface
      I2C00 = 0x03, ///< I2C serial interface - master 0
      I2C01 = 0x04, ///< I2C serial interface - master 1
      I2C02 = 0x05, ///< I2C serial interface - master 2
      I2C03 = 0x06, ///< I2C serial interface - master 3
      I2C04 = 0x07, ///< I2C serial interface - master 4
      I2C05 = 0x08, ///< I2C serial interface - master 5
      I2C06 = 0x09, ///< I2C serial interface - master 6
      I2C07 = 0x0a, ///< I2C serial interface - master 7
      I2C08 = 0x0b, ///< I2C serial interface - master 8
      I2C09 = 0x0c, ///< I2C serial interface - master 9
      I2C10 = 0x0d, ///< I2C serial interface - master 10
      I2C11 = 0x0e, ///< I2C serial interface - master 11
      I2C12 = 0x0f, ///< I2C serial interface - master 12
      I2C13 = 0x10, ///< I2C serial interface - master 13
      I2C14 = 0x11, ///< I2C serial interface - master 14
      I2C15 = 0x12, ///< I2C serial interface - master 15
      JTAG  = 0x13, ///< JTAG serial master interface
      ADC   = 0x14, ///< Analog-to-digital converter
      DAC   = 0x15, ///< Digital-to-analog converter, disconnected in GEM OH
    } SCAChannel;
  };  // struct SCAChannel

  /**
   * \brief SCA control commands
   * Command aliases that can be used when controlling the SCA generic control interface
   */
  struct CTRLCommand {  //CTRLCommand settings
    enum ECTRLCommand { //CTRLCommand settings
      GET_DATA     = 0x00, ///< Read the data
      CTRL_R_ID_V1 = 0x91, ///< Read the SCA ChipID (channel 0x14)
      CTRL_R_ID_V2 = 0xD1, ///< Read the SCA ChipID (channel 0x14)
      CTRL_W_CRB   = 0x02, ///< Write to CTRL register B
      CTRL_W_CRC   = 0x04, ///< Write to CTRL register C
      CTRL_W_CRD   = 0x06, ///< Write to CTRL register D
      CTRL_R_CRB   = 0x03, ///< Read from CTRL register B
      CTRL_R_CRC   = 0x05, ///< Read from CTRL register C
      CTRL_R_CRD   = 0x07, ///< Read from CTRL register D
      CTRL_R_SEU   = 0xF1, ///< Read from the SEU monitor (channel 0x13)
      CTRL_C_SEU   = 0xF0, ///< Reset the SEU monitor (channel 0x13)
    } CTRLCommand;
  };

  /**
   * \brief SCA I2C commands
   * Command aliases that can be used when controlling the I2C interface of the SCA
   */
  struct I2CCommand {  //I2CCommand settings
    enum EI2CCommand { //I2CCommand settings
      // I2C commands
      I2C_W_CTRL   = 0x30, ///< Write I2C control register
      I2C_R_CTRL   = 0x31, ///< Read I2C control register
      I2C_R_STR    = 0x11, ///< Read I2C status register
      I2C_W_MSK    = 0x20, ///< Write I2C mask register
      I2C_R_MSK    = 0x21, ///< Read I2C mask register
      I2C_W_DATA0  = 0x40, ///< Write I2C data register bytes [3:0]
      I2C_R_DATA0  = 0x41, ///< Read I2C data register bytes [3:0]
      I2C_W_DATA1  = 0x50, ///< Write I2C data register bytes [7:4]
      I2C_R_DATA1  = 0x51, ///< Read I2C data register bytes [7:4]
      I2C_W_DATA2  = 0x60, ///< Write I2C data register bytes [11:8]
      I2C_R_DATA2  = 0x61, ///< Read I2C data register bytes [11:8]
      I2C_W_DATA3  = 0x70, ///< Write I2C data register bytes [15:12]
      I2C_R_DATA3  = 0x71, ///< Read I2C data register bytes [15:12]
      I2C_S_7B_W   = 0x82, ///< Start I2C single-byte write with 7-bit address
      I2C_S_7B_R   = 0x86, ///< Start I2C single-byte read with 7-bit address
      I2C_S_10B_W  = 0x8A, ///< Start I2C single-byte write with 10-bit address
      I2C_S_10B_R  = 0x8E, ///< Start I2C single-byte read with 10-bit address
      I2C_M_7B_W   = 0xDA, ///< Start I2C multi-byte write with 7-bit address
      I2C_M_7B_R   = 0xDE, ///< Start I2C multi-byte read with 7-bit address
      I2C_M_10B_W  = 0xE2, ///< Start I2C multi-byte write with 10-bit address
      I2C_M_10B_R  = 0xE6, ///< Start I2C multi-byte read with 10-bit address
      /* I2C_RMW_AND  = 0xXX, ///< Start RMW transaction with AND */
      I2C_RMW_OR   = 0xC6, ///< Start RMW transaction with OR
      I2C_RMW_XOR  = 0xCA, ///< Start RMW transaction with XOR
    } I2CCommand;
  };

  /**
   * \brief SCA GPIO commands
   * Command aliases that can be used when controlling the GPIO interface of the SCA
   */
  struct GPIOCommand {  //GPIOCommand settings
    enum EGPIOCommand { //GPIOCommand settings
      // GPIO commands
      GPIO_W_DATAOUT   = 0x10, ///< Write GPIO DATAOUT register, length 4, read length 2
      GPIO_R_DATAOUT   = 0x11, ///< Read GPIO DATAOUT register, length 1, read length 4
      GPIO_R_DATAIN    = 0x01, ///< Read GPIO DATAIN register, length 1, read length 4
      GPIO_W_DIRECTION = 0x20, ///< Write GPIO direction register, length 4, read length 2
      GPIO_R_DIRECTION = 0x21, ///< Read GPIO direction register, length 1, read length 4
      GPIO_W_INTENABLE = 0x60, ///< Write GPIO interrupt enable register, length 4, read length 2
      GPIO_R_INTENABLE = 0x61, ///< Read GPIO interrupt enable register, length 4, read length 2
      GPIO_W_INTSEL    = 0x30, ///< Write GPIO interrupt line select register, length 4, read length 2
      GPIO_R_INTSEL    = 0x31, ///< Read GPIO interrupt line select register, length 4, read length 2
      GPIO_W_INTTRIG   = 0x40, ///< Write GPIO edge select for interrupt register, length 4, read length 2
      GPIO_R_INTTRIG   = 0x41, ///< Read GPIO edge select for interrupt register, length 4, read length 2
      GPIO_W_INTS      = 0x70, ///< Write GPIO interrupt vector register, length 4, read length 2
      GPIO_R_INTS      = 0x71, ///< Read GPIO interrupt vector register, length 4, read length 2
      GPIO_W_CLKSEL    = 0x80, ///< Write GPIO clock select register, length 4, read length 2
      GPIO_R_CLKSEL    = 0x81, ///< Read GPIO clock select register, length 4, read length 2
      GPIO_W_EDGESEL   = 0x90, ///< Write GPIO clock edge select register, length 4, read length 2
      GPIO_R_EDGESEL   = 0x91, ///< Read GPIO clock edge select register, length 4, read length 2
    } GPIOCommand;
  };

  /**
   * \brief SCA ADC commands
   * Command aliases that can be used when controlling the ADC interface of the SCA
   */
  struct ADCCommand {  //ADCCommand settings
    enum EADCCommand { //ADCCommand settings
      // ADC commands V2
      ADC_GO     = 0x02, ///< [SCA V2 only] Start of ADC conversion
      ADC_W_MUX  = 0x50, ///< [SCA V2 only] Write ADC register INSEL
      ADC_R_MUX  = 0x51, ///< [SCA V2 only] Read ADC register INSEL
      ADC_W_CURR = 0x60, ///< [SCA V2 only] Write ADC register
      ADC_R_CURR = 0x61, ///< [SCA V2 only] Read ADC register
      ADC_W_GAIN = 0x10, ///< [SCA V2 only] Write ADC register output A
      ADC_R_GAIN = 0x11, ///< [SCA V2 only] Read ADC register output A
      ADC_R_DATA = 0x21, ///< [SCA V2 only] Read ADC converted value
      ADC_R_RAW  = 0x31, ///< [SCA V2 only] Read ADC raw value
      ADC_R_OFS  = 0x41, ///< [SCA V2 only] Read ADC offset
      // ADC commands V1??
      ADC_V1_GO       = 0xB2, ///< [SCA V1 only] Start of ADC conversion
      ADC_V1_W_INSEL  = 0x30, ///< [SCA V1 only] Write ADC register INSEL
      ADC_V1_R_INSEL  = 0x31, ///< [SCA V1 only] Read ADC register INSEL
      ADC_V1_W_CURREN = 0x40, ///< [SCA V1 only] Write ADC register
      ADC_V1_R_CURREN = 0x41, ///< [SCA V1 only] Read ADC register
    } ADCCommand;
  };  // struct ADCCommand

  /**
   * \brief SCA Error Flags
   * List of error flags that can be set in the reply
   */
  struct SCAErrorFlags {  //SCAErrorFlags settings
    enum ESCAErrorFlags { //SCAErrorFlags settings
      Generic     = 0x01, ///< Generic error flag
      InvChReq    = 0x02, ///< Invalid channel requested
      InvCmdReq   = 0x04, ///< Invalid command requested
      InvTranReqN = 0x08, ///< Invalid transaction request number
      InvLen      = 0x10, ///< Invalid length
      ChNotEn     = 0x20, ///< Channel not enabled
      ChBusy      = 0x40, ///< Channel busy
      CmdInTreat  = 0x80, ///< Command in treatment
    } SCAErrorFlags;
  };  // struct SCAErrorFlags

  /**
   * \brief SCA I2C channel
   * List of I2C channel aliases on the SCA
   */
  struct I2CChannel {  //I2CChannel settings
    enum EI2CChannel { //I2CChannel settings
      I2C00 = 0x03, ///< I2C serial interface - master 0
      I2C01 = 0x04, ///< I2C serial interface - master 1
      I2C02 = 0x05, ///< I2C serial interface - master 2
      I2C03 = 0x06, ///< I2C serial interface - master 3
      I2C04 = 0x07, ///< I2C serial interface - master 4
      I2C05 = 0x08, ///< I2C serial interface - master 5
      I2C06 = 0x09, ///< I2C serial interface - master 6
      I2C07 = 0x0a, ///< I2C serial interface - master 7
      I2C08 = 0x0b, ///< I2C serial interface - master 8
      I2C09 = 0x0c, ///< I2C serial interface - master 9
      I2C10 = 0x0d, ///< I2C serial interface - master 10
      I2C11 = 0x0e, ///< I2C serial interface - master 11
      I2C12 = 0x0f, ///< I2C serial interface - master 12
      I2C13 = 0x10, ///< I2C serial interface - master 13
      I2C14 = 0x11, ///< I2C serial interface - master 14
      I2C15 = 0x12, ///< I2C serial interface - master 15
    } I2CChannel;
  };  // struct I2CChannel

  /**
   * \brief SCA GPIO channel
   * List of GPIO channels on the SCA
   */
  struct GPIOChannel {  //GPIOChannel settings
    enum EGPIOChannel { //GPIOChannel settings
    } GPIOChannel;
  };  // struct GPIOChannel

  /**
   * \brief SCA ADC channel
   * List of ADC channels on the SCA
   */
  struct ADCChannel {  //ADCChannel settings
    enum EADCChannel { //ADCChannel settings
      // ADC 12-bit range
      // * Voltage 0-1V (1V/0xfff) LSB, offset 0V
      // * Temperature -30-80C (110C/0xfff) LSB, offset -30C
      AVCCN_V1P0     = 27, ///< FPGA MGT 1.0V
      AVTTN_V1P2     = 30, ///< FPGA MGT 1.2V
      INT_V1P0       = 17, ///< 1.0V FPGA core voltage
      ADC_V1P8       = 14, ///< 1.8V used by the PROM
      ADC_V1P5       = 24, ///< 1.5V used by the GBTXs and SCA
      ADC_2V5        = 15, ///< 2.5V used by FPGA I/O and VTRXs/VTTXs
      SCA_TEMP       = 31, ///< Internal SCA temperature sensor

      // Aliases
      FPGA_MGT_V1P0  = 27, ///< FPGA MGT 1.0V
      FPGA_MGT_V1P2  = 30, ///< FPGA MGT 1.2V
      FPGA_CORE      = 17, ///< 1.0V FPGA core voltage
      PROM_V1P8      = 14, ///< 1.8V used by the PROM
      GBTX_V1P5      = 24, ///< 1.5V used by the GBTXs and SCA
      SCA_V1P5       = 24, ///< 1.5V used by the GBTXs and SCA
      FPGA_IO_V2P5   = 15, ///< 2.5V used by FPGA I/O and VTRXs/VTTXs
      VTTX_VTRX_V2P5 = 15, ///< 2.5V used by FPGA I/O and VTRXs/VTTXs

      // SCA ADC temperature sensors
      VTTX_CSC_PT100 = 0x00, ///< Transceiver block next to the CSC VTTX
      VTTX_GEM_PT100 = 0x04, ///< Transceiver block next to the GEM VTTX
      GBT0_PT100     = 0x07, ///< SCA temperature sensor
      V6_FPGA_PT100  = 0x08, ///< Virtex6 temperature sensor

      // SCA ADC signal strength sensors
      VTRX_RSSI3     = 0x12, ///< ADC channel 18, VTRx rssi3
      VTRX_RSSI2     = 0x13, ///< ADC channel 19, VTRx rssi2
      VTRX_RSSI1     = 0x15, ///< ADC channel 21, VTRx rssi1

      // Not connected SCA ADC sensors
      //ADC_CH01 = 0x01, ///< ADC channel 01, NOT CONNECTED
      //ADC_CH02 = 0x02, ///< ADC channel 02, NOT CONNECTED
      //ADC_CH03 = 0x03, ///< ADC channel 03, NOT CONNECTED
      //ADC_CH05 = 0x05, ///< ADC channel 05, NOT CONNECTED
      //ADC_CH09 = 0x09, ///< ADC channel 09
      //ADC_CH10 = 0x0A, ///< ADC channel 10
      //ADC_CH11 = 0x0B, ///< ADC channel 11
      //ADC_CH12 = 0x0C, ///< ADC channel 12, NOT CONNECTED
      //ADC_CH13 = 0x0D, ///< ADC channel 13
      //ADC_CH16 = 0x10, ///< ADC channel 16
      //ADC_CH20 = 0x14, ///< ADC channel 20
      //ADC_CH22 = 0x16, ///< ADC channel 22
      //ADC_CH23 = 0x17, ///< ADC channel 23
      //ADC_CH25 = 0x19, ///< ADC channel 25
      //ADC_CH26 = 0x1A, ///< ADC channel 26
      //ADC_CH28 = 0x1C, ///< ADC channel 28
      //ADC_CH29 = 0x1D, ///< ADC channel 29
    } ADCChannel;

    //static constexpr bool useCurrentSource(EADCChannel sensor)
    static bool useCurrentSource(EADCChannel sensor)
    {
        switch (sensor) {
            case (VTTX_CSC_PT100):	return true;
            case (VTTX_GEM_PT100):	return true;
            case (GBT0_PT100):		return true;
            case (PROM_V1P8):		return true;
            case (V6_FPGA_PT100):	return true;
            default:			return false;
        }
    }
  };  // struct ADCChannel

};

// <name>  is the enum scoped namespace for scope::VALUE access
// <name>T is the enum type

// typedef the struct for access to the members via struct::VALUE
typedef SCASettings::SCAResetType  SCAResetType;
typedef SCASettings::SCAChannel    SCAChannel;
typedef SCASettings::CTRLCommand   SCACTRLCommand;
typedef SCASettings::I2CCommand    SCAI2CCommand;
typedef SCASettings::GPIOCommand   SCAGPIOCommand;
typedef SCASettings::ADCCommand    SCAADCCommand;
typedef SCASettings::I2CChannel    SCAI2CChannel;
typedef SCASettings::GPIOChannel   SCAGPIOChannel;
typedef SCASettings::ADCChannel    SCAADCChannel;

// typedef the enum for casting and access
typedef SCASettings::SCAResetType::ESCAResetType  SCAResetTypeT;
typedef SCASettings::SCAChannel::ESCAChannel      SCAChannelT;
typedef SCASettings::CTRLCommand::ECTRLCommand    SCACTRLCommandT;
typedef SCASettings::I2CCommand::EI2CCommand      SCAI2CCommandT;
typedef SCASettings::GPIOCommand::EGPIOCommand    SCAGPIOCommandT;
typedef SCASettings::ADCCommand::EADCCommand      SCAADCCommandT;
typedef SCASettings::I2CChannel::EI2CChannel      SCAI2CChannelT;
typedef SCASettings::GPIOChannel::EGPIOChannel    SCAGPIOChannelT;
typedef SCASettings::ADCChannel::EADCChannel      SCAADCChannelT;


#endif
