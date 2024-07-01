#include <iostream>
#include <cstdio>
#include <thread>
// #include <gperftools/profiler.h>

#include "DSM.h"

#define MAX_THREAD 24

#define OP_NUMS 100000
#define OBJ_SIZE 8

#define MB (1024ull * 1024)

#define BLOCK_SIZE (30ull * MB)

int nodeNR = 0;
int threadNR = 0;

std::thread th[MAX_THREAD];
DSM *dsm;

void benchmark(int nodeID, int threadID) {

    (void)nodeID;
    (void)threadID;

    dsm->registerThread();

    GlobalAddress addr;
    addr.addr = 0;

    for (int op = 0; op < OP_NUMS; ++op) {
        for (int i = 0; i < nodeNR; ++i) {
            addr.nodeID = i;
            uint64_t v;
            dsm->read(addr, sizeof(uint64_t), (uint8_t *)&v);
            v++;
            dsm->write(addr, sizeof(uint64_t), (uint8_t *)&v);
        }
    }
}

void parserArgs(int argc, char **argv) {

    if (argc != 3) {
        fprintf(stderr, "Usage: ./benchmark nodeNR threadNR\n");
        exit(-1);
    }

    nodeNR = std::atoi(argv[1]);
    threadNR = std::atoi(argv[2]);

    fprintf(stdout, "Benchmark Config: nodeNR %d, threadNR %d\n", nodeNR,
            threadNR);
}

int main(int argc, char **argv) {

    parserArgs(argc, argv);

    DSMConfig conf(CacheConfig(), nodeNR, 1);  // 4G per node;
    dsm = DSM::getInstance(conf);

    // benchmark
    // ProfilerStart("/tmp/dsm-perf");
    for (int i = 0; i < threadNR; ++i) {
        th[i] = std::thread(benchmark, dsm->getMyNodeID(), i);
    }

    timespec s, e;

    clock_gettime(CLOCK_REALTIME, &s);
    for (int i = 0; i < threadNR; ++i) {
        th[i].join();
    }
    clock_gettime(CLOCK_REALTIME, &e);
    // ProfilerStop();
    double microseconds = (e.tv_sec - s.tv_sec) * 1000000 +
                          (double)(e.tv_nsec - s.tv_nsec) / 1000;

    uint64_t ops = threadNR * OP_NUMS;
    fprintf(stderr, "%f op/s\n", ops * 1000000 / microseconds);

    std::cout << "Hello, End\n";

    dsm->showCacheStat();

    while (true)
        ;

    return 0;
}
