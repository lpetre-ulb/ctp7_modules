/*!
 * \file amc/daq.cpp
 * \brief AMC DAQ methods for RPC modules
 */

#include "ctp7_modules/common/amc/daq.h"
#include "ctp7_modules/common/amc/ttc.h"
#include "ctp7_modules/common/amc/sca.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"

void amc::daq::enableDAQLink::operator()(const uint32_t& enableMask) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_DEBUG(logger, "enableDAQLinkLocal called");
  // utils::writeReg("GEM_AMC.DAQ.CONTROL.INPUT_ENABLE_MASK", enableMask);
  utils::writeReg("GEM_AMC.DAQ.CONTROL.DAQ_ENABLE", 0x1);
  return;
}

void amc::daq::disableDAQLink::operator()() const
{
  utils::writeReg("GEM_AMC.DAQ.CONTROL.INPUT_ENABLE_MASK", 0x0);
  utils::writeReg("GEM_AMC.DAQ.CONTROL.DAQ_ENABLE",        0x0);
}

void amc::daq::setZS::operator()(const bool& en) const
{
  utils::writeReg("GEM_AMC.DAQ.CONTROL.ZERO_SUPPRESSION_EN", uint32_t(en));
}

void amc::daq::disableZS::operator()() const
{
  utils::writeReg("GEM_AMC.DAQ.CONTROL.ZERO_SUPPRESSION_EN", 0x0);
}

void amc::daq::resetDAQLink::operator()(const uint32_t& davTO, const uint32_t& ttsOverride) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_DEBUG(logger, "resetDAQLinkLocal called");
  utils::writeReg("GEM_AMC.DAQ.CONTROL.RESET", 0x1);
  utils::writeReg("GEM_AMC.DAQ.CONTROL.RESET", 0x0);
  amc::daq::disableDAQLink{}();
  utils::writeReg("GEM_AMC.DAQ.CONTROL.DAV_TIMEOUT", davTO);
  amc::daq::setDAQLinkInputTimeout{}();
  // setDAQLinkInputTimeout{}(davTO);
  // utils::writeReg("GEM_AMC.DAQ.TTS_OVERRIDE", ttsOverride);
  return;
}

uint32_t amc::daq::getDAQLinkControl::operator()() const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_WARN(logger,"getDAQLinkControl not implemented");
  return 0x0;
}

uint32_t amc::daq::getDAQLinkStatus::operator()() const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_WARN(logger,"getDAQLinkStatus not implemented");
  return 0x0;
}

bool amc::daq::daqLinkReady::operator()() const
{
  return static_cast<bool>(utils::readReg("GEM_AMC.DAQ.STATUS.DAQ_LINK_RDY"));
}

bool amc::daq::daqClockLocked::operator()() const
{
  return static_cast<bool>(utils::readReg("GEM_AMC.DAQ.STATUS.DAQ_CLK_LOCKED"));
}

bool amc::daq::daqTTCReady::operator()() const
{
  return static_cast<bool>(utils::readReg("GEM_AMC.DAQ.STATUS.TTC_RDY"));
}

uint8_t amc::daq::daqTTSState::operator()() const
{
  return static_cast<uint8_t>(utils::readReg("GEM_AMC.DAQ.STATUS.TTS_STATE"));
}

bool amc::daq::daqAlmostFull::operator()() const
{
  return static_cast<bool>(utils::readReg("GEM_AMC.DAQ.STATUS.DAQ_AFULL"));
}

bool amc::daq::l1aFIFOIsEmpty::operator()() const
{
  return static_cast<bool>(utils::readReg("GEM_AMC.DAQ.STATUS.L1A_FIFO_IS_EMPTY"));
}

bool amc::daq::l1aFIFOIsAlmostFull::operator()() const
{
  return static_cast<bool>(utils::readReg("GEM_AMC.DAQ.STATUS.L1A_FIFO_IS_NEAR_FULL"));
}

bool amc::daq::l1aFIFOIsFull::operator()() const
{
  return static_cast<bool>(utils::readReg("GEM_AMC.DAQ.STATUS.L1A_FIFO_IS_FULL"));
}

bool amc::daq::l1aFIFOIsUnderflow::operator()() const
{
  return static_cast<bool>(utils::readReg("GEM_AMC.DAQ.STATUS.L1A_FIFO_IS_UNDERFLOW"));
}

uint32_t amc::daq::getDAQLinkEventsSent::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.EXT_STATUS.EVT_SENT");
}

uint32_t amc::daq::getDAQLinkL1AID::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.EXT_STATUS.L1AID");
}

uint32_t amc::daq::getDAQLinkL1ARate::operator()() const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_WARN(logger,"getDAQLinkL1ARate not implemented");
  return 0x0;
}

uint32_t amc::daq::getDAQLinkDisperErrors::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.EXT_STATUS.DISPER_ERR");
}

uint32_t amc::daq::getDAQLinkNonidentifiableErrors::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.EXT_STATUS.NOTINTABLE_ERR");
}

uint32_t amc::daq::getDAQLinkInputMask::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.CONTROL.INPUT_ENABLE_MASK");
}

uint32_t amc::daq::getDAQLinkDAVTimeout::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.CONTROL.DAV_TIMEOUT");
}

uint32_t amc::daq::getDAQLinkDAVTimer::operator()(bool const& max) const
{
  uint32_t maxtimer  = utils::readReg("GEM_AMC.DAQ.EXT_STATUS.MAX_DAV_TIMER");
  uint32_t lasttimer = utils::readReg("GEM_AMC.DAQ.EXT_STATUS.LAST_DAV_TIMER");
  // FIXME REMOVE // la->response->set_word("max", maxtimer);
  // FIXME REMOVE // la->response->set_word("last",lasttimer);

  if (max)
    return maxtimer;
  else
    return lasttimer;
}

uint32_t amc::daq::getLinkDAQStatus::operator()(const uint8_t& gtx) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_WARN(logger,"getLinkDAQStatus not implemented");
  return 0x0;
}

uint32_t amc::daq::getLinkDAQCounters::operator()(const uint8_t& gtx, const uint8_t& mode) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_WARN(logger,"getLinkDAQCounters not implemented");
  return 0x0;
}

uint32_t amc::daq::getLinkLastDAQBlock::operator()(const uint8_t& gtx) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_WARN(logger,"getLinkLastDAQBlock not implemented");
  return 0x0;
}

uint32_t amc::daq::getDAQLinkInputTimeout::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.EXT_CONTROL.INPUT_TIMEOUT");
}

uint32_t amc::daq::getDAQLinkRunType::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.EXT_CONTROL.RUN_TYPE");
}

uint32_t amc::daq::getDAQLinkRunParameters::operator()() const
{
  return utils::readReg("GEM_AMC.DAQ.EXT_CONTROL.RUN_PARAMS");
}

uint32_t amc::daq::getDAQLinkRunParameter::operator()(const uint8_t& parameter) const
{
  std::stringstream regname;
  regname << "GEM_AMC.DAQ.EXT_CONTROL.RUN_PARAM" << static_cast<int>(parameter);
  return utils::readReg(regname.str());
}


void amc::daq::setDAQLinkInputTimeout::operator()(const uint32_t& inputTO) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
  LOG4CPLUS_WARN(logger,"setDAQLinkInputTimeout not implemented");
}

void amc::daq::setDAQLinkRunType::operator()(const uint32_t& rtype) const
{
  utils::writeReg("GEM_AMC.DAQ.EXT_CONTROL.RUN_TYPE", rtype);
}

void amc::daq::setDAQLinkRunParameter::operator()(const uint8_t& parN, const uint8_t& rparam) const
{
  if (parN < 1 || parN > 3) {
    std::stringstream errmsg;
    errmsg << "Attempting to set DAQ link run parameter " << parN << ": outside expectation (1-3)";
    // FIXME REMOVE // la->response->set_string("error", errmsg.str());
    return;
  }
  std::stringstream regBase;
  regBase << "GEM_AMC.DAQ.EXT_CONTROL.RUN_PARAM" << (int)parN;
  utils::writeReg(regBase.str(), rparam);
}

void amc::daq::setDAQLinkRunParameters::operator()(const uint32_t& rparams) const
{
  utils::writeReg("GEM_AMC.DAQ.EXT_CONTROL.RUN_PARAMS", rparams);
}


/** Composite methods */
void amc::daq::configureDAQModule::operator()(const bool& enableZS, const uint32_t& runType, const bool& doPhaseShift, const bool& relock, const bool& bc0LockPSMode) const
{
  // FIXME: Should the full routine be run in the event of an error?
  // Curently none of these calls will throw/error directly, could handle that here?
  amc::sca::scaHardResetEnable{}(false);
  amc::ttc::ttcCounterReset{}();

  // FIXME: if we include this as part of the sequence, needs to go in amc.cpp (links with ttc.cpp)
  if (doPhaseShift) {
    amc::ttc::ttcMMCMPhaseShift{}(relock, bc0LockPSMode);
  }

  amc::ttc::setL1AEnable{}(false);
  amc::daq::disableDAQLink{}();
  amc::daq::resetDAQLink{}();
  amc::daq::enableDAQLink{}(0x4); // FIXME
  amc::daq::setZS{}(enableZS);
  amc::daq::setDAQLinkRunType{}(0x0);
  amc::daq::setDAQLinkRunParameters{}(0xfaac);
}

void amc::daq::enableDAQModule::operator()(const bool& enableZS) const
{
  // FIXME: Should the full routine be run in the event of an error?
  // Curently none of these calls will throw/error directly, could handle that here?
  amc::ttc::ttcModuleReset{}();
  amc::daq::enableDAQLink{}(0x4); // FIXME
  amc::daq::resetDAQLink{}();
  amc::daq::setZS{}(enableZS);
  amc::ttc::setL1AEnable{}(true);
}
