#include "ctp7_modules/common/utils.h"
#include "ctp7_modules/server/utils.h"

#include <log4cplus/configurator.h>
#include <log4cplus/hierarchy.h>

#include <cstdlib>
#include <fstream>
#include <thread>
#include <chrono>

#include "xhal/common/rpc/register.h"

/*
 * Server methods implementation. These methods can be called at the RPC server side only and are not inteded to be exported as remote calls.
 */

memsvc_handle_t memsvc;

std::vector<std::string> utils::split(const std::string& s, char delim)
{
  std::vector<std::string> elems;
  split(s, delim, std::back_inserter(elems));
  return elems;
}

std::string utils::serialize(xhal::common::utils::Node n)
{
  std::stringstream node;
  node << std::hex << n.real_address << std::dec
       << "|" << n.permission
       << "|" << std::hex << n.mask << std::dec
       << "|" << n.mode
       << "|" << std::hex << n.size << std::dec;
  return node.str();
}

void utils::initLogging()
{
    log4cplus::initialize();

    // Loading the same configuration twice seems to create issues
    // Prefer to start from scratch
    log4cplus::Logger::getDefaultHierarchy().resetConfiguration();

    // Try to get the configuration from the envrionment
    const char * const configurationFilename = std::getenv(LOGGING_CONFIGURATION_ENV);

    std::ifstream configurationFile{};
    if (configurationFilename)
        configurationFile.open(configurationFilename);

    if (configurationFile.is_open()) {
        log4cplus::PropertyConfigurator configurator{configurationFile};
        configurator.configure();
    } else {
        // Fallback to the default embedded configuration
        std::istringstream defaultConfiguration{LOGGING_DEFAULT_CONFIGURATION};
        log4cplus::PropertyConfigurator configurator{defaultConfiguration};
        configurator.configure();

        // FIXME: Cannot use a one-liner, because move constructors are disabled in the compiled library
        auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
        LOG4CPLUS_INFO(logger, LOG4CPLUS_TEXT("Impossible to read the configuration file; using the default embedded configuration."));
    }
}

void utils::update_address_table::operator()(const std::string& at_xml) const
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
  LOG4CPLUS_INFO(logger, "START UPDATE ADDRESS TABLE");
  std::string gem_path = std::getenv("GEM_PATH");
  std::string lmdb_data_file = gem_path+"/address_table.mdb/data.mdb";
  std::string lmdb_lock_file = gem_path+"/address_table.mdb/lock.mdb";
  std::string lmdb_area_file = gem_path+"/address_table.mdb";
  auto parser = std::make_unique<xhal::common::utils::XHALXMLParser>(at_xml.c_str());
  try {
    parser->setLogLevel(0);
    parser->parseXML();
  } catch (...) {
    LOG4CPLUS_ERROR(logger, "XML parser failed");
    throw std::runtime_error("XML parser failed");
  }
  LOG4CPLUS_INFO(logger, "XML PARSING DONE ");
  std::unordered_map<std::string,xhal::common::utils::Node> parsed_at;
  parsed_at = parser->getAllNodes();
  parsed_at.erase("top");
  xhal::common::utils::Node t_node;

  // Remove old DB
  LOG4CPLUS_INFO(logger, "REMOVE OLD DB");
  std::remove(lmdb_data_file.c_str());
  std::remove(lmdb_lock_file.c_str());

  auto env = lmdb::env::create();
  env.set_mapsize(LMDB_SIZE);
  env.open(lmdb_area_file.c_str(), 0, 0664);

  LOG4CPLUS_INFO(logger, "LMDB ENV OPEN");

  lmdb::val key;
  lmdb::val value;
  auto wtxn = lmdb::txn::begin(env);
  auto wdbi  = lmdb::dbi::open(wtxn, nullptr);

  LOG4CPLUS_INFO(logger, "START ITERATING OVER MAP");

  std::string t_key;
  std::string t_value;
  for (auto const& it : parsed_at) {
    t_key = it.first;
    t_node = it.second;
    t_value = serialize(t_node);
    key.assign(t_key);
    value.assign(t_value);
    wdbi.put(wtxn, key, value);
  }
  wtxn.commit();
  LOG4CPLUS_INFO(logger, "COMMIT DB");
  wtxn.abort();
}

uint32_t utils::bitCheck(uint32_t word, int bit)
{
  if (bit > 31)
    throw std::invalid_argument("Invalid request to shift 32-bit word by more than 31 bits");
  return (word >> bit) & 0x1;
}

uint32_t utils::getNumNonzeroBits(uint32_t value)
{
  // https://stackoverflow.com/questions/4244274/how-do-i-count-the-number-of-zero-bits-in-an-integer
  uint32_t numNonzeroBits = 0;
  for (size_t i=0; i < CHAR_BIT * sizeof value; ++i) {
    if ((value & (1 << i)) == 1) {
      ++numNonzeroBits;
    }
  }

  return numNonzeroBits;
}

std::vector<std::string> utils::regExists(const std::string& regName)
{
  auto env = lmdb::env::create();
  env.set_mapsize(utils::LMDB_SIZE);
  std::string gem_path       = std::getenv("GEM_PATH");
  std::string lmdb_data_file = gem_path+"/address_table.mdb";
  env.open(lmdb_data_file.c_str(), 0, 0664);
  auto rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
  auto dbi  = lmdb::dbi::open(rtxn, nullptr);

  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  lmdb::val key, db_res;
  key.assign(regName.c_str());
  if (dbi.get(rtxn, key, db_res)) {
    return split(db_res.data(), '|');
  } else {
    return {};
  }
}

uint32_t utils::getAddress(const std::string& regName)
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  const auto db_res = regExists(regName);
  uint32_t raddr = 0x0;
  if (!db_res.empty()) {
    raddr = stoull(db_res[0], nullptr, 16);
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
  return raddr;
}

uint32_t utils::getMask(const std::string& regName)
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  const auto db_res = regExists(regName);
  uint32_t rmask = 0x0;
  if (!db_res.empty()) {
    rmask = stoull(db_res[2], nullptr, 16);
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
  return rmask;
}

void utils::writeRawAddress(const uint32_t address, const uint32_t value)
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  uint32_t data[] = {value};
  if (memhub_write(memsvc, address, 1, data) != 0) {
    std::stringstream errmsg;
    errmsg << "memsvc error: " << memsvc_get_last_error(memsvc);
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
}

uint32_t utils::readRawAddress(const uint32_t address)
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  uint32_t data[1];
  int n_current_tries = 0;
  while (true) {
    if (memhub_read(memsvc, address, 1, data) != 0) {
      if (n_current_tries < 9) {
        ++n_current_tries;
        std::stringstream errmsg;
        errmsg << "Reading reg "
               << "0x" << std::hex << std::setw(8) << std::setfill('0') << address << std::dec
               << " failed " << n_current_tries << " times.";
        LOG4CPLUS_WARN(logger, errmsg.str());
      } else {
        std::stringstream errmsg;
        errmsg << "memsvc error: " << memsvc_get_last_error(memsvc)
               << " failed 10 times";
        LOG4CPLUS_ERROR(logger, errmsg.str());
        throw std::runtime_error(errmsg.str());
      }
    } else {
      break;
    }
  }
  return data[0];
}

void utils::writeRawReg(const std::string& regName, const uint32_t value)
{
  const auto addr = utils::getAddress(regName);
  return utils::writeRawAddress(addr, value);
}

uint32_t utils::readRawReg(const std::string& regName)
{
  const auto addr = utils::getAddress(regName);
  return utils::readRawAddress(addr);
}

uint32_t utils::applyMask(const uint32_t data, uint32_t mask)
{
  uint32_t result = data & mask;
  for (int i = 0; i < 32; ++i) {
    if (mask & 1) {
      break;
    } else {
      mask = mask >> 1;
      result = result >> 1;
    }
  }
  return result;
}

uint32_t utils::readReg(const std::string& regName)
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  const auto db_res = regExists(regName);
  if (!db_res.empty()) {
    const uint32_t raddr = stoull(db_res[0], nullptr, 16);
    const std::size_t found = db_res[1].find_first_of("r");
    const uint32_t rmask = stoull(db_res[2], nullptr, 16);
    if (found==std::string::npos) {
      std::stringstream errmsg;
      errmsg << "No read permissions for "
             << regName << ": " << db_res[1].c_str();
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
    }
    const uint32_t data = utils::readRawAddress(raddr);
    if (rmask!=0xFFFFFFFF) {
      return utils::applyMask(data, rmask);
    } else {
      return data;
    }
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
}

uint32_t utils::readBlock(const std::string& regName, uint32_t *result, const uint32_t& size, const uint32_t& offset)
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  const auto db_res = regExists(regName);
  if (!db_res.empty()) {
    const uint32_t raddr = stoull(db_res[0], nullptr, 16);
    const uint32_t rmask = stoull(db_res[2], nullptr, 16);
    const uint32_t rsize = stoull(db_res[4], nullptr, 16);
    const std::string rperm = db_res[1];
    const std::string rmode = db_res[3];
    LOG4CPLUS_DEBUG(logger, "node " << regName << " properties:"
                    << " 0x"  << std::hex << std::setw(8) << std::setfill('0') << raddr << std::dec
                    << "  0x" << std::hex << std::setw(8) << std::setfill('0') << rmask << std::dec
                    << "  0x" << std::hex << std::setw(8) << std::setfill('0') << rsize << std::dec
                    << "  " << rmode << "  " << rperm);

    if (rmask != 0xFFFFFFFF) {
      // FIXME
      // deny block read on masked register, but what if mask is None?
      // Adopt the rule that mask None equals to mask == 0xFFFFFFFF?
      std::stringstream errmsg;
      errmsg << "Block read attempted on masked register";
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::range_error(errmsg.str());
    } else if (rmode.rfind("single") != std::string::npos && size > 1) {
      // only allow block read of size 1 on single registers?
      std::stringstream errmsg;
      errmsg << "Block read attempted on single register with size greater than 1";
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::range_error(errmsg.str());
    } else if ((offset+size) > rsize) {
      // don't allow the read to go beyond the range
      std::stringstream errmsg;
      errmsg << "Block read attempted would go beyond the size of the RAM: "
             << "raddr: 0x"    << std::hex << raddr
             << ", offset: 0x" << std::hex << offset
             << ", size: 0x"   << std::hex << size
             << ", rsize: 0x"  << std::hex << rsize;
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::range_error(errmsg.str());
    } else {
      if (memhub_read(memsvc, raddr+offset, size, result) != 0) {
        std::stringstream errmsg;
        errmsg << "Read memsvc error: " << memsvc_get_last_error(memsvc);
        LOG4CPLUS_ERROR(logger, errmsg.str());
        throw std::runtime_error(errmsg.str());
      } else {
        std::stringstream msg;
        msg << "Block read succeeded.";
        LOG4CPLUS_DEBUG(logger, msg.str());
      }
    }
    return size;
  }
  return 0;
}

uint32_t utils::readBlock(const uint32_t& regAddr, uint32_t *result, const uint32_t& size, const uint32_t& offset)
{
  // Might not make sense, as it would be impossible to do any validation at this level
  return 0;
}

utils::SlowCtrlErrCntVFAT utils::repeatedRegRead(const std::string& regName, const bool breakOnFailure, const uint32_t nReads)
{
    // Issue a link reset to reset counters under GEM_AMC.SLOW_CONTROL.VFAT3
    writeReg("GEM_AMC.GEM_SYSTEM.CTRL.LINK_RESET", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(90));

    // Perform the transactions
    for (uint32_t i=0; i<nReads; ++i) {
        try {
          utils::readReg(regName);
          std::this_thread::sleep_for(std::chrono::microseconds(20));
        } catch(const std::runtime_error &) {
          if (breakOnFailure)
            break;
        }
    }

    // Create the output error counter container
    SlowCtrlErrCntVFAT vfatErrs;

    std::string baseReg     = "GEM_AMC.SLOW_CONTROL.VFAT3.";
    vfatErrs.crc            = readReg(baseReg+"CRC_ERROR_CNT");
    vfatErrs.packet         = readReg(baseReg+"PACKET_ERROR_CNT");
    vfatErrs.bitstuffing    = readReg(baseReg+"BITSTUFFING_ERROR_CNT");
    vfatErrs.timeout        = readReg(baseReg+"TIMEOUT_ERROR_CNT");
    vfatErrs.axi_strobe     = readReg(baseReg+"AXI_STROBE_ERROR_CNT");
    vfatErrs.nTransactions  = readReg(baseReg+"TRANSACTION_CNT");
    vfatErrs.sumErrors();

    return vfatErrs;
}

void utils::writeReg(const std::string& regName, const uint32_t value)
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  const auto db_res = regExists(regName);
  if (!db_res.empty()) {
    const uint32_t raddr = stoull(db_res[0], nullptr, 16);
    uint32_t rmask = stoull(db_res[2], nullptr, 16);
    if (rmask==0xFFFFFFFF) {
      return utils::writeRawAddress(raddr, value);
    } else {
      uint32_t current_value = utils::readRawAddress(raddr);
      int shift_amount = 0;
      uint32_t mask_copy = rmask;
      for (int i = 0; i < 32; ++i) {
        if (rmask & 1) {
          break;
        } else {
          shift_amount +=1;
          rmask = rmask >> 1;
        }
      }
      uint32_t val_to_write = value << shift_amount;
      val_to_write = (val_to_write & mask_copy) | (current_value & ~mask_copy);
      utils::writeRawAddress(raddr, val_to_write);
    }
  } else {
    std::stringstream errmsg;
    errmsg << "Key " << regName << " was NOT found";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    throw std::runtime_error(errmsg.str());
  }
}

void utils::writeBlock(const std::string& regName, const uint32_t* values, const uint32_t& size, const uint32_t& offset)
{
  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));

  const auto db_res = regExists(regName);
  if (!db_res.empty()) {
    const uint32_t raddr = stoull(db_res[0], nullptr, 16);
    const uint32_t rmask = stoull(db_res[2], nullptr, 16);
    const uint32_t rsize = stoull(db_res[4], nullptr, 16);
    const std::string rmode = db_res[3];
    const std::string rperm = db_res[1];
    LOG4CPLUS_DEBUG(logger, "node " << regName << " properties:"
                    << " 0x"  << std::hex << std::setw(8) << std::setfill('0') << raddr << std::dec
                    << "  0x" << std::hex << std::setw(8) << std::setfill('0') << rmask << std::dec
                    << "  0x" << std::hex << std::setw(8) << std::setfill('0') << rsize << std::dec
                    << "  " << rmode << "  " << rperm);

    if (rmask != 0xFFFFFFFF) {
      // deny block write on masked register
      std::stringstream errmsg;
      errmsg << "Block write attempted on masked register";
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
    } else if (rmode.rfind("single") != std::string::npos && size > 1) {
      // only allow block write of size 1 on single registers
      std::stringstream errmsg;
      errmsg << "Block write attempted on single register with size greater than 1";
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
    } else if ((offset+size) > rsize) {
      // don't allow the write to go beyond the block range
      std::stringstream errmsg;
      errmsg << "Block write attempted would go beyond the size of the RAM: "
             << "raddr: 0x"    << std::hex << raddr
             << ", offset: 0x" << std::hex << offset
             << ", size: 0x"   << std::hex << size
             << ", rsize: 0x"  << std::hex << rsize;
      LOG4CPLUS_ERROR(logger, errmsg.str());
      throw std::runtime_error(errmsg.str());
    } else {
      if (memhub_write(memsvc, raddr+offset, size, values) != 0) {
        std::stringstream errmsg;
        errmsg << "Write memsvc error: " << memsvc_get_last_error(memsvc);
        LOG4CPLUS_ERROR(logger, errmsg.str());
        throw std::runtime_error(errmsg.str());
      } else {
        std::stringstream msg;
        msg << "Block write succeeded.";
        LOG4CPLUS_DEBUG(logger, msg.str());
      }
    }
  }
}

void utils::writeBlock(const uint32_t& regAddr, const uint32_t* values, const uint32_t& size, const uint32_t& offset)
{
  // This function doesn't make sense with an offset, why would we specify an offset when accessing by register address?
  // Maybe just to do validation checks on the size?
  return;
}

/*
 * Common methods implementation. These methods are exported as remote calls.
 */

utils::RegInfo utils::readRegFromDB::operator()(const std::string& regName) const
{
  auto env = lmdb::env::create();
  env.set_mapsize(utils::LMDB_SIZE);
  std::string gem_path       = std::getenv("GEM_PATH");
  std::string lmdb_data_file = gem_path+"/address_table.mdb";
  env.open(lmdb_data_file.c_str(), 0, 0664);
  auto rtxn = lmdb::txn::begin(env, nullptr, MDB_RDONLY);
  auto dbi  = lmdb::dbi::open(rtxn, nullptr);

  auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("logger"));
  LOG4CPLUS_INFO(logger, "LMDB ENV OPEN");
  lmdb::val key;
  lmdb::val value;

  key.assign(regName.c_str());
  bool found = dbi.get(rtxn,key,value);
  if (found) {
    LOG4CPLUS_INFO(logger, "Key: " + regName + " is found");
    std::string t_value = std::string(value.data());
    t_value = t_value.substr(0,value.size());
    const std::vector<std::string> tmp = split(t_value,'|');
    const uint32_t raddr = stoull(tmp[0], nullptr, 16);
    const uint32_t rmask = stoull(tmp[2], nullptr, 16);
    const uint32_t rsize = stoull(tmp[4], nullptr, 16);
    const std::string rperm = tmp[1];
    const std::string rmode = tmp[3];

    const utils::RegInfo regInfo = {
      .permissions = rperm,
      .mode        = rmode,
      .address     = raddr,
      .mask        = rmask,
      .size        = rsize
    };

    LOG4CPLUS_DEBUG(logger, " node " << regName << " properties: " << regInfo);

    rtxn.abort();
    return regInfo;
  } else {
    std::stringstream errmsg;
    errmsg << "Key: " << regName << " was NOT found!";
    LOG4CPLUS_ERROR(logger, errmsg.str());
    rtxn.abort(); // FIXME duplicated! fixed with a `LocalArgs object that is destroyed when going out of scope
    throw std::runtime_error(errmsg.str());
  }
}

uint32_t utils::readRemoteReg::operator()(const std::string& regName) const
{
  return readReg(regName);
}

void utils::writeRemoteReg::operator()(const std::string& regName, const uint32_t& value) const
{
  return writeReg(regName, value);
}

extern "C" {
    const char *module_version_key = "utils v1.0.1";
    const int module_activity_color = 4;

    void module_init(ModuleManager *modmgr)
    {
        utils::initLogging();

        if (memhub_open(&memsvc) != 0) {
            auto logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("main"));
            LOG4CPLUS_ERROR(logger, LOG4CPLUS_TEXT("Unable to connect to memory service: ") << memsvc_get_last_error(memsvc));
            LOG4CPLUS_ERROR(logger, "Unable to load module");
            return;
        }

        xhal::common::rpc::registerMethod<utils::update_address_table>(modmgr);
        xhal::common::rpc::registerMethod<utils::readRegFromDB>(modmgr);
        xhal::common::rpc::registerMethod<utils::readRemoteReg>(modmgr);
        xhal::common::rpc::registerMethod<utils::writeRemoteReg>(modmgr);
    }
}
