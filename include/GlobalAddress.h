#ifndef __GLOBALADDRESS_H__
#define __GLOBALADDRESS_H__

#include "Common.h"
#include "Hash.h"

#define HASH_INDEX

class GlobalAddress {
public:
  uint64_t addr;
  uint8_t nodeID;

  uint32_t getIndex() const {
#ifdef HASH_INDEX
    uint32_t v = getTag();
    return hash::jenkins(&v, sizeof(v)) % DSM_CACHE_INDEX_SIZE;
#else
    return (addr >> DSM_CACHE_LINE_WIDTH) & (DSM_CACHE_INDEX_SIZE - 1);
#endif
  }

  Tag getTag() const {
#ifdef HASH_INDEX
    return ((getDirKey() << 4) + nodeID);
#else
    return (addr >> (DSM_CACHE_INDEX_WIDTH + DSM_CACHE_LINE_WIDTH) << 8) +
           nodeID;
#endif
  }

  DirKey getDirKey() const { return addr >> DSM_CACHE_LINE_WIDTH; }

  static GlobalAddress genGlobalAddrFormIndexTag(uint32_t index, Tag tag) {
    GlobalAddress res;

#ifdef HASH_INDEX
    res.nodeID = tag & 0xf;
    res.addr = (tag >> 4) << DSM_CACHE_LINE_WIDTH;
#else
    res.nodeID = tag & 0xff;
    res.addr = (((uint64_t)tag >> 8)
                << (DSM_CACHE_INDEX_WIDTH + DSM_CACHE_LINE_WIDTH)) |
               ((uint64_t)index << DSM_CACHE_LINE_WIDTH);
#endif

    return res;
  }

  void print(const char *s) { printf("%s [%d, %lx]\n", s, nodeID, addr); }

  GlobalAddress operator+(int off) {
    auto ret = *this;
    ret.addr += off;
    return ret;
  }

  static GlobalAddress Null() {
    static GlobalAddress zero{0, 0};
    return zero;
  }
} __attribute__((packed));

inline GlobalAddress GADD(const GlobalAddress &addr, int off) {
  auto ret = addr;
  ret.addr += off;
  return ret;
}

inline bool operator==(const GlobalAddress &lhs, const GlobalAddress &rhs) {
  return (lhs.nodeID == rhs.nodeID) && (lhs.addr == rhs.addr);
}

inline bool operator!=(const GlobalAddress &lhs, const GlobalAddress &rhs) {
  return !(lhs == rhs);
}

inline std::ostream &operator<<(std::ostream &os, const GlobalAddress &obj) {
  os << "[" << (int)obj.nodeID << ", " << obj.addr << "]";
  return os;
}

#endif /* __GLOBALADDRESS_H__ */
