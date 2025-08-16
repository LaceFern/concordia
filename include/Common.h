#ifndef __COMMON_H__
#define __COMMON_H__

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <bitset>

#include "Debug.h"
#include "HugePageAlloc.h"
#include "Rdma.h"

#include "Statistics.h"
#include "WRLock.h"

// for baseline, home agents aggregate invalidation ack
// #define BASELINE

#define UNLOCK_SYNC
// #define R_W_CC
#define READ_MISS_DIRTY_TO_DIRTY

// #define VERBOSE
#define DEADLOCK_DETECTION
// #define LRU_EVICT

#define LOCK_BASE_ADDR

#define NUMA_CORE_NUM 12

#define MAX_MACHINE 8

#define ADD_ROUND(x, n) ((x) = ((x) + 1) % (n))

// { cache

typedef uint32_t Tag;
typedef uint64_t DirKey;

#define DSM_CACHE_LINE_WIDTH (10) // 4K
// #define DSM_CACHE_LINE_WIDTH (9) // 512
#define DSM_CACHE_LINE_SIZE (1u << DSM_CACHE_LINE_WIDTH)

#define DSM_CACHE_INDEX_WIDTH (18)
#define DSM_CACHE_INDEX_SIZE (1u << DSM_CACHE_INDEX_WIDTH)

#define CACHE_WAYS (8)


#define DirKey2Addr(x) (x << DSM_CACHE_LINE_WIDTH)

// }

#define MESSAGE_SIZE 96 // byte

#define POST_RECV_PER_RC_QP 1024

#define RAW_RECV_CQ_COUNT 32000

// { app thread
#define MAX_APP_THREAD 9

#define APP_MESSAGE_NR 1024

#define APP_POST_IMM_RECV 1024
// }

// { cache agent thread
#define NR_CACHE_AGENT 1

#define AGENT_MESSAGE_NR 1024

// { dir thread
#define NR_DIRECTORY 1

#define DIR_MESSAGE_NR 1024
// }

void bindCore(uint16_t core);
char *getIP();
char *getMac();

inline uint16_t toBigEndian16(uint16_t v) {
  uint16_t res;

  uint8_t *a = (uint8_t *)&v;
  uint8_t *b = (uint8_t *)&res;

  b[0] = a[1];
  b[1] = a[0];

  return res;
}

inline uint32_t toBigEndian32(uint32_t v) { return __builtin_bswap32(v); }

inline uint64_t toBigEndian64(uint64_t v) { return __builtin_bswap64(v); }

inline int bits_in(std::uint64_t u) {
  auto bs = std::bitset<64>(u);
  return bs.count();
}

namespace define {
constexpr uint64_t MB = 1024ull * 1024;
constexpr uint64_t GB = 1024ull * MB;
constexpr uint16_t kCacheLineSize = 64;

constexpr uint64_t kChunkSize = MB * 32; // MB
} // namespace define

static inline unsigned long long asm_rdtsc(void) {
  unsigned hi, lo;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#endif /* __COMMON_H__ */
