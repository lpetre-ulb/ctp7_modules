/*! \file
 *  \brief Header containing the hardware related constants.
 *  \author Laurent Pétré <lpetre@ulb.ac.be>
 */

#ifndef HW_CONSTANTS_H
#define HW_CONSTANTS_H

#ifndef GEM_VARIANT
#error You must define the GEM_VARIANT constant.
#endif

#include <stdint.h>
#include <array>

/*! \brief This namespace hold the constants related to the AMC.
 */
namespace amc {
    /*! \brief GE1/1 specific namespace.
     */
    namespace ge11 {
        constexpr uint32_t OH_PER_AMC = 12;    ///< The number of OptoHybrids per AMC.
        constexpr uint32_t FULL_OH_MASK = 0xfff; ///< Reading of all OptoHybrids are enabled.
    }

    /*! \brief GE2/1 specific namespace.
     */
    namespace ge21 {
        constexpr uint32_t OH_PER_AMC = 12;    ///< The number of OptoHybrids per AMC.
    }

    using namespace GEM_VARIANT;
}

/*! \brief This namespace hold the constants related to the OptoHybrid.
 */
namespace oh {
    /*! \brief GE1/1 specific namespace.
     */
    namespace ge11 {
        constexpr uint32_t VFATS_PER_OH     = 24;    ///< The number of VFAT's per OptoHybrid.
        constexpr size_t OH_SINGLE_RAM_SIZE = 2*100; ///< Per-OH RAM size: for GE1/1 100 32-bit words of configuration per OH plus the corresponding OH local address
        constexpr uint32_t CHANNELS_PER_OH  = 3072;  ///< The number of VFAT channels per OptoHybrid
        constexpr uint32_t SBITS_PER_OH     = 1536;  ///< The number of S-bits per OptoHybrid
    }

    /*! \brief GE2/1 specific namespace.
     */
    namespace ge21 {
        constexpr uint32_t VFATS_PER_OH     = 12;    ///< The number of VFAT's per OptoHybrid.
        constexpr size_t OH_SINGLE_RAM_SIZE = 2*100; ///< Per-OH RAM size: for GE2/1 100 32-bit words of configuration per OH plus the corresponding OH local address
        constexpr uint32_t CHANNELS_PER_OH  = 1536;  ///< The number of VFAT channels per OptoHybrid
        constexpr uint32_t SBITS_PER_OH     = 768;   ///< The number of S-bits per OptoHybrid
    }

    namespace triggerLinkMappings {
        namespace ge11 {
            constexpr std::array<std::array<uint8_t, 2>, amc::ge11::OH_PER_AMC> OH_TO_CHANNEL {
                { {62, 63}, {61, 60}, {59, 58}, {57, 56}, {54, 55}, {52, 53}, {50, 51}, {49, 48}, {47, 46}, {45, 44}, {41, 37}, {38, 39} }
            };
        }

        using namespace GEM_VARIANT;
    }

    using namespace GEM_VARIANT;
}

/*! \brief This namespace hold the constants related to the OptoHybrid.
 */
namespace vfat {
    /*! \brief GE1/1 specific namespace.
     */
    namespace ge11 {
        constexpr size_t VFAT_SINGLE_RAM_SIZE = 74; ///< Per-VFAT RAM size: for GE1/1 24 VFATs per OH, 74 32-bit words of configuration per VFAT (147 16-bit VFAT configurations -> 32-bit words + padding)
    }

    /*! \brief GE2/1 specific namespace.
     */
    namespace ge21 {
        constexpr size_t VFAT_SINGLE_RAM_SIZE = 74; ///< Per-VFAT RAM size: for GE2/1 12 VFATs per OH, 74 32-bit words of configuration per VFAT (147 16-bit VFAT configurations -> 32-bit words + padding)
    }

    using namespace GEM_VARIANT;
}

/*! \brief This namespace hold the constants related to the GBT.
 */
namespace gbt {

    constexpr uint16_t CONFIG_SIZE = 366; ///< The size of the GBT configuration address space. The corresponding addresses span from 0 to 365.

    typedef std::array<uint8_t, CONFIG_SIZE> config_t; ///< This type defines a GBT configuration blob.

    constexpr uint8_t PHASE_MIN = 0;  ///< Minimal phase for the elink RX GBT phase.
    constexpr uint8_t PHASE_MAX = 15; ///< Maximal phase for the elink RX GBT phase.

    /*! \brief GE1/1 specific namespace.
     */
    namespace ge11{
        constexpr uint32_t GBTS_PER_OH       = 3;  ///< The number of GBT's per OptoHybrid (should be part of the oh namespace, as VFATS_PER_OH)
        constexpr size_t GBT_SINGLE_RAM_SIZE = 92; ///< Per-GBT RAM size: for GE1/1 3 GBTx per OH, 92 32-bit words of configuration per GBT (366 8-bit GBT configurations -> 32-bit words + padding)
    }

    /*! \brief GE2/1 specific namespace.
     */
    namespace ge21{
        constexpr uint32_t GBTS_PER_OH       = 2;  ///< The number of GBT's per OptoHybrid (should be part of the oh namespace, as VFATS_PER_OH)
        constexpr size_t GBT_SINGLE_RAM_SIZE = 92; ///< Per-GBT RAM size: for GE2/1 2 GBTx per OH, 92 32-bit words of configuration per GBT (366 8-bit GBT configurations -> 32-bit words + padding)
    }

    using namespace GEM_VARIANT;

    /*! \brief Mappings between elinks, GBT index and VFAT index.
     */
    namespace elinkMappings {

        /*! \brief GE1/1 specific namespace.
         */
        namespace ge11 {
            /*! \brief Mapping from VFAT index to GBT index.
             */
            constexpr std::array<uint32_t, 24> VFAT_TO_GBT {
                { 1, 1, 1, 1, 1, 1, 1, 0, 1, 2, 2, 2, 0, 0, 0, 0, 1, 2, 2, 2, 2, 2, 2, 0 }
            };

            /*! \brief Mapping from VFAT index to the elink of its corresponding GBT.
             */
            constexpr std::array<uint8_t, 24> VFAT_TO_ELINK {
                { 5, 9, 2, 3, 1, 8, 6, 6, 4, 1, 5, 4, 3, 2, 1, 0, 7, 8, 6, 7, 2, 3, 9, 8 }
            };

            /*! \brief Mapping from elink index to the 3 registers addresses in the GBT.
	         */
            constexpr std::array<std::array<uint16_t, 3>, 10> ELINK_TO_REGISTERS {
                { {69, 73, 77}, {67, 71, 75}, {93, 97, 101}, {91, 95, 99}, {117, 121, 125}, {115, 119, 123}, {141, 145, 149}, {139, 143, 147}, {165, 169, 173}, {163, 167, 171} }
            };
        }

        /*! \brief GE2/1 specific namespace.
         */
        namespace ge21 {
            /*! \brief Mapping from VFAT index to GBT index.
             */
            constexpr std::array<uint32_t, 12> VFAT_TO_GBT {
                { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1 }
            };

            /*! \brief Mapping from VFAT index to the elink of its corresponding GBT.
             */
            constexpr std::array<uint8_t, 12> VFAT_TO_ELINK {
                { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5 }
            };

            /*! \brief Mapping from elink index to the 3 registers addresses in the GBT.
	         */
            constexpr std::array<std::array<uint16_t, 3>, 10> ELINK_TO_REGISTERS {
                { {69, 73, 77}, {67, 71, 75}, {93, 97, 101}, {91, 95, 99}, {117, 121, 125}, {115, 119, 123}, {141, 145, 149}, {139, 143, 147}, {165, 169, 173}, {163, 167, 171} }
            };
        }

        using namespace GEM_VARIANT;
    }
}

#endif
