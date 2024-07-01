#ifndef __CACHESTATUS_H__
#define __CACHESTATUS_H__

#include <cstdint>
#include <cassert>

enum CacheStatus : uint8_t {
  INVALID = 0,
  SHARED = 1,
  MODIFIED,

  BEING_SHARED,

  BEING_INVALID,

  BEING_FILL,

  BEING_EVICT,

};

inline const char *strCacheStatus(CacheStatus s) {
  switch (s) {
  case CacheStatus::INVALID:
    return "INVALID";
  case CacheStatus::SHARED:
    return "SHARED";
  case CacheStatus::MODIFIED:
    return "MODIFIED";
  case CacheStatus::BEING_SHARED:
    return "BEING_SHARED";
  case CacheStatus::BEING_INVALID:
    return "BEING_INVALID";
  case CacheStatus::BEING_FILL:
    return "BEING_FILL";
  case CacheStatus::BEING_EVICT:
    return "BEING_EVICT";
    assert(false);
  }
}

#endif /* __CACHESTATUS_H__ */
