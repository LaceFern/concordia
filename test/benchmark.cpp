#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
// #include <gperftools/profiler.h>
#include <iostream>
#include <random>
#include <thread>

#include "DSM.h"

// #define SHOW_LATENCY

#define MAX_THREAD 5

#define OP_NUMS 2000000
#define OBJ_SIZE 8

#define MB (1024ull * 1024)

const uint64_t BLOCK_SIZE = uint64_t(MB * 62);

#define SUB_PAGE_SIZE (512)
#define STEPS (BLOCK_SIZE * nodeNR / SUB_PAGE_SIZE)

extern thread_local uint64_t evict_time;

GlobalAddress *access_[MAX_THREAD];
double latency[OP_NUMS];

int nodeNR = 4;
int threadNR = 4;
int readNR = 0;
int locality = 0;
int sharing = 0;

int round_cnt = 0;

thread_local uint32_t seed;

std::thread *th[MAX_THREAD];
DSM *dsm;

std::atomic<int> init{0};
std::atomic<int> ready{0};
std::atomic<int> finish{0};

char *output_file = nullptr;

void init_trace(int nodeID, int threadID) {
  auto traces = new GlobalAddress[STEPS];

  int id = nodeID * threadNR + threadID;
  int all = nodeNR * threadNR;

  uint64_t addrStart = id * BLOCK_SIZE;
  uint64_t addrSize = BLOCK_SIZE;

  uint64_t sharingStart = (all + 1) * BLOCK_SIZE;
  uint64_t sharingSize = BLOCK_SIZE;

  seed = time(NULL) + nodeID * 10 + threadID;

  int node = 0;
  uint64_t offset = 0;
  for (int op = 0; op < STEPS; ++op) {
    bool isSharing = (rand_r(&seed) % 100) < sharing;
    GlobalAddress addr;

    if (!isSharing) {
      addr.nodeID = node;
      addr.addr = addrStart + offset;
      node = (node + 1) % nodeNR;
      if (node == 0) {
        offset += SUB_PAGE_SIZE;
      }
    } else {
      addr.nodeID = rand_r(&seed) % nodeNR;
      addr.addr = sharingStart + rand_r(&seed) % sharingSize;
    }

    traces[op] = addr;
  }

  // generate access_
  access_[threadID] = new GlobalAddress[OP_NUMS];
  auto thread_access_ = access_[threadID];

  thread_access_[0] = traces[rand_r(&seed) % STEPS];
  GlobalAddress next = thread_access_[0];
  for (int i = 1; i < OP_NUMS; ++i) {
    bool isLocality = (rand_r(&seed) % 100) < locality;
    if (isLocality) {
      uint64_t offset = next.addr % 4096;
      if (offset + OBJ_SIZE + OBJ_SIZE < 4096) {
        next.addr += OBJ_SIZE;
      } else {
        next.addr -= offset;
      }

    } else {
      next = traces[rand_r(&seed) % STEPS];
    }

    thread_access_[i] = next;
  }

  delete[] traces;
}

void benchmark(int nodeID, int threadID, const std::string &prefix);
void start_thread(int nodeID, int threadID) {

  bindCore(threadID);

  dsm->registerThread();

  benchmark(nodeID, threadID, "warmup"); // warmup

  init.fetch_add(1);
  while (init.load() != threadNR)
    ;

  if (threadID == 0) {
    dsm->keeper->barrier(std::string("benchmark-") + std::to_string(round_cnt));
    // printf("node %d start benchmark\n", nodeID);
  }

  ready.fetch_add(1);
  Statistics::clear();
  while (ready.load() != threadNR)
    ;

  benchmark(nodeID, threadID, "benchmark");
  finish.fetch_add(1);

  // Debug::notifyInfo("node %d thread %d finish, evict time %llu", nodeID,
  //                   threadID, evict_time);
}

void benchmark(int nodeID, int threadID, const std::string &prefix) {

  GlobalAddress *thread_access_ = access_[threadID];

  uint8_t from[OBJ_SIZE];
  uint8_t to[OBJ_SIZE];
  timespec s, e;
  
  for (int op = 0; op < OP_NUMS; ++op) {

    memset(from, 0, OBJ_SIZE);

    // if (op % (OP_NUMS / 10) == 0) {
    //   printf("^%s, node %d thread %d finish %f%%\n", prefix.c_str(), nodeID,
    //          threadID, op * 1.0 / OP_NUMS * 100);
    // }

    bool isRead = (rand_r(&seed) % 100) < readNR;

#ifdef SHOW_LATENCY
    clock_gettime(CLOCK_REALTIME, &s);
#endif

    if (isRead) {
      dsm->read(thread_access_[op], OBJ_SIZE, to);
    } else {
      dsm->write(thread_access_[op], OBJ_SIZE, from);
    }

#ifdef SHOW_LATENCY
    clock_gettime(CLOCK_REALTIME, &e);

    if (threadID == 0) {
      latency[op] = (e.tv_sec - s.tv_sec) * 1000000 +
                    (double)(e.tv_nsec - s.tv_nsec) / 1000;
    }
#endif
  }
}

void parserArgs(int argc, char **argv) {

  if (argc != 6 && argc != 7) {
    fprintf(
        stderr,
        "Usage: ./benchmark nodeNR threadNR readNR locality sharing output \n");
    exit(-1);
  }

  nodeNR = std::atoi(argv[1]);
  threadNR = std::atoi(argv[2]);
  readNR = std::atoi(argv[3]);
  locality = std::atoi(argv[4]);
  sharing = std::atoi(argv[5]);

  if (argc == 7) {
    output_file = argv[6];
  }

  fprintf(stdout,
          "Benchmark Config: nodeNR %d, threadNR %d, readNR %d, locality %d, "
          "sharing %d\n",
          nodeNR, threadNR, readNR, locality, sharing);
}

int main(int argc, char **argv) {

  parserArgs(argc, argv);

  DSMConfig conf(CacheConfig(), nodeNR, 32); // 4G per node;
  dsm = DSM::getInstance(conf);

  // retry:

  // init
  for (int i = 0; i < threadNR; ++i) {
    th[i] = new std::thread(init_trace, dsm->getMyNodeID(), i);
  }
  for (int i = 0; i < threadNR; ++i) {
    th[i]->join();
  }

  init.store(0);
  ready.store(0);
  finish.store(0);

  // benchmark
  // ProfilerStart("/tmp/dsm-perf");
  for (int i = 0; i < threadNR; ++i) {
    th[i] = new std::thread(start_thread, dsm->getMyNodeID(), i);
  }

  timespec s, e;

  while (ready.load() != threadNR)
    ;

  clock_gettime(CLOCK_REALTIME, &s);

  while (finish.load() != threadNR)
    ;

  clock_gettime(CLOCK_REALTIME, &e);

  double microseconds =
      (e.tv_sec - s.tv_sec) * 1000000 + (double)(e.tv_nsec - s.tv_nsec) / 1000;

  uint64_t ops = threadNR * OP_NUMS;
  uint64_t tp = ops * 1000000 / microseconds;

  auto all_tp =
      dsm->keeper->sum(std::string("tp") + std::to_string(round_cnt), tp);

  if (dsm->myNodeID == 0) {
    fprintf(stderr, "%d %d-tp: %llu op/s; all-tp: %llu\n", sharing,
            dsm->getMyNodeID(), tp, all_tp);
  }

  for (int i = 0; i < threadNR; ++i) {
    th[i]->join();
    delete[] access_[i];
  }

#ifdef SHOW_LATENCY
  std::sort(std::begin(latency), std::end(latency));
  printf("median %f\n", latency[OP_NUMS / 2]);
  printf("p99 %f\n", latency[OP_NUMS / 100 * 99]);
#endif

  // dsm->reset();
  // sharing += 10;
  // if (sharing <= 100) {
  //   round_cnt++;
  //   goto retry;
  // }

  uint64_t recv_c = dsm->keeper->sum("dir_recv", Statistics::dir_recv_all());
  uint64_t send_c = dsm->keeper->sum("dir_send", Statistics::dir_send_all());

  if (dsm->myNodeID == 0) {
    printf("recv %lu,  send %lu\n", recv_c, send_c);
  }
  if (output_file != nullptr && dsm->myNodeID == 0) {
    std::ofstream f(std::string(output_file), std::ios::app);
    if (f) {
      f << "[" << nodeNR << "," << threadNR << "," << readNR << "," << locality
        << "," << sharing << "]" << std::endl;
      f << all_tp << " , recv: " << recv_c << ", send: " << send_c << "\n"
        << std::endl;
    }
    f.close();
  }

  sleep(1);

  // std::cout << "Hello, End\n";

  // Statistics::dispaly();

  // ProfilerStop();

  // while (true)
  //   ;

  return 0;
}
