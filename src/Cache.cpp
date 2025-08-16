#include "Cache.h"

#include "RecvImmBatch.h"

#include "Connection.h"
#include "DSM.h"
#include "HugePageAlloc.h"

#include "agent_stat.h"

thread_local uint64_t Cache::evict_time = 0;
thread_local int Cache::iId(-1);
thread_local ThreadConnection *Cache::iCon(nullptr);
thread_local uint32_t Cache::seed;
thread_local bool Cache::pending_unlock_ack = false;
thread_local uint16_t Cache::unlock_flow_control = 0;

//
thread_local uint32_t Cache::next_allocator_node = 0;
thread_local LocalAllocator *Cache::alloc = nullptr;

Cache::Cache(const CacheConfig &conf, DSM *dsm) : dsm(dsm) {
  (void)conf;

  uint64_t pageNR = DSM_CACHE_INDEX_SIZE * CACHE_WAYS;
  cacheHeader =
      (LineSet *)hugePageAlloc(sizeof(LineSet) * DSM_CACHE_INDEX_SIZE);

  cacheSize = pageNR * DSM_CACHE_LINE_SIZE;
  cacheData = hugePageAlloc(cacheSize);

  char *data = (char *)cacheData;
  for (size_t i = 0; i < DSM_CACHE_INDEX_SIZE; ++i) {
    auto &set = cacheHeader[i];
    for (size_t j = 0; j < CACHE_WAYS; ++j) {

      auto &line = set.header[j];
      line.data = data;
      line.setInvalid();
      line.writeEvictLock.init();

#ifdef LRU_EVICT
      line.timestamp.store(0);
#endif

      *data = '\0'; // warm up

      data += DSM_CACHE_LINE_SIZE;
    }
  }
  Debug::notifyInfo("Cache init succ!");
}

Cache::~Cache() {
  // unmap
}

void Cache::registerThread() {

  seed = time(NULL);

  if (!dsm->is_first_run)
    return;

  // for recv write_imm
  for (int i = 0; i < NR_DIRECTORY; ++i) {
    for (size_t k = 0; k < dsm->conf.machineNR; ++k) {
      new RecvImmBatch(iCon->data[i][k], APP_POST_IMM_RECV);
      // for (int n = 0; n < APP_POST_IMM_RECV; ++n) {
      //   rdmaReceive(iCon->data[i][k], 0, 0, 0);
      // }
    }
  }

  for (int i = 0; i < NR_CACHE_AGENT; ++i) {
    for (size_t k = 0; k < dsm->conf.machineNR; ++k) {
      new RecvImmBatch(iCon->fromAgent[i][k], APP_POST_IMM_RECV);
      // for (int n = 0; n < APP_POST_IMM_RECV; ++n) {
      //   rdmaReceive(iCon->fromAgent[i][k], 0, 0, 0);
      // }
    }
  }
  iCon->message->initRecv();
  iCon->message->initSend();

  // alloc
  next_allocator_node = rand_r(&seed) % dsm->conf.machineNR;
  alloc = new LocalAllocator();
}

void Cache::readLine(const GlobalAddress &addr, uint16_t start, uint16_t size,
                     void *to) {
  // sendUnlock(0x3, RawState::S_SHARED, 0, 0x123);
  // return;

#ifdef VERBOSE
  static thread_local int seq = 0;
  seq++;
#endif
 bool is_local_hit = true;

#ifdef DEADLOCK_DETECTION
  int counter = 0;
#endif

retry:

#ifdef VERBOSE
  printf("seq %d, (%d, %d) try to readLine %x, %d\n", seq, dsm->myNodeID, iId,
         addr.getDirKey(), addr.nodeID);
#endif

#ifdef DEADLOCK_DETECTION
  if (counter++ > 100000) {
    // printf("(%d, %d) try to readLine %lx, %d fail!!!\n", dsm->myNodeID, iId, addr.getDirKey(), addr.nodeID);
    assert(false);

    uint64_t delay = counter;
    for(int i = 0; i < delay / 1000; i++);
  }
#endif

  LineInfo *info;

  if (!findLine(addr, info)) { // miss in cache;
    is_local_hit = false;
    assert(info->getTag() == addr.getTag() &&
           info->getStatus() == CacheStatus::BEING_FILL);
    if (!readMiss(addr, info)) {
      info->setInvalid();
      goto retry;
    }
  }

  info->writeEvictLock.rLock();
  {
    AtomicTag t = info->getTagAndStatus();
    if (t.tag != addr.getTag() || !t.isCanRead()) {
      is_local_hit = false;
      info->writeEvictLock.rUnlock();
      goto retry;
    }

    info->setTimeStamp();
    memcpy(to, (char *)info->data + start, size);
  }

  // // debug
  // if (agent_stats_inst.is_valid_gaddr_without_start(addr.addr)) {
  //   findLine(addr, info);
  //   printf("readline: info->getStatus() = %d\n", (int)info->getStatus());
  // }
  // agent_stats_inst.RecordRead(size);
  // agent_stats_inst.cachehit.fetch_add(is_local_hit ? 1 : 0);

  info->writeEvictLock.rUnlock();
}

void Cache::writeLine(const GlobalAddress &addr, uint16_t start, uint16_t size,
                      const void *from) {

#ifdef VERBOSE
  static thread_local int seq = 0;
  seq++;
#endif

#ifdef DEADLOCK_DETECTION
  int write_miss_counter = 0;
  int write_shared_counter = 0;
#endif
  bool is_local_hit = true;
retry:
#ifdef VERBOSE
  printf("seq %d, (%d, %d) try to writeLine %x, %d\n", seq, dsm->myNodeID, iId,
         addr.getDirKey(), addr.nodeID);
#endif

  LineInfo *info;
  if (!findLine(addr, info)) {
    is_local_hit = false;
    assert(info->getTag() == addr.getTag() &&
           info->getStatus() == CacheStatus::BEING_FILL);
    if (!writeMiss(addr, info)) {

      info->setInvalid();

#ifdef DEADLOCK_DETECTION
      if (write_miss_counter++ > 10000) {
        // printf("(%d, %d) try to writeLine %lx, %d fail!!!!\n", dsm->myNodeID, iId, addr.getDirKey(), addr.nodeID);
        assert(false);

        uint64_t delay = write_miss_counter;
        for(int i = 0; i < delay / 100; i++);
      }
#endif
      goto retry;
    }
  }

  if (info->getStatus() == CacheStatus::SHARED) {
    is_local_hit = false;
    if (!writeShared(addr, info)) {
      
#ifdef DEADLOCK_DETECTION
      if (write_shared_counter++ > 10000) {
        // printf("(%d, %d)  try to writeshared %lx, %d fail!!!!\n", dsm->myNodeID,iId, addr.getDirKey(), addr.nodeID);
        assert(false);

        uint64_t delay = write_shared_counter;
        for(int i = 0; i < delay / 100; i++);
      }
#endif
      goto retry;
    }
  }

  info->writeEvictLock.rLock();
  {
    AtomicTag t = info->getTagAndStatus();
    if (t.tag != addr.getTag() || t.status != CacheStatus::MODIFIED) {
      is_local_hit = false;
      info->writeEvictLock.rUnlock();
      goto retry;
    }

    info->setTimeStamp();
    memcpy((char *)info->data + start, from, size);
  }
  // agent_stats_inst.cachehit.fetch_add(is_local_hit ? 1 : 0);
  // agent_stats_inst.RecordWrite(size);
  // // debug
  // if (agent_stats_inst.is_valid_gaddr(addr.addr)) {
  //   findLine(addr, info);
  //   printf("writeline: info->getStatus() = %d\n", (int)info->getStatus());
  // }

  info->writeEvictLock.rUnlock();
}

int Cache::get_evict_index(LineSet &set, LineInfo *&line) {

#ifdef LRU_EVICT

retry:
  uint64_t min_time = UINT64_MAX;
  int min_index = -1;
  for (int i = 0; i < CACHE_WAYS; ++i) {
    auto cur = &set.header[i];
    auto s = cur->getStatus();
    if (s == CacheStatus::INVALID) {
      line = cur;
      return i;
    }

    if (s == CacheStatus::MODIFIED || s == CacheStatus::SHARED) {
      uint64_t cur_time = cur->getTimeStamp();
      if (cur_time < min_time) {
        line = cur;
        min_index = i;
        min_time = cur_time;
      }
    }
  }

  if (min_index == -1) {
    goto retry;
  }
  return min_index;
#else
  int index = rand_r(&seed) % CACHE_WAYS;
  line = &set.header[index];
  return index;
#endif
}

bool Cache::findLine(const GlobalAddress &addr, LineInfo *&line) {

  LineSet &set = cacheHeader[addr.getIndex()];
  Tag tag = addr.getTag();

  assert(tag != InvalidTag);

  int retry_count = 0;
retry:
  assert(retry_count++ <= 10000);

  line = nullptr;
  int invalidIndex = -1;
  for (int i = 0; i < CACHE_WAYS; ++i) {

    auto &l = set.header[i];

    int pending_count = 0;

  pending:
    assert(pending_count++ <= 10000);

    AtomicTag t = l.getTagAndStatus();

    if (t.status == CacheStatus::INVALID) {
      if (invalidIndex == -1)
        invalidIndex = i;
      assert(t.tag == InvalidTag);
      continue;
    }
    if (t.tag != tag) {
      continue;
    }

    switch (t.status) {

    case CacheStatus::MODIFIED:
    case CacheStatus::SHARED:
    case CacheStatus::BEING_INVALID:
    case CacheStatus::BEING_SHARED: {
      line = &l;
      return true;
    }

    case CacheStatus::BEING_FILL: {
      while (l.getStatus() == CacheStatus::BEING_FILL)
        ;
      goto pending;

      break;
    }

    case CacheStatus::BEING_EVICT: {
      while (l.getStatus() == CacheStatus::BEING_EVICT)
        ;
      goto pending;

      break;
    }

    default: { assert(false); }
    }
  }

  // miss

  int lock_invalid_counter = 0;
lock_invalid:
  assert(lock_invalid_counter++ < 10000);

  if (invalidIndex != -1) {
    line = &set.header[invalidIndex];
    AtomicTag t = AtomicTag{tag, CacheStatus::BEING_FILL};
    if (!line->casAllWithStatus(CacheStatus::INVALID, t)) {
      goto retry;
    }

    // re-scan
    for (int k = 0; k < CACHE_WAYS; ++k) {
      if (k != invalidIndex && set.header[k].getTag() == tag) {
        line->setInvalid();
        goto retry;
      }
    }

    return false;
  }

  // evict

  int re_evict_counter = 0;
re_evict:
  evict_time++;
  assert(re_evict_counter++ < 10000);

  int evictIndex = get_evict_index(set, line);

  AtomicTag t = line->getTagAndStatus();
  switch (t.status) {
  case CacheStatus::SHARED:
  case CacheStatus::MODIFIED: {
    break;
  }
  case CacheStatus::INVALID: {
    invalidIndex = evictIndex;
    goto lock_invalid;
    break;
  }
  case CacheStatus::BEING_EVICT:
  case CacheStatus::BEING_FILL:
  case CacheStatus::BEING_INVALID:
  case CacheStatus::BEING_SHARED: {
    if (t.tag == tag)
      goto retry;
    else
      goto re_evict;

    break;
  }
  default:
    assert(false);
  }

  line->writeEvictLock.wLock();
  bool res = line->cas(t.tag, t.status, CacheStatus::BEING_EVICT);
  line->writeEvictLock.wUnlock();

  if (!res)
    goto retry;

  GlobalAddress evictAddr =
      GlobalAddress::genGlobalAddrFormIndexTag(addr.getIndex(), line->getTag());

#ifdef VERBOSE
  printf("(%d, %d) try to evict %x, %d\n", dsm->myNodeID, iId,
         evictAddr.getDirKey(), evictAddr.nodeID);
#endif
  if (!evictLine(line, t.status, evictAddr)) {
    line->cas(t.tag, CacheStatus::BEING_EVICT, t.status);
#ifdef VERBOSE
    printf("(%d, %d) try to evict %x, %d failed\n", dsm->myNodeID, iId,
           evictAddr.getDirKey(), evictAddr.nodeID);
#endif
    goto retry;
  } else {
    invalidIndex = evictIndex;
#ifdef VERBOSE
    printf("(%d, %d) try to evict %x, %d succ\n", dsm->myNodeID, iId,
           evictAddr.getDirKey(), evictAddr.nodeID);
#endif
    goto lock_invalid;
  }

  return false;
};

LineInfo *Cache::findLineForAgent(uint16_t nodeID, DirKey dirKey) {
  GlobalAddress addr;
  addr.nodeID = nodeID;
  addr.addr = dirKey << DSM_CACHE_LINE_WIDTH;

  LineSet &set = cacheHeader[addr.getIndex()];
  Tag tag = addr.getTag();
#ifdef VERBOSE
  LineInfo *ret = nullptr;
#endif

  for (int i = 0; i < CACHE_WAYS; ++i) {
    auto &line = set.header[i];
    AtomicTag t = line.getTagAndStatus();

    if (t.tag != tag) {
      continue;
    }

    if (t.status == CacheStatus::BEING_FILL) {
      continue;
    }

#ifdef VERBOSE
    if (ret != nullptr) {
      auto v = ret->getTagAndStatus();
      printf("tag %x, dirKey (%x, %d), agent %d, %s\n", v.tag, dirKey, nodeID,
             dsm->myNodeID, strCacheStatus(v.status));

      v = line.getTagAndStatus();
      printf("tag %x, dirKey (%x, %d), agent %d, %s\n", v.tag, dirKey, nodeID,
             dsm->myNodeID, strCacheStatus(v.status));
    }

    assert(ret == nullptr);
    ret = &line;
#else
    return &line;
#endif
  }

#ifdef VERBOSE
  if (ret == nullptr) {
    printf("agent %d for (%x, %d) failed\n", dsm->myNodeID, dirKey, nodeID);
  }
  assert(ret != nullptr);
  return ret;
#else
  assert(false);
#endif
}

void Cache::sendMessage2Dir(LineInfo *info, RawMessageType type,
                            uint8_t dirNodeID, uint32_t dirKey,
                            bool enter_pipeline) {

#ifdef UNLOCK_SYNC
  if (pending_unlock_ack) {
    struct ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
    RawMessage *ack = (RawMessage *)iCon->message->getMessage();
    (void)ack;

    pending_unlock_ack = false;
  }
#endif

  RawMessage *m = (RawMessage *)iCon->message->getSendPool();

  m->qpn = iCon->message->getQPN();
  m->mtype = type;
  m->dirKey = dirKey;

  m->dirNodeID = dirNodeID;
  m->nodeID = dsm->myNodeID;

  m->appID = iId;

  m->mybitmap = dsm->mybitmap;
  m->state = RawState::S_UNDEFINED;

  m->is_app_req = enter_pipeline;

  m->destAddr = (uint64_t)info->data;

  printRawMessage(m, "app send");

  if (m->is_app_req) {
    m->set_tag_and_index();
  } else { // sync and malloc
    m->invalidate_tag();
  }

  iCon->sendMessage(m);
}

void Cache::sendUnlock(uint16_t bitmap, uint8_t state, uint8_t dirNodeID,
                       uint32_t dirKey, RawMessageType type) {
  // Debug::notifyError("%s %d", __FILE__, __LINE__);

  // sleep(1);

  RawMessage *m = (RawMessage *)iCon->message->getSendPool();

  m->qpn = iCon->message->getQPN();
  m->mtype = type;

  m->dirKey = dirKey;
  m->dirNodeID = dirNodeID;

  m->nodeID = dsm->myNodeID;
  m->appID = iId;

  m->mybitmap = dsm->mybitmap;

  m->bitmap = bitmap;
  m->state = state;

  m->is_app_req = 0;

  m->set_tag_and_index();

  iCon->sendMessage(m);

  printRawMessage(m, "app unlock");

#ifdef UNLOCK_SYNC
  if (type != RawMessageType::R_UNLOCK_EVICT) {
    pending_unlock_ack = true;
  }
#endif

  // assert(wc.opcode == IBV_WC_RECV);

  // printRawMessage(ack);
  // assert(ack->mtype == RawMessageType::ACK_UNLOCK);
  // Debug::notifyError("%s %d", __FILE__, __LINE__);
}

bool Cache::readMiss(const GlobalAddress &addr, LineInfo *info) {
  // if(iId == 0) agent_stats_inst.set_memaccess_type(MEMACCESS_TYPE::WITH_CC);

  uint32_t dirKey = addr.getDirKey();
  sendMessage2Dir(info, RawMessageType::R_READ_MISS, addr.nodeID, dirKey);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_READ);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  struct ibv_wc wc;
  pollWithCQ(iCon->cq, 1, &wc);
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::WAIT_ASYNC_FINISH);

  // COMMENT: why need to receive other packets? -- home node inform request node that cache node will send data to request node (two packets need receiving)
  // COMMENT: why need to unlock? --inform the home node that the current CC txn is end; the lock is txn atomic lock, instead of app lock
  // agent_stats_inst.start_record_app_thread(addr.addr);

  // uint8_t dirID = dirKey % NR_DIRECTORY;
  if (wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
    RawImm *imm = (RawImm *)&wc.imm_data;
    printRawImm(imm);

    if (imm->state == RawState::S_DIRTY) { // agent send data to me

      // uint8_t agentNodeID = __builtin_ctz(imm->bitmap);

#ifdef READ_MISS_DIRTY_TO_DIRTY
      info->setStatus(CacheStatus::MODIFIED);
      sendUnlock(dsm->mybitmap, RawState::S_DIRTY, addr.nodeID, dirKey);
      // agent_stats_inst.control_packet_send_count[iId] += 1;
#else
      pollWithCQ(iCon->cq, 1, &wc);
      assert(wc.opcode == IBV_WC_RECV);
      RawMessage *ack = (RawMessage *)iCon->message->getMessage();
      printRawMessage(ack);
      info->setStatus(CacheStatus::SHARED);

#ifdef R_W_CC
      sendUnlock(dsm->mybitmap, RawState::S_SHARED, addr.nodeID, dirKey,
                 RawMessageType::R_READ_MISS_UNLOCK);
      // agent_stats_inst.control_packet_send_count[iId] += 1;
#else
      sendUnlock(dsm->mybitmap | imm->bitmap, RawState::S_SHARED, addr.nodeID,
                 dirKey);
      // agent_stats_inst.control_packet_send_count[iId] += 1;
#endif

#endif

    } else {
      switch (imm->state) {
      case RawState::S_UNDEFINED:
        assert(false);
      case RawState::S_UNSHARED:
        // stat.read_miss_unshared++;

        info->setStatus(CacheStatus::SHARED);

#ifdef R_W_CC
        sendUnlock(dsm->mybitmap, RawState::S_SHARED, addr.nodeID, dirKey,
                   RawMessageType::R_READ_MISS_UNLOCK);
        // agent_stats_inst.control_packet_send_count[iId] += 1;
#else
        sendUnlock(dsm->mybitmap, RawState::S_SHARED, addr.nodeID, dirKey);
        // agent_stats_inst.control_packet_send_count[iId] += 1;
#endif
        break;
      case RawState::S_SHARED:
        // stat.read_miss_shared++;
        info->setStatus(CacheStatus::SHARED);

#ifdef R_W_CC
        sendUnlock(dsm->mybitmap, RawState::S_SHARED, addr.nodeID, dirKey,
                   RawMessageType::R_READ_MISS_UNLOCK);
        // agent_stats_inst.control_packet_send_count[iId] += 1;
#else
        sendUnlock(dsm->mybitmap | imm->bitmap, RawState::S_SHARED, addr.nodeID,
                   dirKey);
        // agent_stats_inst.control_packet_send_count[iId] += 1;
#endif
        break;
      default:
        assert(false);
      }
      // rdmaReceive(iCon->data[dirID][addr.nodeID], 0, 0, 0);
    }

    return true;

  } else if (wc.opcode == IBV_WC_RECV) {
    RawMessage *ack = (RawMessage *)iCon->message->getMessage();
    printRawMessage(ack);

    switch (ack->mtype) {
    case RawMessageType::M_LOCK_FAIL:
#ifdef VERBOSE
      printf("lock_fail\n");
#endif
      // if(agent_stats_inst.is_start()) agent_stats_inst.readMiss[iId]++;
      return false;
    case RawMessageType::M_CHECK_FAIL:
      // printf("check_fail");
      // if(agent_stats_inst.is_start()) agent_stats_inst.readMiss_1[iId]++;
      return false;

    case RawMessageType::DIR_2_APP_MISS_SWITCH:
      // printf("switch_fail");
      // (agent_stats_inst.is_start()) agent_stats_inst.readMiss_2[iId]++;
      return false;
    case RawMessageType::N_DIR_ACK_APP_READ_MISS_DIRTY: {
#ifdef READ_MISS_DIRTY_TO_DIRTY
      Debug::notifyError("error");
#endif

      pollWithCQ(iCon->cq, 1, &wc);
      assert(wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM);

      assert(((RawImm *)&wc.imm_data)->state == RawState::S_DIRTY);
      // uint8_t agentNodeID = __builtin_ctz(imm->bitmap);

      info->setStatus(CacheStatus::SHARED);

#ifdef R_W_CC
      sendUnlock(dsm->mybitmap, RawState::S_SHARED, addr.nodeID, dirKey,
                 RawMessageType::R_READ_MISS_UNLOCK);
      // agent_stats_inst.control_packet_send_count[iId] += 1;
#else
      sendUnlock(dsm->mybitmap | ((RawImm *)&wc.imm_data)->bitmap, RawState::S_SHARED, addr.nodeID,
                 dirKey);
      // agent_stats_inst.control_packet_send_count[iId] += 1;
#endif

      return true;
    }
    }
  }
  assert(false);
}

bool Cache::writeMiss(const GlobalAddress &addr, LineInfo *info) {
  // if(iId == 0) agent_stats_inst.set_memaccess_type(MEMACCESS_TYPE::WITH_CC);

  uint32_t dirKey = addr.getDirKey();
  sendMessage2Dir(info, RawMessageType::R_WRITE_MISS, addr.nodeID, dirKey);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_WRITE);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  struct ibv_wc wc;
  pollWithCQ(iCon->cq, 1, &wc);
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::WAIT_ASYNC_FINISH);

  // TODO: why need to receive other packets? why unlock? 
  // agent_stats_inst.start_record_app_thread(addr.addr);

  // uint8_t dirID = dirKey % NR_DIRECTORY;
  if (wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
    RawImm *imm = (RawImm *)&wc.imm_data;
    printRawImm(imm);

    switch (imm->state) {
    case RawState::S_UNDEFINED:
    case RawState::S_UNSHARED:
      // stat.write_miss_unshared++;
      goto succ;
    case RawState::S_DIRTY: {
      // assert(false);
      // stat.write_miss_dirty++;
      // uint8_t agentNodeID = __builtin_ctz(imm->bitmap);

      info->setStatus(CacheStatus::MODIFIED);
      sendUnlock(dsm->mybitmap, RawState::S_DIRTY, addr.nodeID, dirKey);
      // agent_stats_inst.control_packet_send_count[iId] += 1;
      // rdmaReceive(iCon->fromAgent[dirID][agentNodeID], 0, 0, 0);
      return true;
    }
    case RawState::S_SHARED: {
      // stat.write_miss_shared++;

#ifndef BASELINE
      pollWithCQ(iCon->cq, 1, &wc);
      RawMessage *ack = (RawMessage *)iCon->message->getMessage();
      printRawMessage(ack, "app recv");

      uint16_t bitmap = ack->bitmap ^ ack->mybitmap;

      while (bitmap != 0) {
        pollWithCQ(iCon->cq, 1, &wc);
        ack = (RawMessage *)iCon->message->getMessage();
        printRawMessage(ack);
        bitmap ^= ack->mybitmap;
      }
#else
      // TODO wait a ack from home agent
      pollWithCQ(iCon->cq, 1, &wc);

      auto ack = (RawMessage *)iCon->message->getMessage();
      assert(ack->mtype == RawMessageType::DIR_2_APP_WRITE_MISS_BASELINE);
#endif

      goto succ;
    }
    default:
      assert(false);
    }

  } else if (wc.opcode == IBV_WC_RECV) {
    RawMessage *ack = (RawMessage *)iCon->message->getMessage();
    printRawMessage(ack);

    switch (ack->mtype) {
    case RawMessageType::M_LOCK_FAIL:

#ifdef VERBOSE
      printf("lock_fail\n");
#endif
      // stat.write_shared_lock_fail++;
      // if(agent_stats_inst.is_start()) agent_stats_inst.writeMiss[iId]++;
      return false;
    case RawMessageType::M_CHECK_FAIL:
    // printf("check_fail");
    case RawMessageType::DIR_2_APP_MISS_SWITCH:
      //  printf("switch_fail");
      // stat.write_miss_check_fail++;
      // if(agent_stats_inst.is_start()) agent_stats_inst.writeMiss_1[iId]++;
      return false;
    case RawMessageType::AGENT_ACK_WRITE_MISS: {
      // stat.write_miss_shared++;

      uint16_t bitmap = ack->bitmap ^ ack->mybitmap;

      bool hasData = false;
      while (bitmap != 0) {
        pollWithCQ(iCon->cq, 1, &wc);

        if (wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
          assert(!hasData);
          hasData = true;
        } else {
          ack = (RawMessage *)iCon->message->getMessage();
          printRawMessage(ack);
          bitmap ^= ack->mybitmap;
        }
      }
      if (!hasData) {
        pollWithCQ(iCon->cq, 1, &wc);
        assert(wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM);
      }
      goto succ;
    }
#ifdef BASELINE
    case RawMessageType::DIR_2_APP_WRITE_MISS_BASELINE: {

      // TODO wait a data for cache agent
      pollWithCQ(iCon->cq, 1, &wc);
      assert(wc.opcode == IBV_WC_RECV_RDMA_WITH_IMM);

      goto succ;
    }

#endif

    default:
      assert(false);
    }
  }

  assert(false);

succ:

  info->setStatus(CacheStatus::MODIFIED);
  sendUnlock(dsm->mybitmap, RawState::S_DIRTY, addr.nodeID, dirKey);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  // rdmaReceive(iCon->data[dirID][addr.nodeID], 0, 0, 0);
  return true;
}

bool Cache::writeShared(const GlobalAddress &addr, LineInfo *info) {
  // if(iId == 0) agent_stats_inst.set_memaccess_type(MEMACCESS_TYPE::WITH_CC);

  uint32_t dirKey = addr.getDirKey();
  sendMessage2Dir(info, RawMessageType::R_WRITE_SHARED, addr.nodeID, dirKey);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_WRITE);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  struct ibv_wc wc;
  pollWithCQ(iCon->cq, 1, &wc);
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::WAIT_ASYNC_FINISH);

  // agent_stats_inst.start_record_app_thread(addr.addr);

  assert(wc.opcode == IBV_WC_RECV);
  RawMessage *ack = (RawMessage *)iCon->message->getMessage();
  printRawMessage(ack);

  switch (ack->mtype) {
  case RawMessageType::M_LOCK_FAIL: {
#ifdef VERBOSE
    printf("lock fail\n");
#endif
    // stat.write_shared_lock_fail++;
    // if(agent_stats_inst.is_start()) agent_stats_inst.writeShared[iId]++;
    return false;
    break;
  }
  case RawMessageType::M_CHECK_FAIL: {
#ifdef VERBOSE
    printf("check fail %s 0x%x\n", strState(ack->state), ack->bitmap);
#endif
    // stat.write_shared_check_fail++;
    // if(agent_stats_inst.is_start()) agent_stats_inst.writeShared_1[iId]++;
    return false;
    break;
  }
  case RawMessageType::DIR_2_APP_MISS_SWITCH: {
    // if(agent_stats_inst.is_start()) agent_stats_inst.writeShared_2[iId]++;
    return false;
    break;
  }
  case RawMessageType::AGENT_ACK_WRITE_SHARED:
  case RawMessageType::DIR_2_APP_WRITE_SHARED:
  case RawMessageType::R_WRITE_SHARED: {
    // stat.write_shared++;=
    uint16_t bitmap = ack->bitmap ^ ack->mybitmap;

    while (bitmap != 0) {
      pollWithCQ(iCon->cq, 1, &wc);
      ack = (RawMessage *)iCon->message->getMessage();
      printRawMessage(ack);
      bitmap ^= ack->mybitmap;
    }

    info->setStatus(CacheStatus::MODIFIED);
    sendUnlock(dsm->mybitmap, RawState::S_DIRTY, addr.nodeID, dirKey);
    // agent_stats_inst.control_packet_send_count[iId] += 1;
    return true;
    break;
  }
#ifdef BASELINE
  case RawMessageType::DIR_2_APP_WRITE_SHARED_BASELINE: {
    info->setStatus(CacheStatus::MODIFIED);
    sendUnlock(dsm->mybitmap, RawState::S_DIRTY, addr.nodeID, dirKey);
    // agent_stats_inst.control_packet_send_count[iId] += 1;
    return true;
    break;
  }
#endif
  default:
    assert(false);
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
bool Cache::evictLine(LineInfo *line, CacheStatus c,
                      const GlobalAddress &addr) {

  if (c == CacheStatus::SHARED) {
    return evictLineShared(addr, line);
  } else if (c == CacheStatus::MODIFIED) {
    return evictLineDirty(addr, line);
  } else {
    assert(false);
  }

  // if(agent_stats_inst.is_start()) agent_stats_inst.evictLine[iId]++;
  return false;
}

bool Cache::evictLineShared(const GlobalAddress &addr, LineInfo *info) {

  uint32_t dirKey = addr.getDirKey();
  sendMessage2Dir(info, RawMessageType::R_EVICT_SHARED, addr.nodeID, dirKey);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  struct ibv_wc wc;

  pollWithCQ(iCon->cq, 1, &wc);

  assert(wc.opcode == IBV_WC_RECV);
  RawMessage *m = (RawMessage *)iCon->message->getMessage();

  switch (m->mtype) {
  case RawMessageType::M_LOCK_FAIL:
  case RawMessageType::M_CHECK_FAIL:
  case RawMessageType::DIR_2_APP_MISS_SWITCH: {

    // if(agent_stats_inst.is_start()) agent_stats_inst.evictLine_1[iId]++;
    return false;
    break;
  }

  case RawMessageType::R_EVICT_SHARED:
  case RawMessageType::DIR_2_APP_EVICT_SHARED: {
    uint16_t new_bitmap = dsm->mybitmap ^ m->bitmap;
    RawState new_state =
        new_bitmap == 0 ? RawState::S_UNSHARED : RawState::S_SHARED;

    info->writeEvictLock.wLock();
    { info->setInvalid(); }
    info->writeEvictLock.wUnlock();

    sendUnlock(new_bitmap, new_state, addr.nodeID, dirKey,
               RawMessageType::R_UNLOCK_EVICT);
    // agent_stats_inst.control_packet_send_count[iId] += 1;
    return true;
    break;
  }

  default:
    assert(false);
  };

  assert(false);

  // if(agent_stats_inst.is_start()) agent_stats_inst.evictLine_2[iId]++;
  return false;
}

bool Cache::evictLineDirty(const GlobalAddress &addr, LineInfo *info) {

  uint32_t dirKey = addr.getDirKey();
  sendMessage2Dir(info, RawMessageType::R_EVICT_DIRTY, addr.nodeID, dirKey);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  struct ibv_wc wc;

  pollWithCQ(iCon->cq, 1, &wc);

  assert(wc.opcode == IBV_WC_RECV);
  RawMessage *m = (RawMessage *)iCon->message->getMessage();

  switch (m->mtype) {
  case RawMessageType::M_LOCK_FAIL:
  case RawMessageType::M_CHECK_FAIL:
  case RawMessageType::DIR_2_APP_MISS_SWITCH: {

    // if(agent_stats_inst.is_start()) agent_stats_inst.evictLine_3[iId]++;
    return false;
    break;
  }

  case RawMessageType::R_EVICT_DIRTY:
  case RawMessageType::DIR_2_APP_EVICT_DIRTY: {

    uint8_t dirID = dirKey % NR_DIRECTORY;

    rdmaWrite(iCon->data[dirID][addr.nodeID], (uint64_t)info->data,
              remoteInfo[addr.nodeID].dsmBase + DirKey2Addr(dirKey),
              DSM_CACHE_LINE_SIZE, iCon->cacheLKey,
              remoteInfo[addr.nodeID].dsmRKey[dirID], -1, true, 0);
    // agent_stats_inst.data_packet_send_count[iId] += 1;

    pollWithCQ(iCon->cq, 1, &wc);

    assert(wc.opcode == IBV_WC_RDMA_WRITE);

    info->writeEvictLock.wLock();
    { info->setInvalid(); }
    info->writeEvictLock.wUnlock();

    sendUnlock(0, RawState::S_UNSHARED, addr.nodeID, dirKey,
               RawMessageType::R_UNLOCK_EVICT);
    // agent_stats_inst.control_packet_send_count[iId] += 1;

    return true;
    break;
  }

  default:
    assert(false);
  };

  assert(false);

  // if(agent_stats_inst.is_start()) agent_stats_inst.evictLine_4[iId]++;
  return false;
}

// Sync Primtives: rw-lock
bool Cache::r_lock(const GlobalAddress &addr, uint32_t size) {
  unlock_flow_control = 0;

  uint32_t dirKey = addr.getDirKey();
  LineInfo info;

#ifdef LOCK_BASE_ADDR
  info.data = (void *)addr.addr;
#else
  info.data = (void *)GADD(addr, size - 1).getDirKey();
#endif

  // printf("issue r lock %lx\n", info.data);

  sendMessage2Dir(&info, RawMessageType::PRIMITIVE_R_LOCK, addr.nodeID, dirKey,
                  false);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_LOCK);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  struct ibv_wc wc;
  pollWithCQ(iCon->cq, 1, &wc);
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::WAIT_ASYNC_FINISH_LOCK);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  RawMessage *ack = (RawMessage *)iCon->message->getMessage();
  assert(wc.opcode == IBV_WC_RECV);

  // if (ack->mtype != RawMessageType::PRIMITIVE_R_LOCK_FAIL &&
  //     ack->mtype != RawMessageType::PRIMITIVE_R_LOCK_SUCC) {
  //   std::cout << "mtype " << (int)ack->mtype << "v " << ack->destAddr << " |
  //   "
  //             << addr.addr << " " << (int)ack->nodeID << " " <<
  //             (int)ack->appID
  //             << std::endl;
  // }

  // if (addr.addr != ack->destAddr) {
  //   printf("wrong addr %lx, ack %lx", addr.addr, ack->destAddr);
  // }

  assert(addr.addr == ack->destAddr);
  assert(dsm->myNodeID == ack->nodeID && iId == ack->appID);
  assert(ack->mtype == RawMessageType::PRIMITIVE_R_LOCK_FAIL ||
         ack->mtype == RawMessageType::PRIMITIVE_R_LOCK_SUCC);

  // if (ack->mtype == RawMessageType::PRIMITIVE_R_LOCK_SUCC) {
  //   printf("r lock succ\n");
  // } else {
  //   printf("r lock fail\n");
  // }

  return ack->mtype == RawMessageType::PRIMITIVE_R_LOCK_SUCC;
}

void Cache::r_unlock(const GlobalAddress &addr, uint32_t size) {

  LineInfo info;

#ifdef LOCK_BASE_ADDR
  uint32_t dirKey = (++unlock_flow_control) % kUnlockBatchAck == 0;
  info.data = (void *)addr.addr;
#else
  uint32_t dirKey = addr.getDirKey();
  info.data = (void *)GADD(addr, size - 1).getDirKey();
#endif

  // printf("issue un lock %lx\n", info.data);
  sendMessage2Dir(&info, RawMessageType::PRIMITIVE_R_UNLOCK, addr.nodeID,
                  dirKey, false);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_UNLOCK);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  if (dirKey) {
    struct ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
    RawMessage *ack = (RawMessage *)iCon->message->getMessage();
    assert(wc.opcode == IBV_WC_RECV);
  }
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::WAIT_ASYNC_FINISH_UNLOCK);
  
  // agent_stats_inst.start_record_app_thread(addr.addr);
}

bool Cache::w_lock(const GlobalAddress &addr, uint32_t size) {
  unlock_flow_control = 0;

  uint32_t dirKey = addr.getDirKey();
  LineInfo info;

#ifdef LOCK_BASE_ADDR
  info.data = (void *)addr.addr;
#else
  info.data = (void *)GADD(addr, size - 1).getDirKey();
#endif

  // printf("issue w lock %lx\n", info.data);
  sendMessage2Dir(&info, RawMessageType::PRIMITIVE_W_LOCK, addr.nodeID, dirKey,
                  false);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_LOCK);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  struct ibv_wc wc;
  pollWithCQ(iCon->cq, 1, &wc);
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::WAIT_ASYNC_FINISH_LOCK);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  RawMessage *ack = (RawMessage *)iCon->message->getMessage();
  assert(wc.opcode == IBV_WC_RECV);

  // if (ack->mtype != RawMessageType::PRIMITIVE_W_LOCK_FAIL &&
  //     ack->mtype != RawMessageType::PRIMITIVE_W_LOCK_SUCC) {
  //   std::cout << "mtype " << (int)ack->mtype << "v " << ack->destAddr << " |
  //   "
  //             << addr.addr << " " << (int)ack->nodeID << " " <<
  //             (int)ack->appID
  //             << std::endl;
  // }
  // if (addr.addr != ack->destAddr) {
  //   printf("wrong addr %lx, ack %lx\n", addr.addr, ack->destAddr);
  // }

  assert(addr.addr == ack->destAddr);
  assert(dsm->myNodeID == ack->nodeID && iId == ack->appID);
  assert(ack->mtype == RawMessageType::PRIMITIVE_W_LOCK_FAIL ||
         ack->mtype == RawMessageType::PRIMITIVE_W_LOCK_SUCC);

  // if (ack->mtype == RawMessageType::PRIMITIVE_W_LOCK_SUCC) {
  //   printf("w lock succ\n");
  // } else {
  //   printf("w lock fail\n");
  // }

  return ack->mtype == RawMessageType::PRIMITIVE_W_LOCK_SUCC;
}

void Cache::w_unlock(const GlobalAddress &addr, uint32_t size) {

  LineInfo info;

#ifdef LOCK_BASE_ADDR
  uint32_t dirKey = (++unlock_flow_control) % kUnlockBatchAck == 0;
  info.data = (void *)addr.addr;
#else
  uint32_t dirKey = addr.getDirKey();
  info.data = (void *)GADD(addr, size - 1).getDirKey();
#endif

  // printf("issue un lock %lx\n", info.data);
  sendMessage2Dir(&info, RawMessageType::PRIMITIVE_W_UNLOCK, addr.nodeID,
                  dirKey, false);
  // agent_stats_inst.control_packet_send_count[iId] += 1;
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_UNLOCK);

  // agent_stats_inst.start_record_app_thread(addr.addr);
  if (dirKey) {
    struct ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
    RawMessage *ack = (RawMessage *)iCon->message->getMessage();
    assert(wc.opcode == IBV_WC_RECV);
  }
  // agent_stats_inst.stop_record_app_thread_with_op(addr.addr, APP_THREAD_OP::WAIT_ASYNC_FINISH_UNLOCK);
  
  // agent_stats_inst.start_record_app_thread(addr.addr);
}

GlobalAddress Cache::malloc(size_t size, bool align) {
  bool need_new_chunck = false;
  auto res = alloc->malloc(size, need_new_chunck, align);

  assert(size <= define::kChunkSize);
  if (need_new_chunck) {
    // call remote home node for new chunk
    LineInfo info;
    sendMessage2Dir(&info, RawMessageType::PRIMITIVE_ALLOC, next_allocator_node,
                    0, false);

    struct ibv_wc wc;
    pollWithCQ(iCon->cq, 1, &wc);
    RawMessage *ack = (RawMessage *)iCon->message->getMessage();

    GlobalAddress new_chunck;

    new_chunck.nodeID = next_allocator_node;
    new_chunck.addr = ack->destAddr;

    alloc->set_chunck(new_chunck);
    res = alloc->malloc(size, need_new_chunck, align);
    assert(need_new_chunck == false);

    next_allocator_node = (next_allocator_node + 1) % dsm->conf.machineNR;
  }

  return res;
}

void Cache::free(const GlobalAddress &addr) { alloc->free(addr); }
