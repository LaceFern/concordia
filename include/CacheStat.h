#ifndef __CACHESTAT_H__
#define __CACHESTAT_H__ 

#include <cstdint>
#include <cstring>
#include <string>

struct CacheStat {
    uint64_t hit;

    uint64_t read_miss_unshared;
    uint64_t read_miss_shared;
    uint64_t read_miss_dirty;
    uint64_t read_miss_lock_fail;
    uint64_t read_miss_check_fail;

    uint64_t write_miss_unshared;
    uint64_t write_miss_shared;
    uint64_t write_miss_dirty;
    uint64_t write_miss_lock_fail;
    uint64_t write_miss_check_fail;

    uint64_t write_shared;
    uint64_t write_shared_lock_fail;
    uint64_t write_shared_check_fail;

    CacheStat() {
        memset(this, 0, sizeof(*this));
    }

    std::string toString() {
        std::string s = "";
        s.append("hit:\t" + std::to_string(hit) + "\n");

        s.append("read_miss_unshared:\t" + std::to_string(read_miss_unshared) + "\n");
        s.append("read_miss_shared:\t" + std::to_string(read_miss_shared) + "\n");
        s.append("read_miss_dirty:\t" + std::to_string(read_miss_dirty) + "\n");
        s.append("read_miss_lock_fail:\t" + std::to_string(read_miss_lock_fail) + "\n");
        s.append("read_miss_check_fail:\t" + std::to_string(read_miss_check_fail) + "\n");

        s.append("write_miss_unshared:\t" + std::to_string(write_miss_unshared) + "\n");
        s.append("write_miss_shared:\t" + std::to_string(write_miss_shared) + "\n");
        s.append("write_miss_dirty:\t" + std::to_string(write_miss_dirty) + "\n");
        s.append("write_miss_lock_fail:\t" + std::to_string(write_miss_lock_fail) + "\n");
        s.append("write_miss_check_fail:\t" + std::to_string(write_miss_check_fail) + "\n");

        s.append("write_shared:\t" + std::to_string(write_shared) + "\n");
        s.append("write_shared_lock_fail:\t" + std::to_string(write_shared_lock_fail) + "\n");
        s.append("write_shared_check_fail:\t" + std::to_string(write_shared_check_fail) + "\n");


        return s;
    }

};

#endif /* __CACHESTAT_H__ */
