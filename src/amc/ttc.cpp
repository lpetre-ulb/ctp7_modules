/*!
 * \file amc/ttc.cpp
 * \brief AMC TTC methods for RPC modules
 */

#include "ctp7_modules/common/amc/ttc.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"

#include <chrono>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

void amc::ttc::ttcModuleReset::operator()() const
{
  // utils::writeReg("GEM_AMC.TTC.CTRL.MODULE_RESET", 0x1);
}

void amc::ttc::ttcMMCMReset::operator()() const
{
  utils::writeReg("GEM_AMC.TTC.CTRL.MMCM_RESET", 0x1);
}

void amc::ttc::ttcMMCMPhaseShift::operator()(const bool& relock, const bool& modeBC0, const bool& scan) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));

  const int PLL_LOCK_READ_ATTEMPTS = 10;

  std::stringstream msg;
  msg << "ttcMMCMPhaseShift: Starting phase shifting procedure";
  LOG4CPLUS_INFO(logger, msg.str());

  std::string strTTCCtrlBaseNode = "GEM_AMC.TTC.CTRL.";
  std::vector<std::pair<std::string, uint32_t> > ttcCtrlRegs;

  ttcCtrlRegs.push_back(std::make_pair("DISABLE_PHASE_ALIGNMENT"       , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("PA_DISABLE_GTH_PHASE_TRACKING" , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("PA_MANUAL_OVERRIDE"            , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("PA_MANUAL_SHIFT_DIR"           , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("PA_GTH_MANUAL_OVERRIDE"        , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("PA_GTH_MANUAL_SHIFT_DIR"       , 0x0));
  ttcCtrlRegs.push_back(std::make_pair("PA_GTH_MANUAL_SHIFT_STEP"      , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("PA_GTH_MANUAL_SEL_OVERRIDE"    , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("PA_GTH_MANUAL_COMBINED"        , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("GTH_TXDLYBYPASS"               , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("PA_MANUAL_PLL_RESET"           , 0x1));
  ttcCtrlRegs.push_back(std::make_pair("CNT_RESET"                     , 0x1));

  // write & readback of aforementioned registers
  uint32_t readback;
  for (auto const& ttcRegIter : ttcCtrlRegs) {
    utils::writeReg(strTTCCtrlBaseNode + ttcRegIter.first, ttcRegIter.second);
    std::this_thread::sleep_for(std::chrono::microseconds(250));
    readback = utils::readReg(strTTCCtrlBaseNode + ttcRegIter.first);
    if (readback != ttcRegIter.second) {
      std::stringstream errmsg;
      errmsg << "Readback of " << strTTCCtrlBaseNode + ttcRegIter.first
             << " failed, value is " << readback
             << ", expected " << ttcRegIter.second;

      LOG4CPLUS_ERROR(logger, "ttcMMCMPhaseShift: " + errmsg.str());
      throw std::runtime_error(errmsg.str());
    }
  }

  if (utils::readReg(strTTCCtrlBaseNode+"DISABLE_PHASE_ALIGNMENT") == 0x0) {
    std::stringstream errmsg;
    errmsg << "Automatic phase alignment is turned off!!";
    LOG4CPLUS_ERROR(logger, "ttcMMCMPhaseShift: " + errmsg.str());
    throw std::runtime_error(errmsg.str());
  }

  uint32_t readAttempts = 1;
  int maxShift = 7680+(7680/2);

  if (!modeBC0) {
    readAttempts = PLL_LOCK_READ_ATTEMPTS;
  }
  if (scan) {
    readAttempts = PLL_LOCK_READ_ATTEMPTS;
    maxShift = 23040;
  }

  uint32_t mmcmShiftCnt = utils::readReg("GEM_AMC.TTC.STATUS.CLK.PA_MANUAL_SHIFT_CNT");
  uint32_t gthShiftCnt  = utils::readReg("GEM_AMC.TTC.STATUS.CLK.PA_MANUAL_GTH_SHIFT_CNT");
  uint32_t pllLockCnt = amc::ttc::checkPLLLock{}(readAttempts);
  bool firstUnlockFound = false;
  bool nextLockFound    = false;
  bool bestLockFound    = false;
  bool reversingForLock = false;
  uint32_t phase = 0;
  float phaseNs = 0.0;

  bool mmcmShiftTable[] = {false, false, false, true, false, false, false,
                           false, false, true, false, false, false, false,
                           false, true, false, false, false, false, true,
                           false, false, false, false, false, true, false,
                           false, false, false, false, true, false, false,
                           false, false, false, true, false, false};

  int nGoodLocks       = 0;
  int nShiftsSinceLock = 0;
  int nBadLocks        = 0;
  int totalShiftCount  = 0;

  for (int i = 0; i < maxShift; ++i) {
    utils::writeReg(strTTCCtrlBaseNode + "CNT_RESET", 0x1);
    utils::writeReg(strTTCCtrlBaseNode + "PA_GTH_MANUAL_SHIFT_EN", 0x1);

    if (!reversingForLock && (gthShiftCnt == 39)) {
      msg.clear();
      msg.str(std::string());
      msg << "ttcMMCMPhaseShift: Normal GTH shift rollover 39->0";
      LOG4CPLUS_DEBUG(logger,msg.str());
      gthShiftCnt = 0;
    } else if (reversingForLock && (gthShiftCnt == 0)) {
      msg.clear();
      msg.str(std::string());
      msg << "ttcMMCMPhaseShift: Reversed GTH shift rollover 0->39";
      LOG4CPLUS_DEBUG(logger, msg.str());
      gthShiftCnt = 39;
    } else {
      if (reversingForLock) {
        gthShiftCnt -= 1;
      } else {
        gthShiftCnt += 1;
      }
    }

    uint32_t tmpGthShiftCnt  = utils::readReg("GEM_AMC.TTC.STATUS.CLK.PA_MANUAL_GTH_SHIFT_CNT");
    uint32_t tmpMmcmShiftCnt = utils::readReg("GEM_AMC.TTC.STATUS.CLK.PA_MANUAL_SHIFT_CNT");
    LOG4CPLUS_INFO(logger, "tmpGthShiftCnt: " << tmpGthShiftCnt << ", tmpMmcmShiftCnt " << tmpMmcmShiftCnt);
    while (gthShiftCnt != tmpGthShiftCnt) {
      msg.clear();
      msg.str(std::string());
      msg << "ttcMMCMPhaseShift: Repeating a GTH PI shift because the shift count doesn't match the expected value."
          << " Expected shift cnt = " << gthShiftCnt
          << ", ctp7 returned "       << tmpGthShiftCnt;
      LOG4CPLUS_WARN(logger, msg.str());

      utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_EN", 0x1);
      tmpGthShiftCnt = utils::readReg("GEM_AMC.TTC.STATUS.CLK.PA_MANUAL_GTH_SHIFT_CNT");
      //FIX ME should this continue indefinitely...?
    }

    if (mmcmShiftTable[gthShiftCnt+1]) {
      if (!reversingForLock && (mmcmShiftCnt == 0xffff)) {
        mmcmShiftCnt = 0;
      } else if (reversingForLock && (mmcmShiftCnt == 0x0)) {
        mmcmShiftCnt = 0xffff;
      } else {
        if (reversingForLock) {
          mmcmShiftCnt -= 1;
        } else {
          mmcmShiftCnt += 1;
        }
      }

      tmpMmcmShiftCnt = utils::readReg("TTC.STATUS.CLK.PA_MANUAL_SHIFT_CNT");
      if (mmcmShiftCnt != tmpMmcmShiftCnt) {
        msg.clear();
        msg.str(std::string());
        msg << "ttcMMCMPhaseShift: Reported MMCM shift count doesn't match the expected MMCM shift count."
            << " Expected shift cnt = " << mmcmShiftCnt
            << " , ctp7 returned "      << tmpMmcmShiftCnt;
        LOG4CPLUS_WARN(logger, msg.str());
      }
    }

    pllLockCnt = amc::ttc::checkPLLLock{}(readAttempts);
    phase      = utils::readReg("GEM_AMC.TTC.STATUS.CLK.TTC_PM_PHASE_MEAN");
    phaseNs    = phase * 0.01860119;
    uint32_t gthPhase = utils::readReg("TTC.STATUS.CLK.GTH_PM_PHASE_MEAN");
    float gthPhaseNs = gthPhase * 0.01860119;

    uint32_t bc0Locked  = utils::readReg("GEM_AMC.TTC.STATUS.BC0.LOCKED");
    //uint32_t bc0UnlkCnt = utils::readReg("GEM_AMC.TTC.STATUS.BC0.UNLOCK_CNT");
    //uint32_t sglErrCnt  = utils::readReg("GEM_AMC.TTC.STATUS.TTC_SINGLE_ERROR_CNT");
    //uint32_t dblErrCnt  = utils::readReg("GEM_AMC.TTC.STATUS.TTC_DOUBLE_ERROR_CNT");

    msg.clear();
    msg.str(std::string());
    msg << "ttcMMCMPhaseShift: GTH shift #" << i
      << ": mmcm shift cnt = "    << mmcmShiftCnt
      << ", mmcm phase counts = " << phase
      << ", mmcm phase = "        << phaseNs << "ns"
      << ", gth phase counts = "  << gthPhase
      << ", gth phase = "         << gthPhaseNs << "ns"
      << ", PLL lock count = "    << pllLockCnt;
    LOG4CPLUS_DEBUG(logger, msg.str());

    if (modeBC0) {
      if (!firstUnlockFound) {
        bestLockFound = false;
        if (bc0Locked == 0) {
          nBadLocks += 1;
          nGoodLocks = 0;
        } else {
          nBadLocks   = 0;
          nGoodLocks += 1;
        }

        if (relock) {
          if (nBadLocks > 100) {
            firstUnlockFound = true;
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: 100 unlocks found after " << i+1 << " shifts:"
                << " bad locks "              << nBadLocks
                << ", good locks "            << nGoodLocks
                << ", mmcm phase count = "    << phase
                << ", mmcm phase ns = "       << phaseNs << "ns";
            LOG4CPLUS_INFO(logger, msg.str());
          }
        } else {
          if (reversingForLock && (nBadLocks > 0)) {
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: Bad BC0 lock found:"
                << " phase count = " << phase
                << ", phase ns = "   << phaseNs << "ns"
                << ", returning to normal search";
            LOG4CPLUS_DEBUG(logger, msg.str());
            utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_SHIFT_DIR",    1);
            utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_DIR",0);
            bestLockFound    = false;
            reversingForLock = false;
            nGoodLocks       = 0;
          } else if (nGoodLocks == 200) {
            reversingForLock = true;
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: 200 consecutive good BC0 locks found:"
                << " phase count = " << phase
                << ", phase ns = "   << phaseNs << "ns"
                << ", reversing scan direction";
            LOG4CPLUS_INFO(logger, msg.str());
            utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_SHIFT_DIR",    0);
            utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_DIR",1);
          }

          if (reversingForLock && (nGoodLocks == 300)) {
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: Best lock found after reversing:"
                << " phase count = " << phase
                << ", phase ns = "   << phaseNs << "ns.";
            LOG4CPLUS_INFO(logger, msg.str());
            bestLockFound    = true;
            if (scan) {
              utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_SHIFT_DIR",1);
              utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_DIR",0);
              bestLockFound    = false;
              reversingForLock = false;
              nGoodLocks       = 0;
            } else {
              break;
            }
          }
        }
      } else { // shift to first good BC0 locked
        if (bc0Locked == 0) {
          if (nextLockFound) {
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: Unexpected unlock after " << i+1 << " shifts:"
                << " bad locks "              << nBadLocks
                << ", good locks "            << nGoodLocks
                << ", mmcm phase count = "    << phase
                << ", mmcm phase ns = "       << phaseNs << "ns";
            LOG4CPLUS_DEBUG(logger, msg.str());
          }
          nBadLocks += 1;
        } else {
          if (!nextLockFound) {
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: Found next lock after " << i+1 << " shifts:"
                << " bad locks "            << nBadLocks
                << ", good locks "          << nGoodLocks
                << ", mmcm phase count = "  << phase
                << ", mmcm phase ns = "     << phaseNs << "ns";
            LOG4CPLUS_INFO(logger, msg.str());
            nextLockFound = true;
            nBadLocks   = 0;
          }
          nGoodLocks += 1;
        }

        if (nGoodLocks == 1920) {
          msg.clear();
          msg.str(std::string());
          msg << "ttcMMCMPhaseShift: Finished 1920 shifts after first good lock: "
              << "bad locks "   << nBadLocks
              << " good locks " << nGoodLocks;
          LOG4CPLUS_INFO(logger, msg.str());
          bestLockFound = true;
          if (scan) {
            nextLockFound    = false;
            firstUnlockFound = false;
            nGoodLocks       = 0;
            nBadLocks        = 0;
            nShiftsSinceLock = 0;
          } else {
            break;
          }
        }
      }
    } else if (true) { // using the PLL lock counter, but the method as for the BC0 lock
      if (!firstUnlockFound) {
        bestLockFound = false;
        if (pllLockCnt < PLL_LOCK_READ_ATTEMPTS) {
          nBadLocks += 1;
          nGoodLocks = 0;
        } else {
          nBadLocks   = 0;
          nGoodLocks += 1;
        }

        if (relock) {
          if (nBadLocks > 500) {
            firstUnlockFound = true;
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: 500 unlocks found after " << i+1 << " shifts:"
                << " bad locks "              << nBadLocks
                << ", good locks "            << nGoodLocks
                << ", mmcm phase count = "    << phase
                << ", mmcm phase ns = "       << phaseNs << "ns";
            LOG4CPLUS_DEBUG(logger, msg.str());
          } else {
            if (reversingForLock && (nBadLocks > 0)) {
              msg.clear();
              msg.str(std::string());
              msg << "ttcMMCMPhaseShift: Bad BC0 lock found:"
                  << " phase count = " << phase
                  << ", phase ns = "   << phaseNs << "ns"
                  << ", returning to normal search";
              LOG4CPLUS_DEBUG(logger, msg.str());
              utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_SHIFT_DIR",1);
              utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_DIR",0);
              bestLockFound    = false;
              reversingForLock = false;
              nGoodLocks       = 0;
            } else if (nGoodLocks == 50) {
              reversingForLock = true;
              msg.clear();
              msg.str(std::string());
              msg << "ttcMMCMPhaseShift: 50 consecutive good PLL locks found:"
                  << " phase count = " << phase
                  << ", phase ns = "   << phaseNs << "ns"
                  << ", reversing scan direction";
              LOG4CPLUS_INFO(logger, msg.str());
              utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_SHIFT_DIR",0);
              utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_DIR",1);
            }

            if (reversingForLock &&(nGoodLocks == 75)) {
              msg.clear();
              msg.str(std::string());
              msg << "ttcMMCMPhaseShift: Best lock found after reversing:"
                  << " phase count = " << phase
                  << ", phase ns = "   << phaseNs << "ns.";
              LOG4CPLUS_INFO(logger, msg.str());
              bestLockFound = true;
              if (scan) {
                utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_SHIFT_DIR",1);
                utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_DIR",0);
                bestLockFound    = false;
                reversingForLock = false;
                nGoodLocks       = 0;
              } else {
                break;
              }
            }
          }
        }
      } else { // shift to first good PLL locked
        if (pllLockCnt < PLL_LOCK_READ_ATTEMPTS) {
          if (nextLockFound) {
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: Unexpected unlock after " << i+1 << " shifts:"
                << " bad locks "              << nBadLocks
                << ", good locks "            << nGoodLocks
                << ", mmcm phase count = "    << phase
                << ", mmcm phase ns = "       << phaseNs << "ns";
            LOG4CPLUS_WARN(logger, msg.str());
            nBadLocks += 1;
          } else {
            if (!nextLockFound) {
              msg.clear();
              msg.str(std::string());
              msg << "ttcMMCMPhaseShift: Found next lock after " << i+1 << " shifts:"
                  << " bad locks "            << nBadLocks
                  << ", good locks "          << nGoodLocks
                  << ", mmcm phase count = "  << phase
                  << ", mmcm phase ns = "     << phaseNs << "ns";
              LOG4CPLUS_INFO(logger, msg.str());
              nextLockFound = true;
              nBadLocks     = 0;
            }
            nGoodLocks += 1;
          }

          if (nShiftsSinceLock == 1000) {
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: Finished 1000 shifts after first good lock:"
                << " bad locks "   << nBadLocks
                << ", good locks " << nGoodLocks;
            LOG4CPLUS_INFO(logger, msg.str());
            bestLockFound = true;
            if (scan) {
              nextLockFound    = false;
              firstUnlockFound = false;
              nGoodLocks       = 0;
              nBadLocks        = 0;
              nShiftsSinceLock = 0;
            } else {
              break;
            }
          }
        }
      }
    } else {
      if (relock && (pllLockCnt < PLL_LOCK_READ_ATTEMPTS) && !firstUnlockFound) {
        firstUnlockFound = true;
        msg.clear();
        msg.str(std::string());
        msg << "ttcMMCMPhaseShift: Unlocked after "        << i+1 << "shifts:"
            << " mmcm phase count = "     << phase
            << ", mmcm phase ns = "       << phaseNs << "ns"
            << ", pllLockCnt = "          << pllLockCnt
            << ", firstUnlockFound = "    << firstUnlockFound
            << ", relock = " << relock;
        LOG4CPLUS_WARN(logger, msg.str());
      }

      if (pllLockCnt == PLL_LOCK_READ_ATTEMPTS) {
        if (!relock) {
          if (nGoodLocks == 50) {
            reversingForLock = true;
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: 200 consecutive good PLL locks found:"
                << " phase count = " << phase
                << ", phase ns = "   << phaseNs << "ns"
                << ", reversing scan direction";
            LOG4CPLUS_INFO(logger, msg.str());
            utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_SHIFT_DIR",0);
            utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_DIR",1);
          }

          if (reversingForLock && (nGoodLocks == 75)) {
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: Best lock found after reversing:"
                << " phase count = " << phase
                << ", phase ns = "   << phaseNs << "ns.";

            LOG4CPLUS_INFO(logger, msg.str());
            bestLockFound    = true;
            if (scan) {
              utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_SHIFT_DIR",1);
              utils::writeReg("GEM_AMC.TTC.CTRL.PA_GTH_MANUAL_SHIFT_DIR",0);
              bestLockFound    = false;
              reversingForLock = false;
              nGoodLocks       = 0;
              nShiftsSinceLock = 0;
            } else {
              break;
            }
          }
        } else if (firstUnlockFound || !relock) {
          if (!nextLockFound) {
            msg.clear();
            msg.str(std::string());
            msg << "ttcMMCMPhaseShift: Found next lock after " << i+1 << " shifts:"
                << " bad locks "            << nBadLocks
                << ", good locks "          << nGoodLocks
                << ", mmcm phase count = "  << phase
                << ", mmcm phase ns = "     << phaseNs << "ns";
            LOG4CPLUS_DEBUG(logger, msg.str());
            nextLockFound = true;
          }

          if (nShiftsSinceLock > 500) {
            bestLockFound = true;
            if (!scan)
              break;
            nextLockFound    = false;
            firstUnlockFound = false;
            bestLockFound    = false;
            nGoodLocks       = 0;
            nShiftsSinceLock = 0;
          }
        } else {
          nGoodLocks += 1;
        }
      } else if (nextLockFound) {
        if (nShiftsSinceLock > 500) {
          bestLockFound = true;
          if (!scan)
            break;
          nextLockFound    = false;
          firstUnlockFound = false;
          bestLockFound    = false;
          nGoodLocks       = 0;
          nShiftsSinceLock = 0;
        }
      } else {
        bestLockFound = false;
        nBadLocks += 1;
      }
    }

    if (nextLockFound) {
      nShiftsSinceLock += 1;
    }

    if (reversingForLock) {
      totalShiftCount -= 1;
    } else{
      totalShiftCount += 1;
    }
  }

  if (bestLockFound) {
    utils::writeReg("GEM_AMC.TTC.CTRL.MMCM_RESET", 0x1);
    std::stringstream msg;
    msg << "ttcMMCMPhaseShift: Lock was found: phase count " << phase
        << ", phase " << phaseNs << "ns";

    LOG4CPLUS_INFO(logger, msg.str());
  } else {
    std::stringstream errmsg;
    errmsg << "Unable to find lock";
    LOG4CPLUS_ERROR(logger, "ttcMMCMPhaseShift: " + errmsg.str());
    throw std::runtime_error(errmsg.str());
  }

  return;
}

uint32_t amc::ttc::checkPLLLock::operator()(const uint32_t& readAttempts) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));

  uint32_t lockCnt = 0;
  std::stringstream msg;
  msg << "Executing checkPLLLock with " << readAttempts << " attempted relocks";
  LOG4CPLUS_DEBUG(logger, msg.str());
  for (uint32_t i = 0; i < readAttempts; ++i ) {
    utils::writeReg("GEM_AMC.TTC.CTRL.PA_MANUAL_PLL_RESET", 0x1);

    // wait 100us to allow the PLL to lock
    std::this_thread::sleep_for(std::chrono::microseconds(100));

    // Check if it's locked
    if (utils::readReg("GEM_AMC.TTC.STATUS.CLK.PHASE_LOCKED") != 0) {
      lockCnt += 1;
    }
  }
  return lockCnt;
}

// FIXME: can maybe abstract this to amc::ttc::getPhase(clk, mode, reads)
float amc::ttc::getMMCMPhaseMean::operator()(const uint32_t& readAttempts) const
{
  if (readAttempts < 1) {
    return static_cast<float>(utils::readReg("GEM_AMC.TTC.STATUS.CLK.TTC_PM_PHASE_MEAN"));
  } else if (readAttempts < 2) {
    return static_cast<float>(utils::readReg("GEM_AMC.TTC.STATUS.CLK.TTC_PM_PHASE"));
  } else {
    float mean = 0.;
    for (uint32_t read = 0; read < readAttempts; ++read) {
      mean += utils::readReg("GEM_AMC.TTC.STATUS.CLK.TTC_PM_PHASE");
    }
    return mean/readAttempts;
  }
}

float amc::ttc::getGTHPhaseMean::operator()(const uint32_t& readAttempts) const
{
  if (readAttempts < 1) {
    return static_cast<float>(utils::readReg("GEM_AMC.TTC.STATUS.CLK.GTH_PM_PHASE_MEAN"));
  } else if (readAttempts < 2) {
    return static_cast<float>(utils::readReg("GEM_AMC.TTC.STATUS.CLK.GTH_PM_PHASE"));
  } else {
    float mean = 0.;
    for (uint32_t read = 0; read < readAttempts; ++read) {
      mean += utils::readReg("GEM_AMC.TTC.STATUS.CLK.GTH_PM_PHASE");
    }
    return mean/readAttempts;
  }
}

float amc::ttc::getMMCMPhaseMedian::operator()(const uint32_t& readAttempts) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));

  LOG4CPLUS_WARN(logger,"getMMCMPhaseMedian not yet implemented");
  if (readAttempts < 1) {
    return static_cast<float>(utils::readReg("GEM_AMC.TTC.STATUS.CLK.TTC_PM_PHASE_MEAN"));
  } else if (readAttempts < 2) {
    return static_cast<float>(utils::readReg("GEM_AMC.TTC.STATUS.CLK.TTC_PM_PHASE"));
  } else {
    float mean = 0.;
    for (uint32_t read = 0; read < readAttempts; ++read) {
      mean += utils::readReg("GEM_AMC.TTC.STATUS.CLK.TTC_PM_PHASE");
    }
    return mean/readAttempts;
  }
}

float amc::ttc::getGTHPhaseMedian::operator()(const uint32_t& readAttempts) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));

  LOG4CPLUS_WARN(logger,"getGTHPhaseMedian not yet implemented, returning the mean");
  if (readAttempts < 1) {
    return static_cast<float>(utils::readReg("GEM_AMC.TTC.STATUS.CLK.GTH_PM_PHASE_MEAN"));
  } else if (readAttempts < 2) {
    return static_cast<float>(utils::readReg("GEM_AMC.TTC.STATUS.CLK.GTH_PM_PHASE"));
  } else {
    float mean = 0.;
    for (uint32_t read = 0; read < readAttempts; ++read) {
      mean += utils::readReg("GEM_AMC.TTC.STATUS.CLK.GTH_PM_PHASE");
    }
    return mean/readAttempts;
  }
}

void amc::ttc::ttcCounterReset::operator()() const
{
  utils::writeReg("GEM_AMC.TTC.CTRL.CNT_RESET", 0x1);
}

bool amc::ttc::getL1AEnable::operator()() const
{
  return utils::readReg("GEM_AMC.TTC.CTRL.L1A_ENABLE");
}

void amc::ttc::setL1AEnable::operator()(const bool& enable) const
{
  utils::writeReg("GEM_AMC.TTC.CTRL.L1A_ENABLE", int(enable));
}

/*** CONFIG submodule ***/
uint32_t amc::ttc::getTTCConfig::operator()(const uint8_t& cmd) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));

  LOG4CPLUS_WARN(logger,"getTTCConfig not implemented");
  // return utils::readReg("GEM_AMC.TTC.CTRL.L1A_ENABLE");
  return 0x0;
}

void amc::ttc::setTTCConfig::operator()(const uint8_t& cmd, const uint8_t& value) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));

  LOG4CPLUS_WARN(logger,"setTTCConfig not implemented");
  // return utils::writeReg("GEM_AMC.TTC.CTRL.L1A_ENABLE",value);
}

/*** STATUS submodule ***/
uint32_t amc::ttc::getTTCStatus::operator()() const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));

  LOG4CPLUS_WARN(logger,"getTTCStatus not fully implemented");
  // uint32_t retval = utils::readReg("GEM_AMC.TTC.STATUS");
  uint32_t retval = utils::readReg("GEM_AMC.TTC.STATUS.BC0.LOCKED");
  std::stringstream msg;
  msg << "getTTCStatus TTC status reads " << std::hex << std::setw(8) << retval << std::dec;
  LOG4CPLUS_DEBUG(logger,msg.str());
  return retval;
}

uint32_t amc::ttc::getTTCErrorCount::operator()(const bool& single) const
{
  if (single)
    return utils::readReg("GEM_AMC.TTC.STATUS.TTC_SINGLE_ERROR_CNT");
  else
    return utils::readReg("GEM_AMC.TTC.STATUS.TTC_DOUBLE_ERROR_CNT");
}

/*** CMD_COUNTERS submodule ***/
uint32_t amc::ttc::getTTCCounter::operator()(const uint8_t& cmd) const
{
  switch(cmd) {
  case(0x1) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.L1A");
  case(0x2) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.BC0");
  case(0x3) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.EC0");
  case(0x4) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.RESYNC");
  case(0x5) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.OC0");
  case(0x6) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.HARD_RESET");
  case(0x7) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.CALPULSE");
  case(0x8) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.START");
  case(0x9) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.STOP");
  case(0xa) :
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.TEST_SYNC");
  default :
    std::unordered_map<std::string,uint32_t> results;
    std::array<std::string,10> counters = {{"L1A","BC0","EC0","RESYNC","OC0","HARD_RESET","CALPULSE","START","STOP","TEST_SYNC"}};
    for (auto& cnt: counters)
      results[cnt] = utils::readReg("GEM_AMC.TTC.CMD_COUNTERS."+cnt);
    return utils::readReg("GEM_AMC.TTC.CMD_COUNTERS.L1A");
  }
}

uint32_t amc::ttc::getL1AID::operator()() const
{
  return utils::readReg("GEM_AMC.TTC.L1A_ID");
}

uint32_t amc::ttc::getL1ARate::operator()() const
{
  return utils::readReg("GEM_AMC.TTC.L1A_RATE");
}

uint32_t amc::ttc::getTTCSpyBuffer::operator()() const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));

  LOG4CPLUS_WARN(logger,"getTTCSpyBuffer is obsolete");
  // return utils::readReg("GEM_AMC.TTC.TTC_SPY_BUFFER");
  return 0x0;
}
