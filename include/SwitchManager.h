#ifndef __SWITCHMANAGER_H__
#define __SWITCHMANAGER_H__

#include <unordered_set>
#include <cstdint>

enum class AddReturn {
    EXIST,
    ADD_SUCCESS,
    ADD_FAIL,
};

class Controller;
class SwitchManager {
   private:
    const static uint32_t INIT_TABLE_SIZE = 100000ull;
    std::unordered_set<uint32_t> inSwitch;
    Controller *controller;
    uint16_t nodeID;
    uint64_t index;

   public:
    SwitchManager(Controller *controller, uint16_t nodeID);

    AddReturn addEntry(uint32_t dirKey);
    AddReturn addEntryWithOutLock(uint32_t dirKey);

    void addAll(uint16_t machineNR);
};
#endif /* __SWITCHMANAGER_H__ */
