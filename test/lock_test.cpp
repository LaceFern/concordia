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

  GlobalAddress lock_addr;
  lock_addr.nodeID = 2;
  lock_addr.addr = 4096 * 7;
  if (dsm->getMyNodeID() == 0) {
    assert(dsm->r_lock(lock_addr) == true);
  } else if (dsm->getMyNodeID() == 1) {
    assert(dsm->r_lock(lock_addr) == true);
  } else {
    sleep(1);
    assert(dsm->w_lock(lock_addr) == false);
  }

  sleep(3);

  if (dsm->getMyNodeID() != 2) {
    dsm->r_unlock(lock_addr);
  }

  sleep(3);

  if (dsm->getMyNodeID() == 2) {
   assert(dsm->w_lock(lock_addr) == true);

   dsm->w_unlock(lock_addr);
  }



  std::cout << "Hello, World!\n";

  while (true)
    ;
  return 0;
}
