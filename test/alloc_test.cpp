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

  if (dsm->getMyNodeID() == 1) {

    auto res = dsm->malloc(define::kChunkSize);
    res.print("A ");

    res = dsm->malloc(define::kChunkSize);
    res.print("A ");

    res = dsm->malloc(define::kChunkSize);
    res.print("A ");

    res = dsm->malloc(10247);
    res.print("A ");

    res = dsm->malloc(11);
    res.print("A ");
  }

  std::cout << "Hello, World!\n";

  while (true)
    ;
  return 0;
}
