#ifndef __DSM_H__
#define __DSM_H__

#include <atomic>

#include "Cache.h"
#include "Config.h"
#include "Connection.h"
#include "DSMKeeper.h"
#include "GlobalAddress.h"
#include "agent_stat.h"

class Controller;
class DSMKeeper;
class CacheAgent;
class Directory;

class DSM {

  friend class Cache;

public:
  void registerThread();
  static DSM *getInstance(const DSMConfig &conf);

  void read(const GlobalAddress &addr, uint32_t size, uint8_t *to);
  void write(const GlobalAddress &addr, uint32_t size, const uint8_t *from);

  // lock cache line
  bool r_lock(const GlobalAddress &addr, uint32_t size = 1);
  void r_unlock(const GlobalAddress &addr, uint32_t size = 1);
  bool w_lock(const GlobalAddress &addr, uint32_t size = 1);
  void w_unlock(const GlobalAddress &addr, uint32_t size = 1);

  // alloc
  GlobalAddress malloc(size_t size, bool align = false);
  void free(const GlobalAddress &addr);

  uint16_t getMyNodeID() { return myNodeID; }
  uint16_t getMyThreadID() { return Cache::iId; }

  void showCacheStat() {
    // Debug::notifyError("%s", cache.stat.toString().c_str());
  }

  void disable_switch_cc() {

    if (getMyNodeID() == 0) {
      auto addr = GlobalAddress::Null();
      for (size_t i = 0; i < conf.machineNR; ++i) {
        addr.nodeID = i;
        this->w_lock(addr);
      }
    }
  }

  // Compatible With GAM
  GlobalAddress AlignedMalloc(size_t size) { return this->malloc(size, true); };

  GlobalAddress Malloc(size_t size) { return this->malloc(size); };

  void Free(const GlobalAddress addr) { this->free(addr); }

  int Read(const GlobalAddress addr, void *buf, const size_t count) {
    this->read(addr, count, (uint8_t *)buf);
    return count;
  }

  int Read(const GlobalAddress addr, const size_t offset, void *buf,
           const size_t count) {
    this->read(GADD(addr, offset), count, (uint8_t *)buf);
    return count;
  }

  int Write(const GlobalAddress addr, void *buf, const size_t count) {
    this->write(addr, count, (uint8_t *)buf);
    return count;
  }

  int Write(const GlobalAddress addr, const size_t offset, void *buf,
            const size_t count) {
    this->write(GADD(addr, offset), count, (uint8_t *)buf);
    return count;
  }

  void MFence() {
    // not need in our DSM
  }
  void SFence() {
    // not need in our DSM
  }

  void RLock(const GlobalAddress addr, const size_t count) {
    bool res;

    int c = 0;
  retry:
    if (c++ > 10000) {
      printf("error rlock\n");
      assert(false);
    }
    if (c > 10){
        std::cout << "retry rlock " << c << " times" << std::endl;
    }
    res = this->r_lock(addr, count);
    if (!res)
      goto retry;
  }

  void WLock(const GlobalAddress addr, const size_t count) {
    bool res;

    int c = 0;
  retry:
    if (c++ > 10000) {
      printf("error wlock\n");
      exit(-1);
      assert(false);
    }
    if (c > 10){
        std::cout << "retry wlock " << c << " times" << std::endl;
    }
    res = this->w_lock(addr, count);
    if (!res)
      goto retry;
  }

  void UnLock(const GlobalAddress addr, const size_t count) {
    this->r_unlock(addr, count);
  }

  int Try_RLock(const GlobalAddress addr, const size_t count) {
    return this->r_lock(addr, count) ? 0 : -1;
  }

  int Try_WLock(const GlobalAddress addr, const size_t count) {
    return this->w_lock(addr, count) ? 0 : -1;
  }

  size_t Put(uint64_t key, const void *value, size_t count) {
    // std::cout << "put start " << key << "size " << count << std::endl;

    std::string k = std::string("gam-") + std::to_string(key);
    keeper->memSet(k.c_str(), k.size(), (char *)value, count);
    // std::cout << "put end " << key << std::endl;
    return count;
  }

  size_t Get(uint64_t key, void *value) {
    // std::cout << "get start " << key << std::endl;

    std::string k = std::string("gam-") + std::to_string(key);

    size_t size;
    char *ret = keeper->memGet(k.c_str(), k.size(), &size);

    memcpy(value, ret, size);
    // std::cout << "get end " << key << "size " << size << std::endl;
    return size;
  }

private:
  DSM(const DSMConfig &conf);
  ~DSM();

  void initRDMAConnection();
  void initSwitchTable();

  DSMConfig conf;
  Cache cache;

public:
  uint64_t baseAddr;
  uint32_t myNodeID;
  uint8_t mac[6];
  uint16_t mybitmap;

  ThreadConnection *thCon[MAX_APP_THREAD];
  DirectoryConnection *dirCon[NR_DIRECTORY];
  CacheAgentConnection *cacheCon[NR_CACHE_AGENT];

  RemoteConnection *remoteInfo;

  DSMKeeper *keeper;
  Controller *controller;

  std::atomic_int appID;

  CacheAgent *cacheAgent[NR_CACHE_AGENT];
  Directory *dirAgent[NR_DIRECTORY];

  void barrier(const std::string &ss) { keeper->barrier(ss); }
  void reset();

  bool is_first_run;
};

inline bool DSM::r_lock(const GlobalAddress &addr, uint32_t size) {
  // RAII_Timer timer(MULTI_APP_THREAD_OP::RLOCK, cache.iId);
  return cache.r_lock(addr, size);
}

inline void DSM::r_unlock(const GlobalAddress &addr, uint32_t size) {
    // RAII_Timer timer(MULTI_APP_THREAD_OP::RUNLOCK, cache.iId);
  cache.r_unlock(addr, size);
}

inline bool DSM::w_lock(const GlobalAddress &addr, uint32_t size) {
    // RAII_Timer timer(MULTI_APP_THREAD_OP::WLOCK, cache.iId);
  return cache.w_lock(addr, size);
}

inline void DSM::w_unlock(const GlobalAddress &addr, uint32_t size) {
    // RAII_Timer timer(MULTI_APP_THREAD_OP::WUNLOCK, cache.iId);
  cache.w_unlock(addr, size);
}

inline GlobalAddress DSM::malloc(size_t size, bool align) {
    // RAII_Timer timer(MULTI_APP_THREAD_OP::MALLOC, cache.iId);
  return cache.malloc(size, align);
}

inline void DSM::free(const GlobalAddress &addr) { cache.free(addr); }

#endif /* __DSM_H__ */
