#include <iostream>
#include <thread>

#include "DSM.h"

DSM *dsm;

int main() {

  DSMConfig conf(CacheConfig(), 1);
  dsm = DSM::getInstance(conf);

  dsm->registerThread();

  GlobalAddress addr;
  addr.nodeID = 0;
  addr.addr = 1;
  uint8_t buf[5];
  dsm->read(addr, 4, buf);

  while (true)
    ;
  return 0;
}
