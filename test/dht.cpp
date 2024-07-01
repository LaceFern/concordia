#include <cstdio>
#include <fstream>
// #include <gperftools/profiler.h>
#include <iostream>
#include <thread>

#include "DSM.h"
#include "zipf.h"

#define MAX_THREAD 24
#define PAGE_SIZE 4096

char *output_file = nullptr;

#define KEY_SIZE 8
#define VALUE_SIZE 128

struct Slot {
  uint64_t k;
  char v[VALUE_SIZE];
};

#define OP_NUMS 400000

#define MB (1024ull * 1024)

#define KEY_SPACE (64ull * MB)

int nodeNR = 0;
int threadNR = 0;
int readNR = 95;

std::thread *th[MAX_THREAD];
DSM *dsm;

#define SIZE (1 << 20)

uint64_t *key_traces[MAX_THREAD];
thread_local uint32_t seed;

std::atomic<int> init{0};
std::atomic<int> ready{0};
std::atomic<int> finish{0};


#define BIG_CONSTANT(x) (x##LLU)
inline uint64_t
MurmurHash64A(const void *key, int len, uint64_t seed = 931901)
{
	const uint64_t m = BIG_CONSTANT(0xc6a4a7935bd1e995);
	const int r = 47;

	uint64_t h = seed ^ (len * m);

	const uint64_t *data = (const uint64_t *)key;
	const uint64_t *end = data + (len / 8);

	while (data != end) {
		uint64_t k = *data++;

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	const unsigned char *data2 = (const unsigned char *)data;

	switch (len & 7) {
		case 7:
			h ^= ((uint64_t)data2[6]) << 48;
		case 6:
			h ^= ((uint64_t)data2[5]) << 40;
		case 5:
			h ^= ((uint64_t)data2[4]) << 32;
		case 4:
			h ^= ((uint64_t)data2[3]) << 24;
		case 3:
			h ^= ((uint64_t)data2[2]) << 16;
		case 2:
			h ^= ((uint64_t)data2[1]) << 8;
		case 1:
			h ^= ((uint64_t)data2[0]);
			h *= m;
	};

	h ^= h >> r;
	h *= m;
	h ^= h >> r;

	return h;
}

void init_trace(int nodeID, int threadID) {
  uint32_t seed = time(NULL) + nodeID * 10 + threadID;
  struct zipf_gen_state state;
  mehcached_zipf_init(&state, KEY_SPACE, 0.99, seed);

  key_traces[threadID] = new uint64_t[OP_NUMS];

  for (int i = 0; i < OP_NUMS; ++i) {
    key_traces[threadID][i] = mehcached_zipf_next(&state) + 1;
  }
}

void benchmark(int nodeID, int threadID, const std::string &prefix,
               bool is_warmup) {

  const int slot_per_page = PAGE_SIZE / sizeof(Slot);
  for (int op = 0; op < OP_NUMS; ++op) {

    //   if (!is_warmup) {
    //       printf("OP %d\n", op);
    //   }

    // if (op % (OP_NUMS / 20) == 0) {
    //     Debug::notifyInfo("%d %d%%\n", nodeID, op / (OP_NUMS / 20));
    // }

    bool isRead = rand_r(&seed) % 100 < readNR;
    if (is_warmup) {
      isRead = false;
    }

    uint64_t key =
        MurmurHash64A((char *)&key_traces[threadID][op], sizeof(uint64_t));

    uint8_t nodeID = (key >> 48) % nodeNR;                 // NODE
    uint64_t page = ((key << 16) >> 24) % (SIZE / nodeNR); // PAGE
    uint64_t page_slot = key % slot_per_page;

    GlobalAddress bucket;
    bucket.nodeID = nodeID;
    bucket.addr = (page + 1) * PAGE_SIZE + page_slot * sizeof(Slot);

    Slot slot;

    // isRead = true;
    if (isRead) {
      dsm->RLock(bucket, 1);
    } else {
      dsm->WLock(bucket, 1);
    }

    dsm->read(bucket, sizeof(Slot), (uint8_t *)&slot);

    uint8_t KV[VALUE_SIZE];
    if (isRead) {
      if (slot.k == key_traces[threadID][op]) {
        // find;
      }
    } else {
      if (slot.k == key_traces[threadID][op]) {
        dsm->write(GADD(bucket, KEY_SIZE), VALUE_SIZE, KV);
      } else {
        slot.k = key_traces[threadID][op];
        memcpy(slot.v, KV, VALUE_SIZE);
        dsm->write(bucket, sizeof(Slot), (uint8_t *)&slot);
      }
    }
    dsm->UnLock(bucket, 1);
  }
}

void start_thread(int nodeID, int threadID) {

  bindCore(threadID);

  dsm->registerThread();

  benchmark(nodeID, threadID, "warmup", true); // warmup

  init.fetch_add(1);
  while (init.load() != threadNR)
    ;

  if (threadID == 0) {
    dsm->keeper->barrier(std::string("benchmark"));
    printf("node %d start benchmark\n", nodeID);
  }

  ready.fetch_add(1);

  while (ready.load() != threadNR)
    ;

  benchmark(nodeID, threadID, "benchmark", false);
  finish.fetch_add(1);

  // Debug::notifyInfo("node %d thread %d finish, evict time %llu", nodeID,
  //                   threadID, evict_time);
}

void parserArgs(int argc, char **argv) {

  if (argc != 3 && argc != 4) {
    fprintf(stderr, "Usage: ./dht nodeNR threadNR\n");
    exit(-1);
  }

  nodeNR = std::atoi(argv[1]);
  threadNR = std::atoi(argv[2]);
  if (argc == 4) {
    output_file = argv[3];
  }

  fprintf(stdout, "Benchmark Config: nodeNR %d, threadNR %d, readNR %d\n",
          nodeNR, threadNR, readNR);
}

int main(int argc, char **argv) {

  parserArgs(argc, argv);

  DSMConfig conf(CacheConfig(), nodeNR);
  dsm = DSM::getInstance(conf);

  // init
  for (int i = 0; i < threadNR; ++i) {
    th[i] = new std::thread(init_trace, dsm->getMyNodeID(), i);
  }
  for (int i = 0; i < threadNR; ++i) {
    th[i]->join();
  }

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

  auto all_tp = dsm->keeper->sum(std::string("tp"), tp);

  // if (dsm->myNodeID == 0) {
  fprintf(stderr, "%d-tp: %llu op/s; all-tp: %llu\n", dsm->getMyNodeID(), tp,
          all_tp);
  //   }

  for (int i = 0; i < threadNR; ++i) {
    th[i]->join();
  }

  if (output_file != nullptr && dsm->myNodeID == 0) {
    std::ofstream f(std::string(output_file), std::ios::app);
    if (f) {
      f << "[" << nodeNR << "," << threadNR << "," << readNR << "]"
        << std::endl;
      f << all_tp << "\n" << std::endl;
    }
    f.close();
  }

  // std::cout << "Hello, End\n";

  // while (true)
  // ;
  sleep(1);

  return 0;
}
