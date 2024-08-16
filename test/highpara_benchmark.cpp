#include <algorithm>
#include <atomic>
#include <cstdio>
#include <fstream>
// #include <gperftools/profiler.h>
#include <iostream>
#include <random>
#include <thread>

#include "DSM.h"
#include "agent_stat.h"

// #define SHOW_LATENCY

/***********************************/
/******** MY CODE STARTS ********/
#include <cstdint>
#include <chrono>
#include <vector>
using namespace std;
using namespace chrono;
// #define STEPS 204800//409600//1638400//409600
// long ITERATION = STEPS * 10;//STEPS * 10;//STEPS/160;//STEPS
int is_home = 0;
int is_cache = 0;
int is_request = 0;
int cache_rw = 0;
int request_rw = 0;
int breakdown_times = 1024;//1024;//204800;
const char *result_directory = "gam_result";
uint64_t breakdown_size = DSM_CACHE_LINE_SIZE * breakdown_times;
/******** MY CODE ENDS ********/
/***********************************/

#define MAX_THREAD 24

#define OP_NUMS 2000000 //2000000
#define OBJ_SIZE 8

#define MB (1024ull * 1024)

const uint64_t BLOCK_SIZE = uint64_t(MB * 31); //uint64_t(MB * 31); // 31 user application space required per node

#define SUB_PAGE_SIZE (512) // aligned byte granularity for user mem operation
#define STEPS (BLOCK_SIZE * nodeNR / SUB_PAGE_SIZE)

extern thread_local uint64_t evict_time;

GlobalAddress *access_[MAX_THREAD];
double latency[OP_NUMS];

int nodeNR = 0;
int threadNR = 0;
int readNR = 0;
int locality = 0;
int sharing = 0;

int round_cnt = 0;

thread_local uint32_t seed;

std::thread *th[MAX_THREAD];
DSM *dsm;

// std::atomic<int> init{0};
std::atomic<int> ready{0};
std::atomic<int> finish{0};
std::atomic<int> warmup_no_breakdown{0};
std::atomic<int> warmup_breakdown{0};

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

  /***********************************/
  /******** MY CODE STARTS ********/
  auto traces_breakdown = new GlobalAddress[breakdown_times];
  for (int op = 0; op < breakdown_times; ++op) {
    // shared addr
    GlobalAddress addr;
    addr.nodeID = agent_stats_inst.home_node_id;
    addr.addr = sharingStart + sharingSize + op * DSM_CACHE_LINE_SIZE;
    traces_breakdown[op] = addr;
  }
  /******** MY CODE ENDS ********/
  /***********************************/

  // generate access_
  access_[threadID] = new GlobalAddress[OP_NUMS + breakdown_times];
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

  /***********************************/
  /******** MY CODE STARTS ********/
  for (int i = 0; i < breakdown_times; ++i) {
    next = traces_breakdown[i];
    thread_access_[OP_NUMS + i] = next;

    if (threadID == 0) {
      agent_stats_inst.push_valid_gaddr(next.addr);
    }
  }


  // if (threadID == 0) {
  //   agent_stats_inst.print_valid_gaddr();
  // }
  /******** MY CODE ENDS ********/
  /***********************************/

  delete[] traces;
  delete[] traces_breakdown;
}

void Run_cache(int nodeID, int threadID, const std::string &prefix) {

  GlobalAddress *thread_access_ = access_[threadID];
  uint8_t from[OBJ_SIZE];
  uint8_t to[OBJ_SIZE];

  for (int op = 0; op < breakdown_times; ++op) {
    /***********************************/
    /******** MY CODE STARTS ********/
    // printf("checkpoint 1.1 on thread %d: op = %d\n", threadID, op);
    // fflush(stdout);

    if (is_cache == 1 && threadID == 0) {
        GlobalAddress to_access_breakdown = thread_access_[OP_NUMS + op];

        switch (cache_rw) {
        case 0: {
          dsm->Try_RLock(to_access_breakdown, OBJ_SIZE);
          dsm->read(to_access_breakdown, OBJ_SIZE, to);
          dsm->UnLock(to_access_breakdown, OBJ_SIZE);
          break;
        }

        case 1: {
          dsm->Try_WLock(to_access_breakdown, OBJ_SIZE);
          dsm->write(to_access_breakdown, OBJ_SIZE, from);
          dsm->UnLock(to_access_breakdown, OBJ_SIZE);
          break;
        }
        default: {
          break;
        }
      }
    }
    /******** MY CODE ENDS ********/
    /***********************************/
  }
}

void Run_request(int nodeID, int threadID, const std::string &prefix) {

  /***********************************/
  /******** MY CODE STARTS ********/
  int count_4_nobreakdown = 0;
  int count_4_breakdown = 0;
  // edited by cxz, multi 0.75 is used for let app thread 0 stop early than other app thread, so that we can get the "congestion" result
  int thres_4_nobreakdown = 0.75 * (OP_NUMS / (breakdown_times + 1)) + 1;
  /******** MY CODE ENDS ********/
  /***********************************/

  GlobalAddress *thread_access_ = access_[threadID];

  uint8_t from[OBJ_SIZE];
  uint8_t to[OBJ_SIZE];
  timespec s, e;

  for (int op = 0; op < OP_NUMS && count_4_breakdown < breakdown_times; ++op) {
    /***********************************/
    /******** MY CODE STARTS ********/
    if (is_request == 1 && prefix != "warmup" && threadID == 0) {

      // printf("checkpoint 1.1 on thread %d\n", threadID);
      // fflush(stdout);

      count_4_nobreakdown++;
      if (count_4_nobreakdown == thres_4_nobreakdown) {
        // printf("i = %d\n", i);
        count_4_nobreakdown = 0;
        GlobalAddress to_access_breakdown = thread_access_[OP_NUMS + count_4_breakdown];
        count_4_breakdown++;

        // TODO: thread_access_[op] --> to_access_breakdown
        switch (request_rw) {
        case 0: {
          // printf("checkpoint 1.1 on thread %d: Gaddr = %ld\n", threadID, to_access_breakdown.addr);
          agent_stats_inst.start_record_app_thread(to_access_breakdown.addr);
          dsm->Try_RLock(to_access_breakdown, OBJ_SIZE);
          agent_stats_inst.stop_record_app_thread_with_op(to_access_breakdown.addr, APP_THREAD_OP::WAKEUP_2_LOCK_RETURN);

          agent_stats_inst.start_record_app_thread(to_access_breakdown.addr);
          dsm->read(to_access_breakdown, OBJ_SIZE, to);
          agent_stats_inst.stop_record_app_thread_with_op(to_access_breakdown.addr, APP_THREAD_OP::WAKEUP_2_READ_RETURN);

          agent_stats_inst.start_record_app_thread(to_access_breakdown.addr);
          dsm->UnLock(to_access_breakdown, OBJ_SIZE);
          agent_stats_inst.stop_record_app_thread_with_op(to_access_breakdown.addr, APP_THREAD_OP::WAKEUP_2_UNLOCK_RETURN);
          break;
        }

        case 1: {
          // printf("checkpoint 1.1 on thread %d: Gaddr = %ld\n", threadID, to_access_breakdown.addr);
          agent_stats_inst.start_record_app_thread(to_access_breakdown.addr);
          dsm->Try_WLock(to_access_breakdown, OBJ_SIZE);
          agent_stats_inst.stop_record_app_thread_with_op(to_access_breakdown.addr, APP_THREAD_OP::WAKEUP_2_LOCK_RETURN);
          // printf("checkpoint 1.2 on thread %d: Gaddr = %ld\n", threadID, to_access_breakdown.addr);

          agent_stats_inst.start_record_app_thread(to_access_breakdown.addr);
          dsm->write(to_access_breakdown, OBJ_SIZE, from);
          agent_stats_inst.stop_record_app_thread_with_op(to_access_breakdown.addr, APP_THREAD_OP::WAKEUP_2_WRITE_RETURN);
          // printf("checkpoint 1.3 on thread %d: Gaddr = %ld\n", threadID, to_access_breakdown.addr);

          agent_stats_inst.start_record_app_thread(to_access_breakdown.addr);
          dsm->UnLock(to_access_breakdown, OBJ_SIZE);
          agent_stats_inst.stop_record_app_thread_with_op(to_access_breakdown.addr, APP_THREAD_OP::WAKEUP_2_UNLOCK_RETURN);
          // printf("checkpoint 1.4 on thread %d: Gaddr = %ld\n", threadID, to_access_breakdown.addr);

          break;
        }
        default: {
          break;
        }
        }
      }
    }
    /******** MY CODE ENDS ********/
    /***********************************/

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

      // printf("checkpoint 1.2 on thread %d: target node id = %d, op = %d\n", threadID, thread_access_[op].nodeID, op);
      // fflush(stdout);

      dsm->read(thread_access_[op], OBJ_SIZE, to);
    } else {

      // printf("checkpoint 1.3 on thread %d: target node id = %d, op = %d\n", threadID, thread_access_[op].nodeID, op);
      // fflush(stdout);

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

void start_thread(int nodeID, int threadID) {

  // printf("checkpoint 0 on thread %d\n", threadID);
  fflush(stdout);

  // // 总线程数少于等于12且在numa0
  // if(threadID < 12){
  //   bindCore(threadID);
  // }

  // // 总线程数少于等于24且在numa0
  // if(threadID < 12 - NR_CACHE_AGENT - NR_DIRECTORY){
  //   bindCore(threadID);
  // }
  // else{
  //   bindCore(threadID + 24);
  // }

  // app在numa1,sys在numa0
  if(threadID < 12){
    bindCore(12 + threadID);
  }
  else{
    bindCore(24 + threadID);
  }

  // printf("checkpoint 1 on thread %d\n", threadID);
  fflush(stdout);

  dsm->registerThread();

  // if (threadID == 0) {
  //   sleep(5);
  //   dsm->keeper->barrier(std::string("benchmark-") + std::to_string(round_cnt));
  //   // printf("node %d start benchmark\n", nodeID);
  // }

  // agent_stats_inst.waitForSpace();
  // printf("start warmup the cache for no-breakdown on thread %d\n", threadID);
  fflush(stdout);

  Run_request(nodeID, threadID, "warmup"); // warmup

  // printf("checkpoint 2 on thread %d\n", threadID);
  fflush(stdout);

  warmup_no_breakdown.fetch_add(1);
  while (warmup_no_breakdown.load() != threadNR) ;
  if (threadID == 0) {
    dsm->keeper->barrier(std::string("benchmark-") + std::to_string(round_cnt + 1));
    // printf("node %d start benchmark\n", nodeID);
  }

  // printf("start warmup the cache for breakdown on thread %d\n", threadID);
  fflush(stdout);

  Run_cache(nodeID, threadID, "warmup"); // warmup

  // printf("checkpoint 3 on thread %d\n", threadID);
  fflush(stdout);

  warmup_breakdown.fetch_add(1);
  while (warmup_breakdown.load() != threadNR) ;
  if (threadID == 0) {
    dsm->keeper->barrier(std::string("benchmark-") + std::to_string(round_cnt + 2));
    // printf("node %d start benchmark\n", nodeID);
  }

  // printf("checkpoint 4 on thread %d\n", threadID);
  fflush(stdout);

  ready.fetch_add(1);
  Statistics::clear();
  while (ready.load() != threadNR) ;
  if (threadID == 0) agent_stats_inst.start_collection();

  // printf("start run the benchmark on thread %d\n", threadID);
  fflush(stdout);

  Run_request(nodeID, threadID, "benchmark");
  finish.fetch_add(1);

  // Debug::notifyInfo("node %d thread %d finish, evict time %llu", nodeID,
  //                   threadID, evict_time);

  if (threadID == 0) {
    // agent_stats_inst.print_app_thread_stat();
    // agent_stats_inst.print_multi_poll_thread_stat();
    // agent_stats_inst.print_multi_sys_thread_stat();
    agent_stats_inst.save_stat_to_file(std::string(result_directory), agent_stats_inst.sys_thread_num, threadNR);
  }
}

// void start_thread(int nodeID, int threadID) {

//   bindCore(threadID);

//   dsm->registerThread();

//   benchmark(nodeID, threadID, "warmup"); // warmup

//   init.fetch_add(1);
//   while (init.load() != threadNR)
//     ;

//   if (threadID == 0) {
//     dsm->keeper->barrier(std::string("benchmark-") + std::to_string(round_cnt));
//     // printf("node %d start benchmark\n", nodeID);
//   }

//   ready.fetch_add(1);
//   Statistics::clear();
//   while (ready.load() != threadNR)
//     ;

//   benchmark(nodeID, threadID, "benchmark");
//   finish.fetch_add(1);

//   // Debug::notifyInfo("node %d thread %d finish, evict time %llu", nodeID,
//   //                   threadID, evict_time);
// }

void parserArgs(int argc, char **argv) {

  // if (argc != 6 && argc != 7) {
  //   fprintf(
  //       stderr,
  //       "Usage: ./benchmark nodeNR threadNR readNR locality sharing output \n");
  //   exit(-1);
  // }

  // nodeNR = std::atoi(argv[1]);
  // threadNR = std::atoi(argv[2]);
  // readNR = std::atoi(argv[3]);
  // locality = std::atoi(argv[4]);
  // sharing = std::atoi(argv[5]);

  // if (argc == 7) {
  //   output_file = argv[6];
  // }

  // fprintf(stdout,
  //         "Benchmark Config: nodeNR %d, threadNR %d, readNR %d, locality %d, "
  //         "sharing %d\n",
  //         nodeNR, threadNR, readNR, locality, sharing);

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--no_node") == 0) {
      nodeNR = atoi(argv[++i]);  //0..100
    }
    else if (strcmp(argv[i], "--no_thread") == 0) {
      threadNR = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--locality") == 0) {
      locality = atoi(argv[++i]);  //0..100
    } else if (strcmp(argv[i], "--shared_ratio") == 0) {
      sharing = atoi(argv[++i]);  //0..100
    } else if (strcmp(argv[i], "--read_ratio") == 0) {
      readNR = atoi(argv[++i]);  //0..100
    }

    /***********************************/
    /******** MY CODE STARTS ********/
    else if (strcmp(argv[i], "--is_cache") == 0) {
      is_cache = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--cache_rw") == 0) {
      cache_rw = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--is_request") == 0) {
      is_request = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--request_rw") == 0) {
      request_rw = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--is_home") == 0) {
      is_home = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--home_node_id") == 0) {
      agent_stats_inst.home_node_id = atoi(argv[++i]);
    }else if (strcmp(argv[i], "--breakdown_times") == 0) {
      breakdown_times = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--result_dir") == 0) {
      result_directory = argv[++i];  //0..100
    }
    /******** MY CODE ENDS ********/
    /***********************************/

    else {
      fprintf(stderr, "Unrecognized option %s for benchmark\n", argv[i]);
    }
  }

  fprintf(stdout,
          "Benchmark Config: nodeNR %d, threadNR %d, readNR %d, locality %d, "
          "sharing %d\n",
          nodeNR, threadNR, readNR, locality, sharing);

}

int main(int argc, char **argv) {

  parserArgs(argc, argv);

  /***********************************/
  /******** MY CODE STARTS ********/
  printf("My CC configuration is: ");
  printf("is_home = %d, is_cache = %d, cache_rw = %d, is_request = %d, request_rw = %d, breakdown_times = %d\n",
    is_home, is_cache, cache_rw, is_request, request_rw, breakdown_times);
  agent_stats_inst.is_cache = is_cache;
  agent_stats_inst.is_request = is_request;
  agent_stats_inst.is_home = is_home;
  agent_stats_inst.nr_dir = NR_DIRECTORY;
  agent_stats_inst.nr_cache_agent = NR_CACHE_AGENT;
  // printf("checkpoint -6 on main thread: agent_stats_inst.nr_dir = %d\n", agent_stats_inst.nr_dir);
  // printf("checkpoint -5 on main thread: agent_stats_inst.nr_cache_agent = %d\n", agent_stats_inst.nr_cache_agent);
  /******** MY CODE ENDS ********/
  /***********************************/

  uint64_t sys_total_size = 16; //GB
  DSMConfig conf(CacheConfig(), nodeNR, sys_total_size); // 4G per node;

  /***********************************/
  /******** MY CODE STARTS ********/
  cout << "conf.size = " << sys_total_size / nodeNR << "GB" << endl;
  cout << "----------" << endl;
  cout << "user access space = " << ((long)BLOCK_SIZE) * nodeNR * 1.0 / (1024 * 1024 * 1024) << "GB" << endl;
  cout << "user cache ratio = " << ((long)DSM_CACHE_INDEX_SIZE) * CACHE_WAYS * DSM_CACHE_LINE_SIZE / (BLOCK_SIZE * nodeNR) << endl;
  agent_stats_inst.end_collection();
  /******** MY CODE ENDS ********/
  /***********************************/

  dsm = DSM::getInstance(conf);

  /***********************************/
  /******** MY CODE STARTS ********/
  printf("benchmark init\n");
  agent_stats_inst.set1_thread_init_flag(0);
  /******** MY CODE ENDS ********/
  /***********************************/

  // retry:

  // init
  printf("checkpoint -4 on main thread\n");
  for (int i = 0; i < threadNR; ++i) {
    printf("checkpoint -3 on main thread\n");
    th[i] = new std::thread(init_trace, dsm->getMyNodeID(), i);
    printf("checkpoint -2 on main thread\n");
  }
  printf("checkpoint -1 on main thread\n");
  for (int i = 0; i < threadNR; ++i) {
    th[i]->join();
  }

  // init.store(0);
  warmup_no_breakdown.store(0);
  warmup_breakdown.store(0);
  ready.store(0);
  finish.store(0);

  // benchmark
  // ProfilerStart("/tmp/dsm-perf");
  printf("benchmark starts\n");
  for (int i = 0; i < threadNR; ++i) {
    // printf("checkpoint 0 on main thread\n");
    th[i] = new std::thread(start_thread, dsm->getMyNodeID(), i);
    // printf("checkpoint 1 on main thread\n");
  }

  timespec s, e;

  printf("checkpoint 2 on main thread\n");
  while (ready.load() != threadNR)
    ;
  printf("checkpoint 3 on main thread\n");

  clock_gettime(CLOCK_REALTIME, &s);

  while (finish.load() != threadNR)
    ;
  printf("checkpoint 4 on main thread\n");

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

  // if (dsm->myNodeID == 0) {
  //   printf("recv %lu,  send %lu\n", recv_c, send_c);
  // }
  // if (output_file != nullptr && dsm->myNodeID == 0) {
  //   std::ofstream f(std::string(output_file), std::ios::app);
  //   if (f) {
  //     f << "[" << nodeNR << "," << threadNR << "," << readNR << "," << locality
  //       << "," << sharing << "]" << std::endl;
  //     f << all_tp << " , recv: " << recv_c << ", send: " << send_c << "\n"
  //       << std::endl;
  //   }
  //   f.close();
  // }

  /***********************************/
  /******** MY CODE STARTS ********/
  {
    std::string common_suffix = ".txt";
    if (!fs::exists(result_directory)) {
        if (!fs::create_directory(result_directory)) {
            std::cerr << "Error creating folder " << result_directory << std::endl;
            exit(1);
        }
    }
    FILE *file;
    fs::path dir(result_directory);
    fs::path filePath = dir / fs::path("end_to_end" + common_suffix);
    file = fopen(filePath.c_str(), "a");
    assert(file != nullptr);
    fprintf(file, "\n---new results---\n");
    fprintf(file, "dir recv count = %lu\t dir send count =  %lu\n", recv_c, send_c);
    if (dsm->myNodeID == 0) {
      fprintf(file, "sharing ratio = %d\t tp: %llu op/s; all-tp: %llu op/s\n", sharing, tp, all_tp);
    }

    agent_stats_inst.print_all_false_count(file, threadNR);

    fclose(file);
  /******** MY CODE ENDS ********/
  /***********************************/
  }

  sleep(1);

  // std::cout << "Hello, End\n";

  // Statistics::dispaly();

  // ProfilerStop();

  // while (true)
  //   ;

  return 0;
}
