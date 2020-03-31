/*! \file expert_tools.h
 *  \brief Low-level RPC methods exported for the expert tools
 *  \author Laurent Pétré <laurent.petre@cern.ch>
 */

#ifndef COMMON_EXPERT_TOOLS_H
#define COMMON_EXPERT_TOOLS_H

#include "xhal/common/rpc/common.h"

namespace expert {
    /*!
     *  \brief Reads a value from remote address. An \c std::runtime_error is thrown if the register cannot be read.
     *
     *  \param \c address Register address
     *  \returns \c uint32_t register value
     */
    struct readRawAddress : public xhal::common::rpc::Method
    {
        uint32_t operator()(const uint32_t& address) const;
    };

    /*!
     *  \brief Writes a value to remote address. An \c std::runtime_error is thrown if the register cannot be written.
     *
     *  \param \c address Register address
     *  \param \c value Value to write
     */
    struct writeRawAddress : public xhal::common::rpc::Method
    {
        void operator()(const uint32_t& address, const uint32_t& value) const;
    };
}
#endif
