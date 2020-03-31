/*! \file
 *  \brief Low-level RPC methods exported for the expert tools
 *  \author Laurent Pétré <lpetre@ulb.ac.be>
 */

#include "ctp7_modules/common/expert_tools.h"

#include "ctp7_modules/server/utils.h"

#include "xhal/common/rpc/register.h"

uint32_t expert::readRawAddress::operator()(const uint32_t& address) const
{
    return utils::readRawAddress(address);
}

void expert::writeRawAddress::operator()(const uint32_t& address, const uint32_t& value) const
{
    return utils::writeRawAddress(address, value);
}

extern "C" {
    const char *module_version_key = "expert_tools v1.0.1";
    const int module_activity_color = 4;

    void module_init(ModuleManager *modmgr)
    {
        utils::initLogging();

        if (memhub_open(&memsvc) != 0) {
            auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
            LOG4CPLUS_ERROR(logger, "Unable to connect to memory service: " << memsvc_get_last_error(memsvc));
            LOG4CPLUS_ERROR(logger, "Unable to load module");
            return;
        }

        xhal::common::rpc::registerMethod<expert::readRawAddress>(modmgr);
        xhal::common::rpc::registerMethod<expert::writeRawAddress>(modmgr);
    }
}
