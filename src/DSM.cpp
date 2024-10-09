#include "DSM.h"
#include "HugePageAlloc.h"

#include "CacheAgent.h"
#include "Directory.h"

#include "Controller.h"
#include "DSMKeeper.h"

#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <agent_stat.h>

DSM *DSM::getInstance(const DSMConfig &conf) {
  static DSM *dsm = nullptr;
  static WRLock lock;

  lock.wLock();
  if (!dsm) {
    dsm = new DSM(conf);
  } else {
  }
  lock.wUnlock();

  return dsm;
}

DSM::DSM(const DSMConfig &conf)
    : conf(conf), cache(conf.cacheConfig, this), appID(0), is_first_run(true) {

  memcpy(mac, getMac(), 6);
  // baseAddr = (uint64_t)hugePageAlloc(conf.dsmSize * define::GB);
  int dir_qp_num = NR_DIRECTORY * (NR_CACHE_AGENT + MAX_APP_THREAD) * conf.machineNR;
  baseAddr = (uint64_t)hugePageAlloc(conf.dsmSize * define::GB + dir_qp_num * MESSAGE_SIZE * 1024);

  Debug::notifyInfo("shared memory size: %dGB", conf.dsmSize);
  Debug::notifyInfo("cache size: %dGB",
                    (1ull << (DSM_CACHE_LINE_WIDTH + DSM_CACHE_INDEX_WIDTH)) *
                        CACHE_WAYS / define::GB);

  // warmup
  memset((char *)baseAddr, 0, conf.dsmSize * define::GB + dir_qp_num * MESSAGE_SIZE * 1024);
  // for (uint64_t i = baseAddr; i < baseAddr + conf.dsmSize * define::GB;
  //      i += 2 * define::MB) {
  //   *(char *)i = 0;
  // }

  initRDMAConnection();
  initSwitchTable();

  for (int i = 0; i < NR_CACHE_AGENT; ++i) {
    cacheAgent[i] = new CacheAgent(cacheCon[i], remoteInfo, &cache,
                                   conf.machineNR, i, myNodeID);
  }

  for (int i = 0; i < NR_DIRECTORY; ++i) {
    dirAgent[i] = new Directory(dirCon[i], remoteInfo, conf.machineNR, i,
                                myNodeID, controller);
  }

  keeper->barrier("DSM-init");

  srand(time(NULL));
  sleep(rand()%15);
}

DSM::~DSM() {}

void DSM::reset() {
  appID.store(0);
  is_first_run = false;
}

void DSM::registerThread() {
  cache.iId = appID.fetch_add(1);
  cache.iCon = thCon[cache.iId];

  cache.registerThread();

  if (is_first_run) {
    // Debug::notifyInfo("I am thread %d in node %d \n", cache.iId, myNodeID);
  }
}

void DSM::initRDMAConnection() {

  Debug::notifyInfo("Machine NR: %d", conf.machineNR);

  remoteInfo = new RemoteConnection[conf.machineNR];
  cache.remoteInfo = remoteInfo;

  int other_qp_num = MAX_APP_THREAD * conf.machineNR * (NR_CACHE_AGENT + NR_DIRECTORY)
                  + NR_CACHE_AGENT * conf.machineNR * (MAX_APP_THREAD + NR_DIRECTORY);

  for (int i = 0; i < MAX_APP_THREAD; ++i) {

    thCon[i] = new ThreadConnection(i, (void *)cache.cacheData, cache.cacheSize + other_qp_num * MESSAGE_SIZE * 1024,
                                    conf.machineNR, remoteInfo, mac);

    for(int j = 0; j < conf.machineNR; ++j){

      for(int k = 0; k < NR_CACHE_AGENT; ++k){
        int msg_bias = i * (conf.machineNR * (NR_CACHE_AGENT + NR_DIRECTORY))
                      + j * (NR_CACHE_AGENT + NR_DIRECTORY)
                      + k;
        agent_stats_inst.rq_app_f_agent[i][j][k] = (void *)((uint64_t)cache.cacheData + cache.cacheSize + msg_bias * MESSAGE_SIZE);
      }

      for(int k = 0; k < NR_DIRECTORY; ++k){
        int msg_bias = i * (conf.machineNR * (NR_CACHE_AGENT + NR_DIRECTORY))
                      + j * (NR_CACHE_AGENT + NR_DIRECTORY)
                      + NR_CACHE_AGENT + k;
        agent_stats_inst.rq_app_f_dir[i][j][k] = (void *)((uint64_t)cache.cacheData + cache.cacheSize + msg_bias * MESSAGE_SIZE);              
      }
    }
  }

  int dir_qp_num = NR_DIRECTORY * conf.machineNR * (NR_CACHE_AGENT + MAX_APP_THREAD);

  for (int i = 0; i < NR_DIRECTORY; ++i) {

    dirCon[i] = new DirectoryConnection(i, (void *)baseAddr,
                                        conf.dsmSize * define::GB + dir_qp_num * MESSAGE_SIZE * 1024,
                                        conf.machineNR, remoteInfo, mac);

    for(int j = 0; j < conf.machineNR; ++j){

      for(int k = 0; k < NR_CACHE_AGENT; ++k){
        int msg_bias = i * (conf.machineNR * (NR_CACHE_AGENT + MAX_APP_THREAD))
                      + j * (NR_CACHE_AGENT + MAX_APP_THREAD)
                      + k;
        agent_stats_inst.rq_dir_f_agent[i][j][k] = (void *)((uint64_t)baseAddr + conf.dsmSize * define::GB + msg_bias * MESSAGE_SIZE);
      }

      for(int k = 0; k < MAX_APP_THREAD; ++k){
        int msg_bias = i * (conf.machineNR * (NR_CACHE_AGENT + MAX_APP_THREAD))
                      + j * (NR_CACHE_AGENT + MAX_APP_THREAD)
                      + NR_CACHE_AGENT + k;
        agent_stats_inst.rq_dir_f_app[i][j][k] = (void *)((uint64_t)baseAddr + conf.dsmSize * define::GB + msg_bias * MESSAGE_SIZE);
      }
    }                              
  }

  for (int i = 0; i < NR_CACHE_AGENT; ++i) {
    cacheCon[i] =
        new CacheAgentConnection(i, (void *)cache.cacheData, cache.cacheSize + other_qp_num * MESSAGE_SIZE * 1024,
                                 conf.machineNR, remoteInfo, mac);

    for(int j = 0; j < conf.machineNR; ++j){

      for(int k = 0; k < MAX_APP_THREAD; ++k){
        int msg_bias = MAX_APP_THREAD * conf.machineNR * (NR_CACHE_AGENT + NR_DIRECTORY)
                      + i * (conf.machineNR * (MAX_APP_THREAD + NR_DIRECTORY))
                      + j * (MAX_APP_THREAD + NR_DIRECTORY)
                      + k;
        agent_stats_inst.rq_agent_f_app[i][j][k] = (void *)((uint64_t)cache.cacheData + cache.cacheSize + msg_bias * MESSAGE_SIZE);
      }

      for(int k = 0; k < NR_DIRECTORY; ++k){
        int msg_bias = MAX_APP_THREAD * conf.machineNR * (NR_CACHE_AGENT + NR_DIRECTORY)
                      + i * (conf.machineNR * (MAX_APP_THREAD + NR_DIRECTORY))
                      + j * (MAX_APP_THREAD + NR_DIRECTORY)
                      + MAX_APP_THREAD + k;
        agent_stats_inst.rq_agent_f_dir[i][j][k] = (void *)((uint64_t)cache.cacheData + cache.cacheSize + msg_bias * MESSAGE_SIZE);              
      }
    }
  }


  keeper = new DSMKeeper(thCon, dirCon, cacheCon, remoteInfo, conf.machineNR);

  myNodeID = keeper->getMyNodeID();
  mybitmap = 1 << myNodeID;
}

void DSM::initSwitchTable() {

  Debug::notifyError("initSwitchTable!");

  controller = new Controller(myNodeID, keeper->getMyPort());
  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    controller->appQP(thCon[i]->message->getQPN(), i);
  }

  // assert(NR_DIRECTORY == 1 && NR_CACHE_AGENT == 1);

  for (int i = 0; i < NR_DIRECTORY; ++i) {
    controller->dirQP(dirCon[i]->message->getQPN(), i);
  }
  for (int i = 0; i < NR_CACHE_AGENT; ++i) {
    controller->agentQP(cacheCon[i]->message->getQPN(), i);
  }
}

void DSM::read(const GlobalAddress &addr, uint32_t size, uint8_t *to) {

  assert(addr.addr <= conf.dsmSize * define::GB);

  uint32_t start = addr.addr % DSM_CACHE_LINE_SIZE;
  uint32_t len = std::min(size, DSM_CACHE_LINE_SIZE - start);

  cache.readLine(addr, start, len, to);
  if (len == size)
    return;

  uint8_t *end = to + size;
  GlobalAddress iter = addr;
  to += len;
  iter.addr += len;

  while (end - to >= DSM_CACHE_LINE_SIZE) {
    cache.readLine(iter, 0, DSM_CACHE_LINE_SIZE, to);

    to += DSM_CACHE_LINE_SIZE;
    iter.addr += DSM_CACHE_LINE_SIZE;
  }

  if (end != to) {
    cache.readLine(iter, 0, end - to, to);
  }
}

void DSM::write(const GlobalAddress &addr, uint32_t size, const uint8_t *from) {

  // if (addr.addr > conf.dsmSize * define::GB) {
  //   Debug::notifyError("XXXXXXX %llu", addr.addr);
  //   exit(-1);
  // }
  assert(addr.addr <= conf.dsmSize * define::GB);

  uint32_t start = addr.addr % DSM_CACHE_LINE_SIZE;
  uint32_t len = std::min(size, DSM_CACHE_LINE_SIZE - start);

  cache.writeLine(addr, start, len, from);
  if (len == size)
    return;

  uint8_t *end = (uint8_t *)from + size;
  GlobalAddress iter = addr;
  from += len;
  iter.addr += len;

  while (end - from >= DSM_CACHE_LINE_SIZE) {
    cache.writeLine(iter, 0, DSM_CACHE_LINE_SIZE, from);

    from += DSM_CACHE_LINE_SIZE;
    iter.addr += DSM_CACHE_LINE_SIZE;
  }

  if (end != from) {
    cache.writeLine(iter, 0, end - from, from);
  }
}

