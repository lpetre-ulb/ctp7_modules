/*! \file include/amc/blaster_ram.h
 *  \brief AMC BLASTER RAM methods for RPC modules
 *  \author Jared Sturdy <sturdy@cern.ch>
 */

#ifndef COMMON_AMC_BLASTER_RAM_H
#define COMMON_AMC_BLASTER_RAM_H

#include "ctp7_modules/common/amc/blaster_ram_defs.h"

#include "xhal/common/rpc/common.h"

#include <vector>

namespace amc {
    namespace blaster {
        /*!
         *  \brief Returns the size of the specified RAM in the BLASTER module
         *
         *  \throws \c std::runtime_error if \c type is not one of the allowed values
         *
         *  \param \c type Select which RAM to obtain the size of, allowed values are:
         *           * \c BLASTERType::GBT
         *           * \c BLASTERType::OptoHybrid
         *           * \c BLASTERType::VFAT
         *           * \c BLASTERType::ALL
         *
         *  \returns size of the RAM in 32-bit words
         */
        struct getRAMMaxSize : public xhal::common::rpc::Method
        {
            uint32_t operator()(BLASTERType const& type) const;
        };

        /*!
         *  \brief Reads configuration \c BLOB from BLASTER RAM
         *
         *  The CTP7 has three RAMs that store configuration information for \c GBT, \c OptoHybrid, and \c VFAT
         *  The \c BLOB returned here will contain the configuration blob for one (or all) of the three RAMs
         *
         *  \throws \c std::runtime_error if:
         *            * \c blob_sz does not match the size of the \c type specified
         *            * \c type is not one of the allowed values
         *
         *  \param \c type specifies which of the RAMs to read, options include:
         *           * \c BLASTERType::GBT - will return all GBT configurations for a single CTP7 BLASTER RAM
         *           * \c BLASTERType::OptoHybrid - will return all OptoHybrid configurations for a single CTP7 BLASTER RAM
         *           * \c BLASTERType::VFAT - will return all VFAT configurations for a single CTP7 BLASTER RAM
         *           * \c BLASTERType::ALL - will return the full configuration of a single CTP7 BLASTER RAM
         *  \param \c blob_sz number of 32-bit words in configuration \c BLOB.
         *         Must be equal to the size of the RAM specified:
         *           * if the size is more, an error will be thrown
         *           * if the size is less, only the number of words specified will be read from the RAM (maybe)
         *           * if ALL is specified, the size must be the total RAM size
         *             - GBT_RAM_SIZE*N_GBTX*N_OH + OH_RAM_SIZE*N_OH + VFAT_RAM_SIZE*N_VFAT*N_OH
         *         Must not exceed GEM_AMC.CONFIG_BLASTER.STATUS.GBT_RAM_SIZE +
         *                         GEM_AMC.CONFIG_BLASTER.STATUS.OH_RAM_SIZE +
         *                         GEM_AMC.CONFIG_BLASTER.STATUS.VFAT_RAM_SIZE
         *
         *  \returns \c std::vector<uint32_t> of the read configuration
         */
        struct readConfRAM : public xhal::common::rpc::Method
        {
            std::vector<uint32_t> operator()(BLASTERType const& type, size_t const& blob_sz) const;
        };

        /*!
         *  \brief Reads GBT configuration \c BLOB from BLASTER GBT_RAM for specified OptoHybrid (3 GBT BLOBs)
         *
         *  \detail The CTP7 has a RAM that stores configuration information for all GBTxs connected to the card.
         *          The \c BLOB is a sequence of 8-bit register values for each GBT register (366 total).
         *          The 8 bit configuration for GBT register 0 should be written to the lowest byte.
         *          Each subsequent register fills the next byte.
         *          GBT0 for OH0 is first, followed by GBT1, and then GBT2.
         *          This is repeated for OH1...OHN, or as specified in the \c ohMask
         *
         *  \param \c ohMask links for which to fill the configuration.
         *         If a link is not specified, that portion of the RAM will be skipped
         *         WARNING: \c ohMask assumes that the BLOB structure skips the masked links
         *         Default value is 0xfff, for all GE1/1 OptoHybrids, a value of 0x0 will be treated the same
         *         Other values in the range (0x0,0xfff) will be treated as described
         *
         *  \returns \c std::vector<uint32_t> of the read GBT BLOB words as 32-bit words
         */
        struct readGBTConfRAM : public xhal::common::rpc::Method
        {
            std::vector<uint32_t> operator()(const uint16_t &ohMask=0xfff) const;
        };

        /*!
         *  \brief Reads OptoHybrid configuration \c BLOB from BLASTER OH_RAM
         *
         *  \detail The CTP7 has a RAM that stores all configuration information for all OptoHybrids connected to the card.
         *          The \c BLOB is a sequence of pairs of 32-bit register addresses followed by 32-bit register values for each OH register.
         *          The local OH address for the first register in OH0 is written to the lowest 32 bits, followed by the value to be written to that register.
         *          Subsequent bits are allocated for the subsequent address/value pairs, and then repeated by the same for OH1...OHN, or as specified in the \c ohMask
         *
         *  \param \c ohMask links for which to fill the configuration.
         *         If a link is not specified, that portion of the RAM will be skipped
         *         WARNING: \c ohMask assumes that the BLOB structure skips the masked links
         *         Default value is 0xfff, for all GE1/1 OptoHybrids, a value of 0x0 will be treated the same
         *         Other values in the range (0x0,0xfff) will be treated as described
         *
         *  \returns \c std::vector<uint32_t> of the read OptoHybrid BLOB words as 32-bit words
         */
        struct readOptoHybridConfRAM : public xhal::common::rpc::Method
        {
            std::vector<uint32_t> operator()(const uint16_t &ohMask=0xfff) const;
        };


        /*!
         *  \brief Reads VFAT configuration \c BLOB from BLASTER VFAT_RAM for specified OptoHybrid (24 VFAT BLOBs)
         *
         *  \detail The CTP7 has a RAM that stores configuration information for all VFATs connected to the card.
         *          The \c BLOB is a sequence of 16-bit register values for each VFAT register (147 total, the MS16-bits are ignored, but must be sent).
         *          The 16 bit configuration for VFAT0 register 0 should be written to the 16 lowest bits.
         *          Each subsequent register fills the next 16 bits, until register 147, which should then be followed by 16 0's
         *          This is then repeated for OH1...OHN, or as specified in the \c ohMask
         *
         *  \param \c ohMask links for which to fill the configuration.
         *         If a link is not specified, that portion of the RAM will be skipped/skipped?
         *         WARNING: \c ohMask assumes that the BLOB structure skips the masked links
         *         Default value is 0xfff, for all GE1/1 OptoHybrids, a value of 0x0 will be treated the same
         *         Other values in the range (0x0,0xfff) will be treated as described
         *
         *  \returns \c std::vector<uint32_t> of the read VFAT BLOB words as 32-bit words
         */
        struct readVFATConfRAM : public xhal::common::rpc::Method
        {
            std::vector<uint32_t> operator()(const uint16_t &ohMask=0xfff) const;
        };


        /*!
         *  \brief Writes configuration \c BLOB to BLASTER RAM
         *
         *  The CTP7 has three RAMs that store configuration information for \c GBT, \c OptoHybrid, and \c VFAT
         *  The \c BLOB provided here contains the configuration blob for one (or all) of the three RAMs
         *
         *  \throws \c std::runtime_error if:
         *            * the requested RAM is not known
         *            * the size of the BLOB does not match the size of the requested RAM
         *            * the data of the BLOB is not valid (deprecated)
         *
         *  \param \c type specifies which of the RAMs to write, options include:
         *         \c BLASTERType::GBT - will write all GBT configurations for a single CTP7 BLASTER RAM
         *         \c BLASTERType::OptoHybrid - will write all OptoHybrid configurations for a single CTP7 BLASTER RAM
         *         \c BLASTERType::VFAT - will write all VFAT configurations for a single CTP7 BLASTER RAM
         *         \c BLASTERType::ALL - will write the full configuration of a single CTP7 BLASTER RAM
         *  \param \c blob configuration \c BLOB
         *         The size must be equal to the size of the RAM specified:
         *         * if the size is more, an error will be thrown
         *         * if the size is less, only the number of words specified will be written to the RAM (maybe)
         *         * if ALL is specified, the size must be the total RAM size
         *           - GBT_RAM_SIZE*N_GBTX*N_OH + OH_RAM_SIZE*N_OH + VFAT_RAM_SIZE*N_VFAT*N_OH
         *         Must not exceed GEM_AMC.CONFIG_BLASTER.STATUS.GBT_RAM_SIZE +
         *                         GEM_AMC.CONFIG_BLASTER.STATUS.OH_RAM_SIZE +
         *                         GEM_AMC.CONFIG_BLASTER.STATUS.VFAT_RAM_SIZE
         */
        struct writeConfRAM : public xhal::common::rpc::Method
        {
            void operator()(BLASTERType const& type, const std::vector<uint32_t>& blob) const;
        };

        /*!
         *  \brief Writes configuration \c BLOB to BLASTER GBT_RAM
         *
         *  \detail The CTP7 has a RAM that stores configuration information for all GBTxs connected to the card.
         *          The \c BLOB is a sequence of 8-bit register values for each GBT register (366 total).
         *          The 8 bit configuration for GBT register 0 should be written to the lowest byte.
         *          Each subsequent register fills the next byte.
         *          GBT0 for OH0 is first, followed by GBT1, and then GBT2.
         *          This is repeated for OH1...OHN, or as specified in the \c ohMask
         *
         *  \throws \c std::range_error if the size of the BLOB is larger than the size of the GBT RAM
         *
         *  \param \c gbtblob GBT configuration \c BLOB corresponding to all GBTs on all listed links
         *         Size should be equal to GBT_RAM_SIZE*N_GBTX*N_OH
         *         Size should not exceed GEM_AMC.CONFIG_BLASTER.STATUS.GBT_RAM_SIZE
         *  \param \c ohMask links for which to fill the configuration.
         *         If a link is not specified, that portion of the RAM will be skipped
         *         WARNING: \c ohMask assumes that the BLOB structure skips the masked links
         *         Default value is 0xfff, for all GE1/1 OptoHybrids, a value of 0x0 will be treated the same
         *         Other values in the range (0x0,0xfff) will be treated as described
         */
        struct writeGBTConfRAM : public xhal::common::rpc::Method
        {
          void operator()(const std::vector<uint32_t>& gbtblob, const uint16_t& ohMask=0xfff) const;
        };

        /*!
         *  \brief Writes configuration \c BLOB to BLASTER OH_RAM
         *
         *  \detail The CTP7 has a RAM that stores all configuration information for all OptoHybrids connected to the card.
         *          The \c BLOB is a sequence of pairs of 32-bit register addresses followed by 32-bit register values for each OH register.
         *          The local OH address for the first register in OH0 is written to the lowest 32 bits, followed by the value to be written to that register.
         *          Subsequent bits are allocated for the subsequent address/value pairs, and then repeated by the same for OH1...OHN, or as specified in the \c ohMask
         *
         *  \throws \c std::range_error if the size of the BLOB is larger than the size of the OptoHybrid RAM
         *
         *  \param \c ohblob OptoHybrid configuration \c BLOB corresponding to all OptoHybrids on all listed links
         *         Size should be equal to OH_RAM_SIZE*N_OH
         *         Size should not exceed GEM_AMC.CONFIG_BLASTER.STATUS.OH_RAM_SIZE
         *  \param \c ohMask links for which to fill the configuration.
         *         If a link is not specified, that portion of the RAM will be skipped
         *         WARNING: \c ohMask assumes that the BLOB structure skips the masked links
         *         Default value is 0xfff, for all GE1/1 OptoHybrids, a value of 0x0 will be treated the same
         *         Other values in the range (0x0,0xfff) will be treated as described
         */
        struct writeOptoHybridConfRAM : public xhal::common::rpc::Method
        {
            void operator()(const std::vector<uint32_t>& ohblob, const uint16_t& ohMask=0xfff) const;
        };


        /*!
         *  \brief Writes configuration \c BLOB to BLASTER VFAT_RAM
         *
         *  \detail The CTP7 has a RAM that stores configuration information for all VFATs connected to the card.
         *          The \c BLOB is a sequence of 16-bit register values for each VFAT register (147 total, the MS16-bits are ignored, but must be sent).
         *          The 16 bit configuration for VFAT0 register 0 should be written to the 16 lowest bits.
         *          Each subsequent register fills the next 16 bits, until register 147, which should then be followed by 16 0's
         *          This is then repeated for OH1...OHN, or as specified in the \c ohMask
         *
         *  \throws \c std::range_error if the size of the BLOB is larger than the size of the VFAT RAM
         *
         *  \param \c vfatblob VFAT configuration \c BLOB corresponding to all VFATs on all listed links
         *         Size should be equal to VFAT_RAM_SIZE*N_VFAT*N_OH
         *         Size should not exceed GEM_AMC.CONFIG_BLASTER.STATUS.VFAT_RAM_SIZE
         *  \param \c ohMask links for which to fill the configuration.
         *         If a link is not specified, that portion of the RAM will be skipped/skipped?
         *         WARNING: \c ohMask assumes that the BLOB structure skips the masked links
         *         Default value is 0xfff, for all GE1/1 OptoHybrids, a value of 0x0 will be treated the same
         *         Other values in the range (0x0,0xfff) will be treated as described
         */
        struct writeVFATConfRAM : public xhal::common::rpc::Method
        {
          void operator()(const std::vector<uint32_t>& vfatblob, const uint16_t& ohMask=0xfff) const;
        };
    }
}

#endif
