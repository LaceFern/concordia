#include <iostream>
#include <thread>

#include "DSM.h"

#define PAGE_SIZE 4096

int main() {

    DSMConfig conf(CacheConfig(), 3);
    auto dsm = DSM::getInstance(conf);

    dsm->registerThread();

    if (dsm->getMyNodeID() == 0) {
        char *m = (char *)dsm->baseAddr;
        m[PAGE_SIZE + 12] = '1';
        m[PAGE_SIZE + 13] = '2';
        m[PAGE_SIZE + 14] = '3';
        m[PAGE_SIZE + 15] = '4';
    }

    GlobalAddress addr;
    addr.nodeID = 0;
    addr.addr = PAGE_SIZE + 12;

    uint8_t b[3] = {'a', 'b'};

    if (dsm->getMyNodeID() == 1) {

        sleep(2);

        dsm->write(addr, 2, b);

    } else if (dsm->getMyNodeID() == 2) {
        sleep(3);

        uint8_t a[3];
        dsm->write(addr, 3, a);
        // std::cout << a[0] << a[1] << a[2] << std::endl;
    }

    std::cout << "Hello, World!\n";

    while (true)
        ;
    return 0;
}
