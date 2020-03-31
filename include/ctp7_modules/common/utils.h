/*! \file utils.h
 *  \brief util methods for RPC modules running on a Zynq
 *  \author Mykhailo Dalchenko <mykhailo.dalchenko@cern.ch>
 *  \author Brian Dorney <brian.l.dorney@cern.ch>
 */

#ifndef COMMON_UTILS_H
#define COMMON_UTILS_H

#include "xhal/common/rpc/common.h"

#include <ostream>
#include <iomanip>
#include <string>

namespace utils {

    /*!
     *  \brief Contains information stored in the address table for a given register node
     */
    struct RegInfo
    {
        std::string permissions; ///< Named register permissions: r,w,rw
        std::string mode;        ///< Named register mode: s(ingle),b(lock)
        uint32_t    address;     ///< Named register address
        uint32_t    mask;        ///< Named register mask
        uint32_t    size;        ///< Named register size, in 32-bit words

        /*!
         *  \brief With its intrusive serializer
         */
        template<class Message> void serialize(Message & msg) {
            msg & permissions & mode & address & mask & size;
        }

        friend std::ostream& operator<<(std::ostream &o, RegInfo const& r) {
            o << "0x" << std::hex << std::setw(8) << std::setfill('0') << r.address << std::dec << "  "
              << "0x" << std::hex << std::setw(8) << std::setfill('0') << r.mask    << std::dec << "  "
              << "0x" << std::hex << std::setw(8) << std::setfill('0') << r.size    << std::dec << "  "
              << r.mode << "  " << r.permissions;
            return o;
        }
    };

    /*!
     *  \brief Updates the LMDB object
     *
     *  \param \c at_xml is the name of the XML address table to use to populate the LMDB
     */
    struct update_address_table : public xhal::common::rpc::Method
    {
        void operator()(const std::string& at_xml) const;
    };

    /*!
     *  \brief Read register information from LMDB
     *
     *  \param \c regName Name of node to read from DB
     *
     *  \returns \c RegInfo object with the properties of the found node
     */
    struct readRegFromDB : public xhal::common::rpc::Method
    {
        RegInfo operator()(const std::string& regName) const;
    };

    /*!
     *  \brief Reads a value from remote register. Exported method. Register mask is applied. An \c std::runtime_error is thrown if the register cannot be read.
     *
     *  \param \c regName Register name
     *
     *  \returns \c uint32_t register value
     */
    struct [[deprecated]] readRemoteReg : public xhal::common::rpc::Method
    {
        uint32_t operator()(const std::string& regName) const;
    };

    /*!
     *  \brief Writes a value to remote register. Exported method. Register mask is applied. An \c std::runtime_error is thrown if the register cannot be read.
     *
     *  \param \c regName Register name
     *
     *  \param \c value Value to write
     */
    struct [[deprecated]] writeRemoteReg : public xhal::common::rpc::Method
    {
        void operator()(const std::string& regName, const uint32_t& value) const;
    };
}
#endif
