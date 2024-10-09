#ifndef __CACHE_H__
#define __CACHE_H__

#include "CacheStat.h"
#include "CacheStatus.h"
#include "Common.h"
#include "GlobalAddress.h"
#include "RawMessageConnection.h"

#include "LocalAllocator.h"

class DSM;
class CacheConfig;
class RemoteConnection;
class ThreadConnection;

inline void printStatus(CacheStatus c) {
  std::string s;
  switch (c) {
  case CacheStatus::INVALID:
    s = "INVALID";
    break;
  case CacheStatus::BEING_INVALID:
    s = "Being INVALID";
    break;
  case CacheStatus::MODIFIED:
    s = "MODIFIED";
    break;
  case CacheStatus::SHARED:
    s = "SHARED";
    break;
  case CacheStatus::BEING_FILL:
    s = "BEING_FILL";
    break;
  case CacheStatus::BEING_EVICT:
    s = "BEING_EVICT";
    break;
  default:
    assert(false);
  }
  Debug::notifyInfo("%s", s.c_str());
}

union AtomicTag {
  struct {
    Tag tag;
    CacheStatus status;
  };
  uint64_t v;

  AtomicTag() noexcept : v(0) {}

  AtomicTag(Tag tag, CacheStatus c) noexcept : v(0) {
    this->tag = tag;
    this->status = c;
  }

  bool isCanRead() {
    return status != CacheStatus::INVALID && status != CacheStatus::BEING_FILL;
  }

  bool isValid() {
    return status == CacheStatus::MODIFIED || status == CacheStatus::SHARED;
  }
} __attribute__((packed));

static const Tag InvalidTag = (Tag)-1;

static_assert(sizeof(AtomicTag) == 8, "XX");

struct LineInfo {

  void *data;
  std::atomic<AtomicTag> status;
  WRLock writeEvictLock;

#ifdef LRU_EVICT
  std::atomic<uint64_t> timestamp;
#endif

  void setStatus(CacheStatus s,
                 std::memory_order order = std::memory_order_relaxed) {
    auto v = AtomicTag{status.load(std::memory_order_relaxed).tag, s};
    status.store(v, order);
  }

  void setTag(Tag tag, std::memory_order order = std::memory_order_relaxed) {
    auto v = AtomicTag{tag, status.load(std::memory_order_relaxed).status};
    status.store(v, order);
  }

  CacheStatus getStatus(std::memory_order order = std::memory_order_relaxed) {
    return status.load(order).status;
  }

  Tag getTag(std::memory_order order = std::memory_order_relaxed) {
    return status.load(order).tag;
  }

  uint64_t getTimeStamp(std::memory_order order = std::memory_order_relaxed) {
#ifdef LRU_EVICT
    return timestamp.load(order);
#else
    return 0;
#endif
  }

  void setTimeStamp(std::memory_order order = std::memory_order_relaxed) {
#ifdef LRU_EVICT
    timestamp.store(asm_rdtsc(), order);
#endif
  }

  AtomicTag
  getTagAndStatus(std::memory_order order = std::memory_order_relaxed) {
    return status.load(order);
  }

  void setInvalid(std::memory_order order = std::memory_order_relaxed) {
    status.store(AtomicTag{InvalidTag, CacheStatus::INVALID});
  }

  void setTagAndStatus(Tag tag, CacheStatus s,
                       std::memory_order order = std::memory_order_relaxed) {
    status.store(AtomicTag{tag, s}, order);
  }

  bool cas(Tag tag, CacheStatus from, CacheStatus to) {
    AtomicTag a{tag, from}, b{tag, to};

    return status.compare_exchange_strong(a, b);
  }

  bool cas(CacheStatus from, CacheStatus to) {
    Tag t = status.load(std::memory_order_relaxed).tag;

    return cas(t, from, to);
  }

  bool casAllWithStatus(CacheStatus e, AtomicTag t) {
    AtomicTag et{status.load(std::memory_order_relaxed).tag, e};

    return status.compare_exchange_strong(et, t);
  }
};

struct LineSet {
  LineInfo header[CACHE_WAYS];
};

class Cache {

public:
  friend class DSM;
  friend class CacheAgent;

private:
  Cache(const CacheConfig &conf, DSM *dsm);
  void readLine(const GlobalAddress &addr, uint16_t start, uint16_t size,
                void *to);
  void writeLine(const GlobalAddress &addr, uint16_t start, uint16_t size,
                 const void *from);

  // locks
  bool r_lock(const GlobalAddress &addr, uint32_t size);
  void r_unlock(const GlobalAddress &addr, uint32_t size);
  bool w_lock(const GlobalAddress &addr, uint32_t size);
  void w_unlock(const GlobalAddress &addr, uint32_t size);

  // alloc
  GlobalAddress malloc(size_t, bool align);
  void free(const GlobalAddress &);

  bool findLine(const GlobalAddress &addr, LineInfo *&line);
  LineInfo *findLineForAgent(uint16_t nodeID, DirKey dirKey);

  static int get_evict_index(LineSet &set, LineInfo *&line);
  bool evictLine(LineInfo *line, CacheStatus c, const GlobalAddress &addr);
  bool evictLineShared(const GlobalAddress &addr, LineInfo *line);
  bool evictLineDirty(const GlobalAddress &addr, LineInfo *line);

  bool readMiss(const GlobalAddress &addr, LineInfo *info);
  bool writeMiss(const GlobalAddress &addr, LineInfo *info);
  bool writeShared(const GlobalAddress &addr, LineInfo *info);

  void registerThread();

  void sendMessage2Dir(LineInfo *info, RawMessageType type, uint8_t dirNodeID,
                       uint32_t dirKey, bool enter_pipeline = true);
  void sendUnlock(uint16_t bitmap, uint8_t state, uint8_t dirNodeID,
                  uint32_t dirKey,
                  RawMessageType type = RawMessageType::R_UNLOCK);

  LineSet *cacheHeader;
  void *cacheData;
  uint64_t cacheSize;

  RemoteConnection *remoteInfo;
  DSM *dsm;

  // CacheStat stat;

  // thread local for DSM applications
  static thread_local int iId;
  static thread_local ThreadConnection *iCon;
  static thread_local uint32_t seed;
  static thread_local uint64_t evict_time;
  static thread_local bool pending_unlock_ack;
  static thread_local uint32_t next_allocator_node;
  static thread_local LocalAllocator *alloc;
  static thread_local uint16_t unlock_flow_control;

  const static uint16_t kUnlockBatchAck = 8;

  ~Cache();

  // send with RC
  void sendMsgWithRC(RawMessage *m, ibv_qp *qp);
};

#endif /* __CACHE_H__ */
