/*! \file src/amc/blaster_ram.cpp
 *  \author Jared Sturdy <sturdy@cern.ch>
 */

#include "ctp7_modules/common/amc/blaster_ram.h"
#include "ctp7_modules/server/amc/blaster_ram.h"
#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"
#include "ctp7_modules/common/hw_constants.h"

#include <chrono>
#include <iomanip>
#include <string>
#include <time.h>
#include <thread>

namespace amc {
  namespace blaster {
    auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
  }
}

uint32_t amc::blaster::getRAMMaxSize::operator()(const BLASTERType& type) const
{
  uint32_t ram_size = 0x0;
  switch (type) {
  case (BLASTERType::GBT) :
    return utils::readReg("GEM_AMC.CONFIG_BLASTER.STATUS.GBT_RAM_SIZE");
  case (BLASTERType::OptoHybrid) :
    return utils::readReg("GEM_AMC.CONFIG_BLASTER.STATUS.OH_RAM_SIZE");
  case (BLASTERType::VFAT) :
    return utils::readReg("GEM_AMC.CONFIG_BLASTER.STATUS.VFAT_RAM_SIZE");
  case (BLASTERType::ALL) :
    ram_size  = getRAMMaxSize{}(BLASTERType::GBT);
    ram_size += getRAMMaxSize{}(BLASTERType::OptoHybrid);
    ram_size += getRAMMaxSize{}(BLASTERType::VFAT);
    return ram_size;
  default:
    break;
  }

  std::stringstream errmsg;
  errmsg << "Invalid BLASTER type " << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint8_t>(type) << std::dec << " specified";
  LOG4CPLUS_ERROR(logger, errmsg.str());

  // FIXME throw? or return 0?
  throw std::range_error(errmsg.str());
  // return ram_size;
}

bool amc::blaster::checkBLOBSize(const BLASTERType& type, const size_t& sz)
{
  uint32_t ram_sz = getRAMMaxSize{}(type);
  bool valid = (sz == ram_sz) ? true : false;
  return valid;
}

uint32_t amc::blaster::getRAMBaseAddr(const BLASTERType& type, const uint8_t& ohN, const uint8_t& partN)
{
  uint32_t base = 0x0;
  std::string regName;

  // FIXME should we have a check on supported OptoHybrids here?

  if (ohN > (amc::OH_PER_AMC-1)) {
    std::stringstream errmsg;
    errmsg << "Invalid OptoHybrid specified: OH" << ohN << " > " << (amc::OH_PER_AMC-1);
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  switch (type) {
  case (BLASTERType::GBT) :
    if (partN > (gbt::GBTS_PER_OH-1)) {
      std::stringstream errmsg;
      errmsg << "Invalid GBT specified: GBT" << partN << " > " << (gbt::GBTS_PER_OH-1);
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::range_error(errmsg.str());
    }
    regName = "GEM_AMC.CONFIG_BLASTER.RAM.GBT_OH" + std::to_string(ohN);
    base = utils::getAddress(regName);
    base+= gbt::GBT_SINGLE_RAM_SIZE*partN;
    return base;
  case (BLASTERType::OptoHybrid) :
    regName = "GEM_AMC.CONFIG_BLASTER.RAM.OH_FPGA_OH" + std::to_string(ohN);
    base    = utils::getAddress(regName);
    return base;
  case (BLASTERType::VFAT) :
    if (partN > (oh::VFATS_PER_OH-1)) {
      std::stringstream errmsg;
      errmsg << "Invalid GBT specified: VFAT" << partN << " > " << (oh::VFATS_PER_OH-1);
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::range_error(errmsg.str());
    }
    regName = "GEM_AMC.CONFIG_BLASTER.RAM.VFAT_OH" + std::to_string(ohN);
    base = utils::getAddress(regName);
    base+= vfat::VFAT_SINGLE_RAM_SIZE*partN;
    return base;
  default:
    break;
  }

  std::stringstream errmsg;
  errmsg << "Invalid BLASTER type " << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint8_t>(type) << std::dec << " specified";
  LOG4CPLUS_ERROR(logger, errmsg.str());
  throw std::runtime_error(errmsg.str());
}

std::vector<uint32_t> amc::blaster::readConfRAM::operator()(const BLASTERType& type, const size_t& blob_sz) const
{
  uint32_t nwords = 0x0;
  // uint32_t offset = 0x0;

  if (!checkBLOBSize(type, blob_sz)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob_sz << " for BLASTER RAM BLOB";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }

  std::vector<uint32_t> blob;
  std::vector<uint32_t> tmpblob;

  std::stringstream regName;
  regName << "GEM_AMC.CONFIG_BLASTER.RAM";

  LOG4CPLUS_DEBUG(logger, "readConfRAM with type: 0x" << std::hex << static_cast<uint8_t>(type) << std:: dec << ", size: " << blob_sz);
  switch (type) {
  case (BLASTERType::GBT):
    blob.resize(getRAMMaxSize{}(BLASTERType::GBT));
    regName << ".GBT";
    // return readGBTConfRAM{}(BLASTERType::GBT, getRAMMaxSize{}(BLASTERType::GBT)); // FIXME function does not exist
    break;
  case (BLASTERType::OptoHybrid):
    blob.resize(getRAMMaxSize{}(BLASTERType::OptoHybrid));
    regName << ".OH_FPGA";
    // return readOptoHybridConfRAM{}(blob, getRAMMaxSize{}(BLASTERType::OptoHybrid)); // FIXME function does not exist
    break;
  case (BLASTERType::VFAT):
    blob.resize(getRAMMaxSize{}(BLASTERType::VFAT));
    regName << ".VFAT";
    // return readVFATConfRAM{}(blob, getRAMMaxSize{}(BLASTERType::VFAT)); // FIXME function does not exist
    break;
  case (BLASTERType::ALL):
    // nwords  = readConfRAM{}(BLASTERType::GBT, blob.data()+nwords, getRAMMaxSize{}(BLASTERType::GBT));
    tmpblob = readConfRAM{}(BLASTERType::GBT, getRAMMaxSize{}(BLASTERType::GBT));
    // offset += getRAMMaxSize{}(BLASTERType::GBT);
    // nwords += readConfRAM{}(BLASTERType::OptoHybrid, blob.data()+nwords, getRAMMaxSize{}(BLASTERType::OptoHybrid));
    blob.insert(blob.end(), tmpblob.begin(), tmpblob.end());
    tmpblob.clear();
    tmpblob = readConfRAM{}(BLASTERType::OptoHybrid, getRAMMaxSize{}(BLASTERType::OptoHybrid));
    // offset += getRAMMaxSize{}(BLASTERType::OptoHybrid);
    // nwords += readConfRAM{}(BLASTERType::VFAT, blob.data()+nwords, getRAMMaxSize{}(BLASTERType::VFAT));
    blob.insert(blob.end(), tmpblob.begin(), tmpblob.end());
    tmpblob.clear();
    tmpblob = readConfRAM{}(BLASTERType::VFAT, getRAMMaxSize{}(BLASTERType::VFAT));
    blob.insert(blob.end(), tmpblob.begin(), tmpblob.end());
    return blob;
  default:
    std::stringstream errmsg;
    errmsg << "Invalid BLASTER RAM type "
           << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint8_t>(type) << std::dec
           << " selected for read.";
    LOG4CPLUS_ERROR(logger, errmsg.str());

    throw std::range_error(errmsg.str());
  }

  nwords = utils::readBlock(regName.str(), blob.data(), blob_sz);
  LOG4CPLUS_DEBUG(logger, "read: " << nwords << " words from " << regName.str());

  return blob;
}

std::vector<uint32_t> amc::blaster::readGBTConfRAM::operator()(const uint16_t& ohMask) const
{
  LOG4CPLUS_DEBUG(logger, "readGBTConfRAM called");

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // read full GBT RAM blob
    uint32_t blob_sz = getRAMMaxSize{}(BLASTERType::GBT);
    std::vector<uint32_t> gbtblob(blob_sz, 0);
    uint32_t nwords = utils::readBlock("GEM_AMC.CONFIG_BLASTER.RAM.GBT", static_cast<uint32_t*>(gbtblob.data()), blob_sz);
    return gbtblob;
  } else {
    // read blob from specific GBT RAM, as specified by ohMask, support non consecutive OptoHybrids?
    // emtpy space for missing OHs?
    uint32_t blob_sz = getRAMMaxSize{}(BLASTERType::GBT);
    std::vector<uint32_t> gbtblob(blob_sz, 0);
    const uint32_t perblk = gbt::GBT_SINGLE_RAM_SIZE*gbt::GBTS_PER_OH;
    uint32_t nwords = 0x0;
    uint32_t* blob = static_cast<uint32_t*>(gbtblob.data());
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.GBT_OH" << oh;
        nwords += utils::readBlock(reg.str(), blob, perblk);
        blob += perblk;
      }
    }
    return gbtblob;
  }
}

std::vector<uint32_t> amc::blaster::readOptoHybridConfRAM::operator()(const uint16_t& ohMask) const
{
  LOG4CPLUS_DEBUG(logger, "readOptoHybridConfRAM called");

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // read from all OptoHybrids
    uint32_t blob_sz = getRAMMaxSize{}(BLASTERType::OptoHybrid);
    std::vector<uint32_t> ohblob(blob_sz, 0);
    uint32_t nwords = utils::readBlock("GEM_AMC.CONFIG_BLASTER.RAM.OH", ohblob.data(), blob_sz);
    return ohblob;
  } else {
    // read blob from specific OptoHybrid RAM, as specified by ohMask, support non consecutive OptoHybrids?
    // emtpy space for missing OHs?
    uint32_t blob_sz = getRAMMaxSize{}(BLASTERType::OptoHybrid);
    std::vector<uint32_t> ohblob(blob_sz, 0);
    uint32_t nwords = 0x0;
    uint32_t* blob = ohblob.data();
    const uint32_t perblk = oh::OH_SINGLE_RAM_SIZE;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.OH_FPGA_OH" << oh;
        nwords += utils::readBlock(reg.str(), blob, perblk);
        blob += perblk;
      }
    }
    return ohblob;
  }
}

std::vector<uint32_t> amc::blaster::readVFATConfRAM::operator()(const uint16_t& ohMask) const
{
  LOG4CPLUS_DEBUG(logger, "readVFATConfRAM called");

  if (ohMask == 0x0 || ohMask == 0xfff) {
    // read VFAT blob from all OptoHybrids
    uint32_t blob_sz = getRAMMaxSize{}(BLASTERType::VFAT);
    std::vector<uint32_t> vfatblob(blob_sz, 0);
    uint32_t nwords = utils::readBlock("GEM_AMC.CONFIG_BLASTER.RAM.VFAT", vfatblob.data(), blob_sz);
    return vfatblob;
  } else {
    // read blob from OptoHybrid specific VFAT RAM, as specified by ohMask, support non consecutive OptoHybrids?
    // emtpy space for missing OHs?
    uint32_t blob_sz = getRAMMaxSize{}(BLASTERType::VFAT);
    std::vector<uint32_t> vfatblob(blob_sz, 0);
    uint32_t nwords = 0x0;
    uint32_t* blob = vfatblob.data();
    const uint32_t perblk = vfat::VFAT_SINGLE_RAM_SIZE*oh::VFATS_PER_OH;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.VFAT_OH" << oh;
        nwords += utils::readBlock(reg.str(), blob, perblk);
        blob += perblk;
      }
    }
    return vfatblob;
  }
}

void amc::blaster::writeConfRAM::operator()(const BLASTERType& type, const std::vector<uint32_t>& blob) const
{
  if (!checkBLOBSize(type, blob.size())) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << blob.size() << " for BLASTER RAM BLOB";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }

  // do memory validation on blob, doesn't make sense now that this is a vector...
  if (!blob.data()) {
    std::stringstream errmsg;
    errmsg << "Invalid BLOB " << std::hex << std::setw(8) << std::setfill('0') << blob.data()
           << std::dec << " provided to write BLASTER RAM";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }

  auto iter = blob.begin();
  std::vector<uint32_t> tmpblob; // = blob;

  LOG4CPLUS_WARN(logger, "writeConfRAM with type: 0x" << std::hex << static_cast<uint8_t>(type) << std::dec << ", size: " << blob.size());
  switch (type) {
  case (BLASTERType::GBT):
    writeGBTConfRAM{}(blob);
    return;
  case (BLASTERType::OptoHybrid):
    writeOptoHybridConfRAM{}(blob);
    return;
  case (BLASTERType::VFAT):
    writeVFATConfRAM{}(blob);
    return;
  case (BLASTERType::ALL):
    LOG4CPLUS_WARN(logger, "Reading the full RAM");
    tmpblob.clear();
    tmpblob.resize(getRAMMaxSize{}(BLASTERType::GBT));
    tmpblob.insert(tmpblob.begin(), iter, iter+getRAMMaxSize{}(BLASTERType::GBT));
    writeConfRAM{}(BLASTERType::GBT, tmpblob);

    iter += getRAMMaxSize{}(BLASTERType::GBT);
    tmpblob.clear();
    tmpblob.resize(getRAMMaxSize{}(BLASTERType::OptoHybrid));
    tmpblob.insert(tmpblob.begin(), iter, iter+getRAMMaxSize{}(BLASTERType::OptoHybrid));
    writeConfRAM{}(BLASTERType::OptoHybrid, tmpblob);

    iter += getRAMMaxSize{}(BLASTERType::OptoHybrid);
    tmpblob.clear();
    tmpblob.resize(getRAMMaxSize{}(BLASTERType::VFAT));
    tmpblob.insert(tmpblob.begin(), iter, iter+getRAMMaxSize{}(BLASTERType::VFAT));
    writeConfRAM{}(BLASTERType::VFAT, tmpblob);
    return;
  default:
    // writeConfRAM{}(BLASTERType::ALL, blob, blob.size());
    std::stringstream errmsg;
    errmsg << "Invalid BLASTER RAM type "
           << std::hex << std::setw(8) << std::setfill('0') << static_cast<uint8_t>(type) << std::dec
           << " selected for write.";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
}

void amc::blaster::writeGBTConfRAM::operator()(const std::vector<uint32_t>& gbtblob, const uint16_t& ohMask) const
{
  LOG4CPLUS_DEBUG(logger, "writeGBTConfRAM called");

  if (gbtblob.size() > getRAMMaxSize{}(BLASTERType::GBT)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << gbtblob.size() << " for GBT BLASTER RAM BLOB";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    utils::writeBlock("GEM_AMC.CONFIG_BLASTER.RAM.GBT", gbtblob.data(), gbtblob.size());
  } else {
    const uint32_t perblk = gbt::GBT_SINGLE_RAM_SIZE*gbt::GBTS_PER_OH;
    const uint32_t* blob = gbtblob.data();
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.GBT_OH" << oh;
        utils::writeBlock(reg.str(), blob, perblk);
        blob += perblk;
      }
    }
  }

  return;
}

void amc::blaster::writeOptoHybridConfRAM::operator()(const std::vector<uint32_t>& ohblob, const uint16_t& ohMask) const
{
  LOG4CPLUS_DEBUG(logger, "writeOptoHybridConfRAM called");

  if (ohblob.size() > getRAMMaxSize{}(BLASTERType::OptoHybrid)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << ohblob.size() << " for OptoHybrid BLASTER RAM BLOB";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    utils::writeBlock("GEM_AMC.CONFIG_BLASTER.RAM.OH", ohblob.data(), ohblob.size());
  } else {
    const uint32_t* blob = ohblob.data();
    const uint32_t perblk = oh::OH_SINGLE_RAM_SIZE;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.OH_FPGA_OH" << oh;
        utils::writeBlock(reg.str(), blob, perblk);
        blob += perblk;
      }
    }
  }

  return;
}

void amc::blaster::writeVFATConfRAM::operator()(const std::vector<uint32_t>& vfatblob, const uint16_t& ohMask) const
{
  LOG4CPLUS_DEBUG(logger, "writeVFATConfRAM called");

  if (vfatblob.size() > getRAMMaxSize{}(BLASTERType::VFAT)) {
    std::stringstream errmsg;
    errmsg << "Invalid size " << vfatblob.size() << " for VFAT BLASTER RAM BLOB";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::range_error(errmsg.str());
  }

  if (ohMask == 0x0 || ohMask == 0xfff) {
    utils::writeBlock("GEM_AMC.CONFIG_BLASTER.RAM.VFAT", vfatblob.data(), vfatblob.size());
  } else {
    const uint32_t *blob = vfatblob.data();
    const uint32_t perblk = vfat::VFAT_SINGLE_RAM_SIZE*oh::VFATS_PER_OH;
    for (size_t oh = 0; oh < amc::OH_PER_AMC; ++oh) {
      if ((0x1<<oh)&ohMask) {
        std::stringstream reg;
        reg << "GEM_AMC.CONFIG_BLASTER.RAM.VFAT_OH" << oh;
        utils::writeBlock(reg.str(), blob, perblk);
        blob += perblk;
      }
    }
  }

  return;
}
