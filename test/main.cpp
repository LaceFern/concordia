#include <iostream>
#include <thread>

#include "DSM.h"

#define PAGE_SIZE 4096

int main() {

    DSMConfig conf(CacheConfig(), 8);
    auto dsm = DSM::getInstance(conf);

    dsm->registerThread();

    GlobalAddress addr1, addr2;
    addr1.nodeID = addr2.nodeID = 6;
    addr1.addr = 0;
    addr2.addr = 19886080;

    uint64_t s;
    if (dsm->getMyNodeID() == 0) {

        uint64_t a = 12345;
        uint64_t v = 1111;

        dsm->write(addr1, sizeof(uint64_t), (uint8_t *)&a);
        dsm->write(addr2, sizeof(uint64_t), (uint8_t *)&v);
        dsm->read(addr1, sizeof(uint64_t), (uint8_t *)&s);
        Debug::notifyError("*** %d ***", s);
    }

    // if (dsm->getMyNodeID() == 0) {
        // char *m = (char *)dsm->baseAddr;
        // m[13 * PAGE_SIZE + 12] = '1';
        // m[13 * PAGE_SIZE + 13] = '2';
        // m[13 * PAGE_SIZE + 14] = '3';
        // m[13 * PAGE_SIZE + 15] = '4';
    // }

    // GlobalAddress addr;
    // addr.nodeID = 0;
    // addr.addr = 13 * PAGE_SIZE + 12;

    // uint8_t b[3];

    // if (dsm->getMyNodeID() == 1) {

        // sleep(2);

        // dsm->read(addr, 3, b);
        // Debug::notifyInfo("----------");
        // std::cout << b[0] << b[1] << b[2] << std::endl;

    // } else if (dsm->getMyNodeID() == 0) {
        // sleep(3);
        // dsm->read(addr, 3, b);
        // Debug::notifyInfo("----------");
        // std::cout << b[0] << b[1] << b[2] << std::endl;
    // } else if (dsm->getMyNodeID() == 2) {
        // sleep(6);
        // dsm->read(addr, 3, b);
        // // Debug::notifyInfo("----------");
        // // std::cout << b[0] << b[1] << b[2] << std::endl;
        // dsm->write(addr, 3, b);

    // }

    // std::cout << "Hello, World!\n";

    while (true)
        ;
    return 0;
}
