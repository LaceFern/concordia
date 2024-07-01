#include <iostream>
#include <thread>

#include "DSM.h"

#define PAGE_SIZE 4096

int main() {

  DSMConfig conf(CacheConfig(), 3);
  auto dsm = DSM::getInstance(conf);

  dsm->registerThread();

  GlobalAddress addr;
  addr.nodeID = 1;

  char buf[32] = "hello, world";
  if (dsm->getMyNodeID() != 1) {

    // for (uint64_t i = 0; i < 1 + CACHE_WAYS; ++i) {
    //   addr.addr = (i << (DSM_CACHE_INDEX_WIDTH + DSM_CACHE_LINE_WIDTH)) + 23;
    //   dsm->write(addr, 12, (uint8_t *)buf);
    // }

    printf("--- read ---\n");

    for (uint64_t i = 0; i < CACHE_WAYS; ++i) {

      char res[32];
      res[0] = 'q';
      addr.addr = (i << (DSM_CACHE_INDEX_WIDTH + DSM_CACHE_LINE_WIDTH)) + 23;
      dsm->read(addr, 12, (uint8_t *)res);

      // for (int k = 0; k < 12; ++k) {
      //   assert(res[k] = buf[k]);
      // }
    }
  }

  std::cout << "Hello, World!\n";

  while (true)
    ;
  return 0;
}
