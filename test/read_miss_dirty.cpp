#include <iostream>
#include <thread>

#include "DSM.h"

#define PAGE_SIZE 4096

DSM *dsm;
GlobalAddress addr;

void concurrent(int i) {
  dsm->registerThread();

  char h[] = "hello";

  while (true) {
    dsm->write(addr, 5, (uint8_t *)h);
    printf("%d -\n", i);
  }
}

int main() {

  DSMConfig conf(CacheConfig(), 3);
  dsm = DSM::getInstance(conf);

  dsm->registerThread();

  if (dsm->getMyNodeID() == 0) {
    char *m = (char *)dsm->baseAddr;
    m[PAGE_SIZE + 12] = '1';
    m[PAGE_SIZE + 13] = '2';
    m[PAGE_SIZE + 14] = '3';
    m[PAGE_SIZE + 15] = '4';
  }

  addr.nodeID = 0;
  addr.addr = PAGE_SIZE + 12;

  uint8_t b[3] = {'a', 'b'};

  if (dsm->getMyNodeID() == 1) {

    // for (int i = 0; i < 3; ++i) {
    //   new std::thread(concurrent, i);
    // }

    // sleep(2);

    uint8_t a[3];
    // dsm->read(addr, 3, a);

    dsm->write(addr, 2, b);
    // addr.addr += PAGE_SIZE;
    // dsm->write(addr, 2, b);

  } else if (dsm->getMyNodeID() == 2) {
    sleep(3);

    uint8_t a[3];
    dsm->read(addr, 3, a);
    std::cout << a[0] << a[1] << a[2] << std::endl;
  }

  std::cout << "Hello, World!\n";

  while (true)
    ;
  return 0;
}
