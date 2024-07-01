#include "SwitchManager.h"
#include "Controller.h"

SwitchManager::SwitchManager(Controller *controller, uint16_t nodeID)
    : inSwitch(std::unordered_set<uint32_t>(INIT_TABLE_SIZE)),
      controller(controller),
      nodeID(nodeID), index(0) {}

AddReturn SwitchManager::addEntry(uint32_t dirKey) {
    if (inSwitch.find(dirKey) != inSwitch.end()) {
        return AddReturn::EXIST;
    }

    if (controller->addEntry(dirKey, index, nodeID)) {
        inSwitch.insert(dirKey);
        index++;
        return AddReturn::ADD_SUCCESS;
    }
    return AddReturn::ADD_FAIL;
}

AddReturn SwitchManager::addEntryWithOutLock(uint32_t dirKey) {
    if (inSwitch.find(dirKey) != inSwitch.end()) {
        return AddReturn::EXIST;
    }

    if (controller->addEntryWithOutLock(dirKey, index, nodeID)) {
        inSwitch.insert(dirKey);
        index++;
        return AddReturn::ADD_SUCCESS;
    }
    return AddReturn::ADD_FAIL;
}


void SwitchManager::addAll(uint16_t machineNR) {
    uint32_t dirKey = 0;

    for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 100000; ++j) {
                if (j % machineNR == nodeID) {
                    controller->addEntryWithOutLock(dirKey++, j, i);
                }
            }
    }
}
