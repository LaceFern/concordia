#ifndef _GALLOC_H_
#define _GALLOC_H_

#include "DSM.h"

#include "log.h"

#include <string>

// #define GADD(addr, off) ((addr)+(off))
#define Gnullptr (GlobalAddress::Null())

using GAlloc = DSM;
using GAddr = GlobalAddress;

struct Conf {
  bool is_master = true; // mark whether current process is the master (obtained
                         // from conf and the current ip)
  int master_port = 12345;
  std::string master_ip = "localhost";
  std::string master_bindaddr;
  int worker_port = 12346;
  std::string worker_bindaddr;
  std::string worker_ip = "localhost";
  size_t size = 1024 * 1024L * 512; // per-server size of memory pre-allocated
  size_t ghost_th = 1024 * 1024;
  double cache_th = 0.15; // if free mem is below this threshold, we start to
                          // allocate memory from remote nodes
  int unsynced_th = 1;
  double factor = 1.25;
  int maxclients = 1024;
  int maxthreads = 10;
  // int backlog = TCP_BACKLOG;
  int loglevel = LOG_DEBUG;
  std::string *logfile = nullptr;
  int timeout = 10;          // ms
  int eviction_period = 100; // ms

  int cluster_size = 0;
};

class GAllocFactory {
private:
  static Conf *conf;

public:
  static std::string *LogFile() {
    return conf->logfile;
  }

  static int LogLevel() { return LOG_INFO; }
  static DSM *CreateAllocator(Conf *gam_conf) {

    conf = gam_conf;

    DSMConfig c(CacheConfig(), gam_conf->cluster_size);
    auto dsm = DSM::getInstance(c);

    return dsm;
  }
};

#endif