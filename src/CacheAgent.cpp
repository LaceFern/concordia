#include "CacheAgent.h"

#include "Cache.h"
#include "Common.h"
#include "Connection.h"
#include "RecvImmBatch.h"
#include "agent_stat.h"

uint64_t agentSendDataCounter = 0;
uint64_t agentSendControlCounter = 0;
uint64_t agentRecvDataCounter = 0;
uint64_t agentRecvControlCounter = 0;
uint64_t agentWriteShared = 0;
uint64_t agentWriteMissDirty = 0;
uint64_t agentWriteMissShared = 0;
uint64_t agentReadMissDirty = 0;

static_assert(sizeof(AgentWrID) == sizeof(uint64_t), "XX");
CacheAgent::CacheAgent(CacheAgentConnection *cCon, RemoteConnection *remoteInfo,
                       Cache *cache, uint32_t machineNR, uint16_t agentID,
                       uint16_t nodeID)
    : cCon(cCon), remoteInfo(remoteInfo), cache(cache), machineNR(machineNR),
      agentID(agentID), nodeID(nodeID), agent(nullptr) {

  for (int i = 0; i < NR_DIRECTORY; ++i) {
    for (size_t k = 0; k < machineNR; ++k) {
      void *rq_buffer = NULL; // TODO: partition buffer in agent_stat and assigned here!
      new RecvImmBatch(cCon->data[i][k], POST_RECV_PER_RC_QP, rq_buffer, cCon->cacheLKey);
      // for (int n = 0; n < POST_RECV_PER_RC_QP; ++n) {
      //   rdmaReceive(dCon->data2cache[i][k], 0, 0, 0);
      // }
    }
  }

  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    for (size_t k = 0; k < machineNR; ++k) {
      void *rq_buffer = NULL; // TODO: partition buffer in agent_stat and assigned here!
      new RecvImmBatch(cCon->toApp[i][k], POST_RECV_PER_RC_QP, rq_buffer, cCon->cacheLKey);
      // for (int n = 0; n < POST_RECV_PER_RC_QP; ++n) {
      //   rdmaReceive(dCon->data2app[i][k], 0, 0, 0);
      // }
    }
  }

  mybitmap = 1 << nodeID;

  sysID = NR_DIRECTORY + agentID;
  agent = new std::thread(&CacheAgent::agentThread, this);
  // sysID = NR_DIRECTORY + agentID;
  // if(agentID < agent_stats_inst.cache_queue_num){
  //   queueID = agent_stats_inst.dir_queue_num + agentID;
  //   agent_stats_inst.queues[queueID] = new SPSC_QUEUE(MAX_WORKER_PENDING_MSG);

  //   processTh = new std::thread(&CacheAgent::processThread, this);
  //   agent = new std::thread(&CacheAgent::queueThread, this);
  // }
  // else{
  //   agent = new std::thread(&CacheAgent::agentThread, this);
  // }
}

void CacheAgent::queueThread() {
  bindCore(NUMA_CORE_NUM - 1 - agentID);
  Debug::notifyInfo("cache queue %d launch!\n", agentID);

  while(true){
    struct ibv_wc wc;
    char msg[MESSAGE_SIZE] = {0};
    pollWithCQ(cCon->cq, 1, &wc, msg);
    uint64_t now_time_tsc = agent_stats_inst.rdtsc();

    struct queue_entry entry;
    entry.wc = wc;
    entry.starting_point = now_time_tsc;
    for(int i = 0; i < MESSAGE_SIZE; i++){
      entry.msg[i] = msg[i];
    }
    (agent_stats_inst.queues[queueID])->push(entry);

    // while (!(agent_stats_inst.queues[queueID]->try_push(queue_entry{ wc, now_time_tsc, 0 }))) ;
    // std::atomic_thread_fence(std::memory_order_release);
    // agent_stats_inst.update_cache_recv_count(queueID);
    // agent_stats_inst.update_cache_recv_count(sysID);
  }
}

void CacheAgent::processThread() {
  bindCore(NUMA_CORE_NUM - 1 - NR_CACHE_AGENT - NR_DIRECTORY - queueID);
  Debug::notifyInfo("cache pure %d launch!\n", agentID);

  while (true) {
    queue_entry entry = agent_stats_inst.queues[queueID]->pop();
    // std::atomic_thread_fence(std::memory_order_acquire);
    uint64_t waiting_time = agent_stats_inst.rdtscp() - entry.starting_point;
    (void)waiting_time;
    ibv_wc &wc = entry.wc;

    agent_stats_inst.start_record_multi_sys_thread(queueID);
    uint64_t proc_starting_point = agent_stats_inst.rdtsc();
    MULTI_SYS_THREAD_OP res_op = MULTI_SYS_THREAD_OP::NONE;
    
    RawMessage *m;
    switch (int(wc.opcode)) {
    case IBV_WC_RECV: // control message

      // m = (RawMessage *)cCon->message->getMessage();
      m = (RawMessage *)entry.msg;

      if(agent_stats_inst.is_valid_gaddr(DirKey2Addr(m->dirKey))){
        res_op = MULTI_SYS_THREAD_OP::PROCESS_IN_CACHE_NODE;
        agent_stats_inst.update_cache_recv_count(sysID);
      }

      printRawMessage(m, "agent recv");
      processSwitchMessage(m);
      break;
    case IBV_WC_SEND:
      assert(false);
      break;
    case IBV_WC_RDMA_WRITE:
      processImmRet(*(AgentWrID *)&wc.wr_id);
      break;
    case IBV_WC_RECV_RDMA_WITH_IMM:
      assert(false);
      break;
    default:
      assert(false);
    }

    // TODO:debug, find correct addr
    if(agent_stats_inst.is_start()){
      uint64_t proc_ending_point = agent_stats_inst.rdtscp();
      uint64_t proc_time = proc_ending_point - proc_starting_point;
      if(res_op != MULTI_SYS_THREAD_OP::NONE){
        // agent_stats_inst.stop_record_multi_sys_thread_with_op(queueID, res_op);
        agent_stats_inst.record_multi_sys_thread_with_op(queueID, proc_time, res_op);
        agent_stats_inst.record_poll_thread_with_op(queueID, waiting_time, MULTI_POLL_THREAD_OP::WAITING_IN_SYSTHREAD_QUEUE);
      }
      else{
        agent_stats_inst.record_multi_sys_thread_with_op(queueID, proc_time, MULTI_SYS_THREAD_OP::PROCESS_NOT_TARGET);
        // agent_stats_inst.stop_record_multi_sys_thread_with_op(queueID, MULTI_SYS_THREAD_OP::PROCESS_NOT_TARGET);
        agent_stats_inst.record_poll_thread_with_op(queueID, waiting_time, MULTI_POLL_THREAD_OP::WAITING_NOT_TARGET);
      }
    }
  }
}

void CacheAgent::agentThread() {

  bindCore(NUMA_CORE_NUM - 1 - agentID);
  // bindCore(NUMA_CORE_NUM - agentID);

  Debug::notifyInfo("cache agent %d launch!\n", agentID);
  while (true) {
    struct ibv_wc wc;
    RawMessage *m;

    char msg[MESSAGE_SIZE] = {0};
    pollWithCQ(cCon->cq, 1, &wc, msg);
    // agent_stats_inst.update_cache_recv_count(sysID);

    // printf("checkpoing 1 in cache agent %d\n", agentID);
    
    // Debug::notifyError("HHH");
    switch (int(wc.opcode)) {
    case IBV_WC_RECV: // control message
      // m = (RawMessage *)cCon->message->getMessage();
      m = (RawMessage *)msg;

      if(agent_stats_inst.is_valid_gaddr(DirKey2Addr(m->dirKey))){
        agent_stats_inst.update_cache_recv_count(sysID);
      }

      printRawMessage(m, "agent recv");
      processSwitchMessage(m);
      break;
    case IBV_WC_SEND:
      assert(false);
      break;
    case IBV_WC_RDMA_WRITE:
      processImmRet(*(AgentWrID *)&wc.wr_id);
      break;
    case IBV_WC_RECV_RDMA_WITH_IMM:
      assert(false);
      break;
    default:
      assert(false);
    }
  }
}

void CacheAgent::processSwitchMessage(RawMessage *m) {
  if (m->state == RawState::S_SHARED) {
    if (m->mtype == RawMessageType::R_WRITE_SHARED ||
        m->mtype == RawMessageType::DIR_2_AGENT_WRITE_SHARED) {

      assert(((m->bitmap >> nodeID) & 1) == 1);
      assert(((m->bitmap >> m->nodeID) & 1) == 1);

      agentWriteShared++;
      processWriteSharedInv(m);
    } else if (m->mtype == RawMessageType::R_WRITE_MISS ||
               m->mtype == RawMessageType::DIR_2_AGENT_WRITE_MISS_SHARED) {

      assert(((m->bitmap >> nodeID) & 1) == 1);
      assert(((m->bitmap >> m->nodeID) & 1) == 0);

      agentWriteMissShared++;
      processWriteMissInvShared(m);
    } else {
      assert(false);
    }
  } else if (m->state == RawState::S_DIRTY) {
    if (m->mtype == RawMessageType::R_READ_MISS ||
        m->mtype == RawMessageType::DIR_2_AGENT_READ_MISS_DIRTY) {

      assert(((m->bitmap >> nodeID) & 1) == 1);
      assert(((m->bitmap >> m->nodeID) & 1) == 0);

      agentReadMissDirty++;
      processReadMissInv(m);
    } else if (m->mtype == RawMessageType::R_WRITE_MISS ||
               m->mtype == RawMessageType::DIR_2_AGENT_WRITE_MISS_DIRTY) {

      assert(((m->bitmap >> nodeID) & 1) == 1);
      assert(((m->bitmap >> m->nodeID) & 1) == 0);

      agentWriteMissDirty++;
      processWriteMissInvDirty(m);
    } else {
      assert(false);
    }
  } else {
    assert(false);
  }
}

void CacheAgent::processReadMissInv(RawMessage *m) {

#ifdef VERBOSE
  printf("agent %d read miss inv (%x %d), requster %d\n", this->nodeID,
         m->dirKey, m->dirNodeID, m->nodeID);
#endif

  auto line = cache->findLineForAgent(m->dirNodeID, m->dirKey);

#ifdef VERBOSE
  bool succ = line->getStatus() == CacheStatus::BEING_EVICT ||
              line->getStatus() == CacheStatus::MODIFIED ||
              line->getStatus() == CacheStatus::BEING_SHARED ||
              line->getStatus() == CacheStatus::SHARED;
  if (!succ) {
    printf("agent %d read miss inv failed (%x %d), requster %d, %s\n",
           this->nodeID, m->dirKey, m->dirNodeID, m->nodeID,
           strCacheStatus(line->getStatus()));
    assert(false);
  }

#else
  assert(line->getStatus() == CacheStatus::BEING_EVICT ||
         line->getStatus() == CacheStatus::MODIFIED ||
         line->getStatus() == CacheStatus::BEING_SHARED ||
         line->getStatus() == CacheStatus::SHARED);
#endif

  if (line->getStatus() == CacheStatus::BEING_SHARED ||
      line->getStatus() == CacheStatus::SHARED) { // concurrent read miss
    AgentWrID w;
    w.wrId = (uint64_t)line;
    w.type = AgentPendingReason::NOP;

    m->state = CacheStatus::SHARED;
    sendData2App(m, line, w.wrId);
    agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  AgentWrID w;
  w.wrId = (uint64_t)line;

  line->writeEvictLock.wLock();
#ifdef READ_MISS_DIRTY_TO_DIRTY
  line->setStatus(CacheStatus::BEING_INVALID);
#else
  line->setStatus(CacheStatus::BEING_SHARED);
#endif
  line->writeEvictLock.wUnlock();

#ifdef READ_MISS_DIRTY_TO_DIRTY
  w.type = AgentPendingReason::WAIT_WRITE_BACK_2_INVALID;
  sendData2App(m, line, w.wrId);
  agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
#else
  w.type = AgentPendingReason::WAIT_WRITE_BACK_2_SHARED;
  sendData2Dir(m, line, RawMessageType::R_READ_MISS, w.wrId);
  agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;

  w.type = AgentPendingReason::NOP;
  sendData2App(m, line, w.wrId);
  agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
#endif
}

void CacheAgent::processWriteMissInvShared(RawMessage *m) {

#ifdef VERBOSE
  printf("agent %d write miss shared (%x %d), requster %d\n", this->nodeID,
         m->dirKey, m->dirNodeID, m->nodeID);
#endif

  auto line = cache->findLineForAgent(m->dirNodeID, m->dirKey);

#ifdef VERBOSE
  bool succ = (line->getStatus() == CacheStatus::BEING_EVICT ||
               line->getStatus() == CacheStatus::SHARED ||
               line->getStatus() == CacheStatus::BEING_SHARED);

  if (!succ) {
    printf("agent %d write miss shared failed (%x %d), requster %d, %s\n",
           this->nodeID, m->dirKey, m->dirNodeID, m->nodeID,
           strCacheStatus(line->getStatus()));

    assert(false);
  }
#else
  assert(line->getStatus() == CacheStatus::BEING_EVICT ||
         line->getStatus() == CacheStatus::SHARED ||
         line->getStatus() == CacheStatus::BEING_SHARED);
#endif

  line->writeEvictLock.wLock();
  { line->setInvalid(); }
  line->writeEvictLock.wUnlock();

#ifndef BASELINE
  sendAck2App(m, RawMessageType::AGENT_ACK_WRITE_MISS);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
#else
  sendAck2App(m, RawMessageType::AGENT_ACK_WRITE_MISS_BASELINE);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
#endif
}

void CacheAgent::processWriteMissInvDirty(RawMessage *m) {

#ifdef VERBOSE
  printf("agent %d write miss dirty (%x %d), requster %d\n", this->nodeID,
         m->dirKey, m->dirNodeID, m->nodeID);
#endif

  auto line = cache->findLineForAgent(m->dirNodeID, m->dirKey);

#ifdef VERBOSE
  if ((!line->getStatus() == CacheStatus::BEING_EVICT) &&
      (!line->getStatus() == CacheStatus::MODIFIED)) {

    printf("agent %d write miss dirty failed (%x %d), requster %d, %s\n",
           this->nodeID, m->dirKey, m->dirNodeID, m->nodeID,
           strCacheStatus(line->getStatus()));

    assert(false);
  }
#else
  assert(line->getStatus() == CacheStatus::BEING_EVICT ||
         line->getStatus() == CacheStatus::MODIFIED);
#endif

  line->writeEvictLock.wLock();
  { line->setStatus(CacheStatus::BEING_INVALID); }
  line->writeEvictLock.wUnlock();

  AgentWrID w;
  w.wrId = (uint64_t)line;
  w.type = AgentPendingReason::WAIT_WRITE_BACK_2_INVALID;

  sendData2App(m, line, w.wrId);
  agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
}

void CacheAgent::processWriteSharedInv(RawMessage *m) {

#ifdef VERBOSE
  printf("agent %d write shared (%x %d), requster %d\n", this->nodeID,
         m->dirKey, m->dirNodeID, m->nodeID);
#endif
  auto line = cache->findLineForAgent(m->dirNodeID, m->dirKey);

  if (line->getStatus() != CacheStatus::BEING_EVICT) {
    assert(line->getStatus() == CacheStatus::SHARED ||
           line->getStatus() == CacheStatus::BEING_SHARED);
  }

  line->writeEvictLock.wLock();
  { line->setInvalid(); }
  line->writeEvictLock.wUnlock();

#ifndef BASELINE
  sendAck2App(m, RawMessageType::AGENT_ACK_WRITE_SHARED);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
#else
  sendAck2App(m, RawMessageType::AGENT_ACK_WRITE_SHARED_BASELINE);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
#endif
}

//////////////////////////////////////////////////////////////////////////
void CacheAgent::sendData2Dir(RawMessage *m, LineInfo *l, RawMessageType type,
                              uint64_t wrId) {

  agentSendDataCounter++;
  uint16_t dirID = m->dirKey % NR_DIRECTORY;

  RawImm imm;
  imm.mtype = type;
  imm.nodeID = m->nodeID;
  imm.appID = m->appID;
  imm.agentNodeID = nodeID;

  rdmaWrite(cCon->data[dirID][m->dirNodeID], (uint64_t)l->data,
            remoteInfo[m->dirNodeID].dsmBase + DirKey2Addr(m->dirKey),
            DSM_CACHE_LINE_SIZE, cCon->cacheLKey,
            remoteInfo[m->dirNodeID].dsmRKey[dirID], imm.imm, true, wrId);
}

void CacheAgent::sendData2App(RawMessage *m, LineInfo *l, uint64_t wrId) {
  agentSendDataCounter++;
  RawImm imm;
  imm.bitmap = m->bitmap;
  imm.state = m->state;

  rdmaWrite(cCon->toApp[m->appID][m->nodeID], (uint64_t)l->data, m->destAddr,
            DSM_CACHE_LINE_SIZE, cCon->cacheLKey,
            remoteInfo[m->nodeID].appRKey[m->appID], imm.imm, true, wrId);
}

void CacheAgent::sendAck2App(RawMessage *m, RawMessageType type) {

  agentSendControlCounter++;
  RawMessage *ack = (RawMessage *)cCon->message->getSendPool();

  *ack = *m;

  ack->qpn = cCon->message->getQPN();
  ack->mtype = type;
  ack->mybitmap = mybitmap;
  ack->is_app_req = 0;

  printRawMessage(ack, "agent send");

  // cCon->sendMessage(ack);
  cCon->sendMsgWithRC(ack, cCon->toApp[ack->appID][ack->nodeID]);
}

///////////////////////////////////////////////////////////////////////////
void CacheAgent::processImmRet(AgentWrID w) {

  // Async Message :)
  LineInfo *info = (LineInfo *)(w.wrId & 0xffffffffffff);
  if (w.type == AgentPendingReason::WAIT_WRITE_BACK_2_SHARED) {

    // if ((info->getTag() & 0xff) == w.fingerprint)
    info->cas(CacheStatus::BEING_SHARED, CacheStatus::SHARED);
  } else if (w.type == AgentPendingReason::WAIT_WRITE_BACK_2_INVALID) {

    info->writeEvictLock.wLock();
    info->setInvalid();
    info->writeEvictLock.wUnlock();

  } else {
    assert(w.type == AgentPendingReason::NOP);
  }
}

void CacheAgent::sendMsgWithRC(RawMessage *m, ibv_qp *qp){
  rdmaSend(qp, (uint64_t)m, sizeof(RawMessage), cCon->cacheLKey, -1);
}