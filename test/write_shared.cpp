#include <iostream>
#include <thread>

#include "DSM.h"

#define PAGE_SIZE 4096

DSM *dsm;
GlobalAddress addr;

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
  } else {
    sleep(1);
  }

  addr.nodeID = 0;
  addr.addr = PAGE_SIZE + 12;

  uint8_t b[3] = {'a', 'b'};

  dsm->read(addr, 3, b);
  printf("%c %c %c\n", b[0], b[1], b[2]);

  sleep(1);
  if (dsm->getMyNodeID() == 1) {
    dsm->write(addr, 3, b);
  }

  std::cout << "Hello, World!\n";

  while (true)
    ;
  return 0;
}
