#include "Directory.h"
#include "Common.h"

#include "Connection.h"
#include "RecvImmBatch.h"
#include "SwitchManager.h"

#include "agent_stat.h"

// #include <gperftools/profiler.h>

#ifndef BASELINE
// #define ENABLE_SWITCH_CC
#endif

uint64_t dirSendDataCounter = 0;
uint64_t dirSendControlCounter = 0;
uint64_t dirRecvDataCounter = 0;
uint64_t dirRecvControlCounter = 0;

thread_local bool can_add_switch = true;

Directory::Directory(DirectoryConnection *dCon, RemoteConnection *remoteInfo,
                     uint32_t machineNR, uint16_t dirID, uint16_t nodeID,
                     Controller *controller)
    : dCon(dCon), remoteInfo(remoteInfo), machineNR(machineNR), dirID(dirID),
      nodeID(nodeID), controller(controller), dirTh(nullptr) {

  assert(sizeof(RawImm) == sizeof(uint32_t));

  switchManager = new SwitchManager(controller, nodeID);

  for (int i = 0; i < NR_CACHE_AGENT; ++i) {
    for (size_t k = 0; k < machineNR; ++k) {
      new RecvImmBatch(dCon->data2cache[i][k], POST_RECV_PER_RC_QP);
      // for (int n = 0; n < POST_RECV_PER_RC_QP; ++n) {
      //   rdmaReceive(dCon->data2cache[i][k], 0, 0, 0);
      // }
    }
  }

  for (int i = 0; i < MAX_APP_THREAD; ++i) {
    for (size_t k = 0; k < machineNR; ++k) {
      new RecvImmBatch(dCon->data2app[i][k], POST_RECV_PER_RC_QP);
      // for (int n = 0; n < POST_RECV_PER_RC_QP; ++n) {
      //   rdmaReceive(dCon->data2app[i][k], 0, 0, 0);
      // }
    }
  }

  { // chunck alloctor
    GlobalAddress dsm_start;
    uint64_t per_directory_dsm_size = dCon->dsmSize / NR_DIRECTORY;
    dsm_start.nodeID = nodeID;
    dsm_start.addr = per_directory_dsm_size * dirID;
    chunckAlloc = new GlobalAllocator(dsm_start, per_directory_dsm_size);
  }

  // Debug::notifyInfo("start insert table");
  // // switchManager->addAll(machineNR);
  // Debug::notifyInfo("end start insert table");

  sysID = dirID;
  dirTh = new std::thread(&Directory::dirThread, this);
  // sysID = dirID;
  // if(dirID < agent_stats_inst.dir_queue_num){
  //   queueID = dirID;
  //   agent_stats_inst.queues[queueID] = new SPSC_QUEUE(MAX_WORKER_PENDING_MSG);
  //   // agent_stats_inst.safe_queues[queueID] = new SafeQueue<queue_entry>();

  //   // countTh = new std::thread(&Directory::countThread, this);
  //   processTh = new std::thread(&Directory::processThread, this);
  //   dirTh = new std::thread(&Directory::queueThread, this);
  // }
  // else{
  //   dirTh = new std::thread(&Directory::dirThread, this);
  // }
}

Directory::~Directory() {

  delete switchManager;
  delete chunckAlloc;
}

void Directory::init_switch() {
  uint64_t block_size = 31 * (1024ull) * 1024;
  uint64_t start = (2ull * 1 + 1) * block_size;
  uint64_t cnt = 2; // block_size / 4096;
  for (int i = start / 4096; cnt > 0; i++, cnt--) {
    BlockInfo *info = new BlockInfo();
    dir_map[i] = info;
    assert(push_to_switch(i, info));

    struct ibv_wc wc;
    pollWithCQ(dCon->cq, 1, &wc);
    auto m = (RawMessage *)dCon->message->getMessage();
    processOwnershipChange(m);
  }

  Debug::notifyInfo("init switch succ");
}

void Directory::countThread() {
  bindCore(NUMA_CORE_NUM - NR_CACHE_AGENT - NR_DIRECTORY - queueID);
  Debug::notifyInfo("dir count %d launch!\n", dirID);

  uint64_t start, end;
  double cpu_frequency = 2.2; // Assume CPU frequency in GHz
  uint64_t cycles_per_second = static_cast<uint64_t>(cpu_frequency * 1e9);

  while (true) {
      start = agent_stats_inst.rdtsc();
      uint64_t elapsed_cycles = 0;
      // Busy-wait loop until approximately 1 second has passed
      do {
          end = agent_stats_inst.rdtsc();
          elapsed_cycles = end - start;
      } while (elapsed_cycles < cycles_per_second);
      double elapsed_seconds = elapsed_cycles / (cpu_frequency * 1e9);
      
      printf("\ndir queue %d receives %d packets\n", dirID, agent_stats_inst.get_home_recv_count(queueID));
      printf("dir pure %d process %d packets\n", dirID, agent_stats_inst.get_home_process_count(queueID));
      
      printf("dir queue %d op = %d\n", dirID, agent_stats_inst.debug_info_1);
      printf("dir queue %d queueid = %d\n", dirID, agent_stats_inst.debug_info_5);
      printf("dir pure %d op = %d (%d, %d)\n", dirID, agent_stats_inst.debug_info_2, agent_stats_inst.debug_info_3, agent_stats_inst.debug_info_4);
      printf("dir pure %d branch = %d\n", dirID, agent_stats_inst.debug_info);
      printf("dir pure %d queueid = %d\n", dirID, agent_stats_inst.debug_info_6);
  }
}

void Directory::queueThread() {
  bindCore(NUMA_CORE_NUM - 1 - NR_CACHE_AGENT - dirID);
  Debug::notifyInfo("dir queue %d launch!\n", dirID);

  // printf("IBV_WC_RECV = %d, IBV_WC_RDMA_WRITE = %d, IBV_WC_RECV_RDMA_WITH_IMM = %d", 
  //     IBV_WC_RECV, IBV_WC_RDMA_WRITE, IBV_WC_RECV_RDMA_WITH_IMM);

  while(true){
    struct ibv_wc wc;
    pollWithCQ(dCon->cq, 1, &wc);

    // printf("checkpoint 0 on dir queue %d: receive a packet!\n", dirID);

    uint64_t now_time_tsc = agent_stats_inst.rdtsc();
    // agent_stats_inst.debug_info_1 = wc.opcode;
    // agent_stats_inst.debug_info_5 = queueID;
    struct queue_entry entry;
    entry.wc = wc;
    entry.starting_point = now_time_tsc;
    (agent_stats_inst.queues[queueID])->push(entry);
    // while (!(agent_stats_inst.queues[queueID]->try_push(queue_entry{ wc, now_time_tsc, 0 }))) ;
    // std::atomic_thread_fence(std::memory_order_release);
    // while (!((agent_stats_inst.queues[queueID])->try_push(entry))) ;
    // while (!((agent_stats_inst.queues[queueID])->try_push(queue_entry{ wc, now_time_tsc, 0 }))) ;
    // agent_stats_inst.update_home_recv_count(sysID);
    // agent_stats_inst.update_home_recv_count(queueID);
    // (agent_stats_inst.safe_queues[queueID])->enqueue(queue_entry{ wc, now_time_tsc, 0 });
  }
}

void Directory::processThread() {
  bindCore(NUMA_CORE_NUM - 1 - NR_CACHE_AGENT - NR_DIRECTORY - queueID);
  Debug::notifyInfo("dir pure %d launch!!!", dirID);

  while (true) {
    queue_entry entry = (agent_stats_inst.queues[queueID])->pop();
    // std::atomic_thread_fence(std::memory_order_acquire);
    // queue_entry entry = (agent_stats_inst.queues[queueID])->pop();
    // agent_stats_inst.update_home_process_count(queueID);
    // queue_entry entry = (agent_stats_inst.safe_queues[queueID])->dequeue();
    // printf("checkpoint 0 on dir pure %d: process a packet!\n", dirID);

    uint64_t waiting_time = agent_stats_inst.rdtscp() - entry.starting_point;
    (void)waiting_time;
    ibv_wc &wc = entry.wc;
    // struct ibv_wc wc = entry.wc;

    agent_stats_inst.start_record_multi_sys_thread(queueID);
    uint64_t proc_starting_point = agent_stats_inst.rdtsc();
    MULTI_SYS_THREAD_OP res_op = MULTI_SYS_THREAD_OP::NONE;

    // agent_stats_inst.debug_info_6 = queueID;
    // agent_stats_inst.debug_info_2 = wc.opcode;
    // agent_stats_inst.debug_info = 0;
    // agent_stats_inst.debug_info_3 = (int(wc.opcode) == IBV_WC_RECV);
    // agent_stats_inst.debug_info_4 = IBV_WC_RECV;
    switch (int(wc.opcode)) {
      case IBV_WC_RECV: // control message
      {
        dirRecvControlCounter++;
        auto m = (RawMessage *)dCon->message->getMessage();

        if(agent_stats_inst.is_valid_gaddr(DirKey2Addr(m->dirKey))){
          // //debug
          // printf("processThread: Im here!\n");
          res_op = MULTI_SYS_THREAD_OP::PROCESS_IN_HOME_NODE;
          agent_stats_inst.update_home_recv_count(sysID);
        }

        printRawMessage(m, "dir recv");

        if (m->state == RawState::S_UNDEFINED ||
            m->mtype == RawMessageType::R_UNLOCK ||
            m->mtype == RawMessageType::R_READ_MISS_UNLOCK ||
            m->mtype == RawMessageType::R_UNLOCK_EVICT) {
          // agent_stats_inst.debug_info = 1;
          processSwitchMiss(m);
        } else if (m->mtype >= RawMessageType::ADD_DIR &&
                 m->mtype <= RawMessageType::ADD_DIR_SUCC) {
          // agent_stats_inst.debug_info = 2;
          processOwnershipChange(m);
        }
#ifdef BASELINE
        else if (m->mtype == RawMessageType::AGENT_ACK_WRITE_MISS_BASELINE ||
                m->mtype == RawMessageType::AGENT_ACK_WRITE_SHARED_BASELINE) {
          processAgentAckBaseline(m);
        }
#endif
        else {
          // agent_stats_inst.debug_info = 3;
          processSwitchHit(m);
        }
        break;
      }
      case IBV_WC_RDMA_WRITE: {
        // agent_stats_inst.debug_info = 4;
        break;
      }
      case IBV_WC_RECV_RDMA_WITH_IMM: {

        dirRecvDataCounter++;
        RawImm imm = *(RawImm *)(&wc.imm_data);

        if (imm.mtype == RawMessageType::R_READ_MISS) {
          // agent_stats_inst.debug_info = 5;
          RawMessage m;
          m.nodeID = imm.nodeID;
          m.appID = imm.appID;
          m.state = RawState::S_UNDEFINED;
          sendAck2AppByPassSwitch(&m,
                                RawMessageType::N_DIR_ACK_APP_READ_MISS_DIRTY);
          agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
        }
        else{
          // agent_stats_inst.debug_info = 6;
        }
        break;
      }
      default:
        // agent_stats_inst.debug_info = 7;
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

void Directory::dirThread() {

  bindCore(NUMA_CORE_NUM - 1 - NR_CACHE_AGENT - dirID);
  Debug::notifyInfo("dir %d launch!\n", dirID);

  // test_change_ownership();

  while (true) {
    struct ibv_wc wc;
    pollWithCQ(dCon->cq, 1, &wc);

    switch (int(wc.opcode)) {
    case IBV_WC_RECV: // control message
    {
      dirRecvControlCounter++;
      auto m = (RawMessage *)dCon->message->getMessage();

      if(agent_stats_inst.is_valid_gaddr(DirKey2Addr(m->dirKey))){
        // //debug
        // printf("dirThread: Im here!\n");
        agent_stats_inst.update_home_recv_count(sysID);
      }

      printRawMessage(m, "dir recv");

      if (m->state == RawState::S_UNDEFINED ||
          m->mtype == RawMessageType::R_UNLOCK ||
          m->mtype == RawMessageType::R_READ_MISS_UNLOCK ||
          m->mtype == RawMessageType::R_UNLOCK_EVICT) {
        processSwitchMiss(m);
      } else if (m->mtype >= RawMessageType::ADD_DIR &&
                 m->mtype <= RawMessageType::ADD_DIR_SUCC) {
        processOwnershipChange(m);

      }
#ifdef BASELINE
      else if (m->mtype == RawMessageType::AGENT_ACK_WRITE_MISS_BASELINE ||
               m->mtype == RawMessageType::AGENT_ACK_WRITE_SHARED_BASELINE) {
        processAgentAckBaseline(m);
      }
#endif
      else {
        processSwitchHit(m);
      }
      break;
    }
    case IBV_WC_RDMA_WRITE: {
      break;
    }
    case IBV_WC_RECV_RDMA_WITH_IMM: {
      dirRecvDataCounter++;
      RawImm imm = *(RawImm *)(&wc.imm_data);

      if (imm.mtype == RawMessageType::R_READ_MISS) {

        RawMessage m;
        m.nodeID = imm.nodeID;
        m.appID = imm.appID;
        m.state = RawState::S_UNDEFINED;
        sendAck2AppByPassSwitch(&m,
                                RawMessageType::N_DIR_ACK_APP_READ_MISS_DIRTY);
        agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
      }
      break;
    }
    default:
      assert(false);
    }
  }
}

void Directory::processSwitchMiss(RawMessage *m) {

  if (m->mtype >= PRIMITIVE_R_LOCK && m->mtype <= PRIMITIVE_FREE) {

    switch (m->mtype) {
    // lock or unlock
    case RawMessageType::PRIMITIVE_R_LOCK: {
      primitive_r_lock(m);
      break;
    }
    case RawMessageType::PRIMITIVE_R_UNLOCK: {
      primitive_r_unlock(m);
      break;
    }
    case RawMessageType::PRIMITIVE_W_LOCK: {
      primitive_w_lock(m);
      break;
    }
    case RawMessageType::PRIMITIVE_W_UNLOCK: {
      primitive_w_unlock(m);
      break;
    }

    // alloc
    case RawMessageType::PRIMITIVE_ALLOC: {

      auto chunck = chunckAlloc->alloc_chunck();

      // printf("malloc chunck %lx\n", chunck.addr);
      sendAck2AppByPassSwitch(m, RawMessageType::PRIMITIVE_ALLOC_ACK,
                              chunck.addr);
      agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
      break;
    }

    default:
      assert(false);
    }
    return;
  }

  auto it = dir_map.find(m->dirKey);

  BlockInfo *block = nullptr;
  if (it == dir_map.end()) { // new
    block = new BlockInfo();
    dir_map[m->dirKey] = block;
  } else {
    block = it->second;
  }

  if (block->in_switch) {
    sendAck2AppByPassSwitch(m, RawMessageType::DIR_2_APP_MISS_SWITCH);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  if (m->is_app_req == 1) { // is same as in switch
    m->bitmap = block->bitmap;
    m->state = block->state;
  }

#ifdef VERBOSE
  printf("dir %d %s %x, from (%d, %d)\n", nodeID, strType(m->mtype), m->dirKey,
         m->nodeID, m->appID);
#endif

  assert((1 << m->nodeID) == m->mybitmap);
  switch (m->mtype) {
  case RawMessageType::R_READ_MISS: {
    in_host_read_miss(m, block);
    break;
  }

  case RawMessageType::R_WRITE_MISS: {
    in_host_write_miss(m, block);
    break;
  }

  case RawMessageType::R_WRITE_SHARED: {
    in_host_write_share(m, block);
    break;
  }

  case RawMessageType::R_EVICT_DIRTY: {
    in_host_evict_dirty(m, block);
    break;
  }

  case RawMessageType::R_EVICT_SHARED: {
    in_host_evict_share(m, block);
    break;
  }

  case RawMessageType::R_UNLOCK: {
    in_host_unlock(m, block, false);
    break;
  }
  case RawMessageType::R_READ_MISS_UNLOCK: {
    in_host_unlock(m, block, true);
    break;
  }
  case RawMessageType::R_UNLOCK_EVICT: {
    in_host_unlock(m, block, false, true);
    break;
  }

  default:
    printf("ERROR dir\n");
    exit(-1);
    assert(false);
  }
}

void Directory::processSwitchHit(RawMessage *m) {

#ifdef VERBOSE
  printf("dir(switch) %d %s %x, from (%d, %d)\n", nodeID, strType(m->mtype),
         m->dirKey, m->nodeID, m->appID);
#endif

  if (m->mtype == RawMessageType::R_READ_MISS) {
    assert(m->state != RawState::S_DIRTY);
    sendData2App(m);
    agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
  } else if (m->mtype == RawMessageType::R_WRITE_MISS) {
    assert(m->state != RawState::S_DIRTY);
    sendData2App(m);
    agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
  } else {
    assert(false);
  }
}

void Directory::in_host_read_miss(RawMessage *m, BlockInfo *block) {
  // if (!block->lock.try_rLock()) {
  if (!block->lock.try_wLock()) {
    sendAck2AppByPassSwitch(m, RawMessageType::M_LOCK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  if ((m->mybitmap & block->bitmap) != 0) {
    // block->lock.rUnlock();
    block->lock.wUnlock();
    sendAck2AppByPassSwitch(m, RawMessageType::M_CHECK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  switch (block->state) {
  case RawState::S_UNSHARED:
  case RawState::S_SHARED: {
    sendData2App(m);
    agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    break;
  }
  case RawState::S_DIRTY: {
    assert(bits_in(block->bitmap) == 1);
    for (size_t i = 0; i < sizeof(block->bitmap) * 8; ++i) {
      if (((block->bitmap >> i) & 1) == 1) {
        block->invalidation_cnt++;
        sendMessage2Agent(m, i, RawMessageType::DIR_2_AGENT_READ_MISS_DIRTY);
        agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
        break;
      }
    }
    break;
  }
  case RawState::S_UNDEFINED:
  default: { assert(false); }
  }
}

void Directory::in_host_write_miss(RawMessage *m, BlockInfo *block) {
  if (!block->lock.try_wLock()) {
    sendAck2AppByPassSwitch(m, RawMessageType::M_LOCK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  if ((m->mybitmap & block->bitmap) != 0) {
    block->lock.wUnlock();
    sendAck2AppByPassSwitch(m, RawMessageType::M_CHECK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  switch (block->state) {
  case RawState::S_UNSHARED: {
    sendData2App(m);
    agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    break;
  }
  case RawState::S_SHARED: {

#ifdef BASELINE
    block->processing_request = RawMessageType::R_WRITE_MISS;
#endif

    for (size_t i = 0; i < sizeof(block->bitmap) * 8; ++i) {
      if (((block->bitmap >> i) & 1) == 1) {
        block->invalidation_cnt++;
        sendMessage2Agent(m, i, RawMessageType::DIR_2_AGENT_WRITE_MISS_SHARED);
        agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
      }
    }
    sendData2App(m);
    agent_stats_inst.data_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    break;
  }
  case RawState::S_DIRTY: {
    assert(bits_in(block->bitmap) == 1);
    for (size_t i = 0; i < sizeof(block->bitmap) * 8; ++i) {
      if (((block->bitmap >> i) & 1) == 1) {
        block->invalidation_cnt++;
        sendMessage2Agent(m, i, RawMessageType::DIR_2_AGENT_WRITE_MISS_DIRTY);
        agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
        break;
      }
    }
    break;
  }
  case RawState::S_UNDEFINED:
  default: { assert(false); }
  }
}

void Directory::in_host_write_share(RawMessage *m, BlockInfo *block) {
  if (!block->lock.try_wLock()) {
    sendAck2AppByPassSwitch(m, RawMessageType::M_LOCK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  if (block->state != RawState::S_SHARED ||
      (m->mybitmap & block->bitmap) == 0) {
    block->lock.wUnlock();
    sendAck2AppByPassSwitch(m, RawMessageType::M_CHECK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

#ifdef BASELINE
  bool has_share_nodes = false;
#endif
  for (size_t i = 0; i < sizeof(block->bitmap) * 8; ++i) {
    if (((block->bitmap >> i) & 1) == 1 && i != m->nodeID) {
      block->invalidation_cnt++;
      sendMessage2Agent(m, i, RawMessageType::DIR_2_AGENT_WRITE_SHARED);
      agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
#ifdef BASELINE
      has_share_nodes = true;
#endif
    }
  }

#ifdef BASELINE
  if (!has_share_nodes) {
    sendAck2AppByPassSwitch(m, RawMessageType::DIR_2_APP_WRITE_SHARED_BASELINE);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
  } else {
    block->processing_request = RawMessageType::R_WRITE_SHARED;
  }
#else
  sendAck2AppByPassSwitch(m, RawMessageType::DIR_2_APP_WRITE_SHARED);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
#endif
}

void Directory::in_host_evict_share(RawMessage *m, BlockInfo *block) {
  if (!block->lock.try_wLock()) {
    sendAck2AppByPassSwitch(m, RawMessageType::M_LOCK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  if (block->state != RawState::S_SHARED ||
      (m->mybitmap & block->bitmap) == 0) {
    block->lock.wUnlock();
    sendAck2AppByPassSwitch(m, RawMessageType::M_CHECK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  sendAck2AppByPassSwitch(m, RawMessageType::DIR_2_APP_EVICT_SHARED);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
}

void Directory::in_host_evict_dirty(RawMessage *m, BlockInfo *block) {
  if (!block->lock.try_wLock()) {
    sendAck2AppByPassSwitch(m, RawMessageType::M_LOCK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  if (block->state != RawState::S_DIRTY || (m->mybitmap & block->bitmap) == 0) {
    block->lock.wUnlock();
    sendAck2AppByPassSwitch(m, RawMessageType::M_CHECK_FAIL);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
    return;
  }

  sendAck2AppByPassSwitch(m, RawMessageType::DIR_2_APP_EVICT_DIRTY);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
}

void Directory::in_host_unlock(RawMessage *m, BlockInfo *block,
                               bool is_read_miss, bool is_evict) {
  block->state = (RawState)m->state;

  if (is_read_miss) {
    block->bitmap |= m->bitmap;
    block->lock.wUnlock();
    // block->lock.rUnlock();
  } else {
    block->bitmap = m->bitmap;
    block->lock.wUnlock();
  }

#ifdef UNLOCK_SYNC
  if (!is_evict) {
    sendAck2AppByPassSwitch(m, RawMessageType::UNLOCK_ACK);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
  }
#endif

#ifdef ENABLE_SWITCH_CC
  if (can_add_switch && block->invalidation_cnt > 10 && !block->have_try) {
    push_to_switch(m->dirKey, block);
    // printf("%d \n", m->dirKey);
  }
#endif

  // printf("--- %s 0x%x\n", strState(block->state), block->bitmap);
}

void Directory::processOwnershipChange(RawMessage *m) {
  switch (m->mtype) {
  case RawMessageType::ADD_DIR_SUCC: {

    assert(m->dirKey == 0);

    auto dirKey = RawMessage::get_dirKey_from_tag(m->tag);
    auto *block = dir_map[dirKey];

#ifdef VERBOSE
    printf("dir %d push switch succ %x, %d\n", this->nodeID, dirKey,
           this->nodeID);
#endif
    block->in_switch = true;
    block->lock.wUnlock();
    break;
  }
  case RawMessageType::ADD_DIR_FAIL: {

    assert(m->dirKey != 0);
    auto dirKey = RawMessage::get_dirKey_from_tag(m->tag);
#ifdef VERBOSE
    printf("dir %d push switch fail %x, %d\n", this->nodeID, dirKey,
           this->nodeID);
#endif
    auto *block = dir_map[dirKey];
    block->lock.wUnlock();
    break;
  }
  case RawMessageType::DEL_DIR_SUCC: {
    auto *block = dir_map[m->dirKey];
    block->bitmap = m->bitmap;
    block->state = (RawState)m->state;
    block->in_switch = false;
    break;
  }

  case RawMessageType::DEL_DIR_FAIL:
  case RawMessageType::DEL_DIR: {
    break;
  }

  default: { assert(false); }
  }
}

bool Directory::push_to_switch(uint32_t dirKey, BlockInfo *block) {
  if (!block->lock.try_wLock()) {
    return false;
  }
  block->have_try = true;

  assert(block->in_switch == false);
  addDir2Switch(dirKey, block);
  return true;
}

bool Directory::pull_from_switch(uint32_t dirKey) {
  auto block = dir_map[dirKey];

  assert(block->in_switch == true);

  delDir2Switch(dirKey);

  return true;
}

////////////////////////////////////////////////////////////////////////////

void Directory::sendData2App(const RawMessage *m) {

  RawImm imm;
  imm.bitmap = m->bitmap;
  imm.state = m->state;

  rdmaWrite(dCon->data2app[m->appID][m->nodeID],
            (uint64_t)dCon->dsmPool + DirKey2Addr(m->dirKey), m->destAddr,
            DSM_CACHE_LINE_SIZE, dCon->dsmLKey,
            remoteInfo[m->nodeID].appRKey[m->appID], imm.imm, true, 0);

  dirSendDataCounter++;
}

void Directory::sendAck2AppByPassSwitch(const RawMessage *from_message,
                                        RawMessageType type, uint64_t value) {
  dirSendControlCounter++;

  RawMessage *m = (RawMessage *)dCon->message->getSendPool();
  memcpy(m, from_message, sizeof(RawMessage));

  m->qpn = dCon->message->getQPN();
  m->mtype = type;
  m->is_app_req = 0;
  m->destAddr = value;

  m->invalidate_tag();

  printRawMessage(m, "dir send");

  dCon->sendMessage(m);
}

void Directory::sendMessage2Agent(const RawMessage *m, uint8_t agentID,
                                  RawMessageType type) {
  dirSendControlCounter++;

  RawMessage *control = (RawMessage *)dCon->message->getSendPool();
  memcpy(control, m, sizeof(RawMessage));

  control->qpn = dCon->message->getQPN();

  control->mtype = type;
  control->agentID = agentID;
  control->is_app_req = 0;

  printRawMessage(control, "dir send");

  dCon->sendMessage(control);
}

void Directory::addDir2Switch(uint32_t dirKey, const BlockInfo *block) {
  RawMessage *m = (RawMessage *)dCon->message->getSendPool();
  m->qpn = dCon->message->getQPN();
  m->mtype = RawMessageType::ADD_DIR;

  m->bitmap = (block->bitmap);
  m->state = (block->state);

  m->dirKey = dirKey;
  m->dirNodeID = nodeID;
  m->is_app_req = 0;

  m->set_tag_and_index();

#ifdef VERBOSE
  printf("dir %d try to push switch %x, %d\n", this->nodeID, dirKey,
         this->nodeID);
#endif

  printRawMessage(m, "dir add");

  dCon->sendMessage(m);
}

void Directory::delDir2Switch(uint32_t dirKey) {
  RawMessage *m = (RawMessage *)dCon->message->getSendPool();
  m->qpn = dCon->message->getQPN();
  m->mtype = RawMessageType::DEL_DIR;

  m->dirKey = dirKey;
  m->dirNodeID = nodeID;
  m->is_app_req = 0;

  m->set_tag_and_index();

  printRawMessage(m, "dir del");

  dCon->sendMessage(m);
}

void Directory::test_change_ownership() {
  BlockInfo block;

  block.state = RawState::S_DIRTY;

  timespec s, e;

  clock_gettime(CLOCK_REALTIME, &s);
  // uint64_t K = 10000000;
  for (size_t i = 0; i < 1; ++i) {

    block.bitmap = i + 1;
    addDir2Switch(0x123 + i, &block);

    struct ibv_wc wc;
    pollWithCQ(dCon->cq, 1, &wc);

    auto m = (RawMessage *)dCon->message->getMessage();
    printRawMessage(m, "dir recv");
  }

  sleep(8);

  clock_gettime(CLOCK_REALTIME, &e);
  double microseconds =
      (e.tv_sec - s.tv_sec) * 1000000 + (double)(e.tv_nsec - s.tv_nsec) / 1000;
  printf("%f us\n", microseconds);

  for (int i = 0; i < 1; ++i) {

    delDir2Switch(0x123 + i);

    struct ibv_wc wc;
    pollWithCQ(dCon->cq, 1, &wc);

    auto m = (RawMessage *)dCon->message->getMessage();

    printRawMessage(m, "dir recv");
  }

  while (true) {
    /* code */
  }
}

// Sync Primitive
void Directory::primitive_r_lock(RawMessage *m) {

  // printf("r lock %x\n", m->dirKey);
  bool lock_succ = true;
  // printf("process r lock %lx, for %d\n", m->destAddr, m->nodeID);

  // std::cout << "r lock" << m->dirKey << " - " << m->destAddr << std::endl;

#ifndef LOCK_BASE_ADDR
  uint64_t cur = m->dirKey;
  for (; cur <= m->destAddr; ++cur) {
    auto it = primitive_lock_table.find(cur);

    if (it == primitive_lock_table.end()) { // new
      primitive_lock_table[cur] = 2;
    } else if ((it->second & 1) == 0) {
      it->second += 2; //  lock
    } else {
      lock_succ = false;
      break;
    }
  }

  if (!lock_succ) {
    for (uint64_t unlock = m->dirKey; unlock < cur; ++unlock) {
      primitive_lock_table[unlock] -= 2;
    }
  }
#else
  auto it = primitive_lock_table.find(m->destAddr);
  if (it == primitive_lock_table.end()) { // new
    primitive_lock_table[m->destAddr] = 2;
  } else if ((it->second & 1) == 0) {
    it->second += 2; //  lock
  } else {
    lock_succ = false;
  }
#endif

  sendAck2AppByPassSwitch(m,
                          lock_succ ? RawMessageType::PRIMITIVE_R_LOCK_SUCC
                                    : RawMessageType::PRIMITIVE_R_LOCK_FAIL,
                          m->destAddr);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
}

void Directory::primitive_r_unlock(RawMessage *m) {

  // printf("r un lock %x\n", m->dirKey);

  // std::cout << "unlock" << m->dirKey << " - " << m->destAddr << std::endl;

  // printf("process un lock %lx, for %d\n", m->destAddr, m->nodeID);
#ifndef LOCK_BASE_ADDR
  int off = primitive_lock_table[m->dirKey] == 0x1 ? 1 : 2;
  for (auto cur = m->dirKey; cur <= m->destAddr; ++cur) {
    primitive_lock_table[cur] -= off;
  }
#else
  auto it = primitive_lock_table.find(m->destAddr);
  // assert(it != primitive_lock_table.end());
  if (it == primitive_lock_table.end()) {
    return;
  }
  if (it->second == 1) {
    it->second = 0;
  } else if (it->second >= 2) {
    it->second -= 2;
  }

  if (m->dirKey) {
    sendAck2AppByPassSwitch(m, RawMessageType::PRIMITIVE_UNLOCK_ACK,
                            m->destAddr);
    agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
  }

#endif
}

void Directory::primitive_w_lock(RawMessage *m) {
  //  printf("w lock %x\n", m->dirKey);
  // std::cout << "w lock" << m->dirKey << " - " << m->destAddr << std::endl;
  bool lock_succ = true;

  // printf("process w lock %lx, for %d\n", m->destAddr, m->nodeID);

#ifndef LOCK_BASE_ADDR
  uint64_t cur = m->dirKey;
  for (; cur <= m->destAddr; ++cur) {
    auto it = primitive_lock_table.find(cur);
    if (it == primitive_lock_table.end()) { // new
      primitive_lock_table[cur] = 1;
    } else if (it->second == 0) {
      it->second = 1; //  lock
    } else {
      lock_succ = false;
      break;
    }
  }

  if (!lock_succ) {
    for (uint64_t unlock = m->dirKey; unlock < cur; ++unlock) {
      primitive_lock_table[unlock] = 0;
    }
  }
#else
  if (m->destAddr == 0) {
    can_add_switch = false;
    primitive_lock_table.clear();
    printf("SYNC\n");
  } else {
    auto it = primitive_lock_table.find(m->destAddr);
    if (it == primitive_lock_table.end()) { // new
      primitive_lock_table[m->destAddr] = 1;
    } else if (it->second == 0) {
      it->second = 1; //  lock
    } else {
      lock_succ = false;
    }
  }

#endif

  sendAck2AppByPassSwitch(m,
                          lock_succ ? RawMessageType::PRIMITIVE_W_LOCK_SUCC
                                    : RawMessageType::PRIMITIVE_W_LOCK_FAIL,
                          m->destAddr);
  agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
}

void Directory::primitive_w_unlock(RawMessage *m) {
  // printf("w un lock %x\n", m->dirKey);
  primitive_r_unlock(m);
}

void Directory::processAgentAckBaseline(RawMessage *m) {
#ifdef BASELINE

  auto block = dir_map[m->dirKey];
  if (m->mtype == RawMessageType::AGENT_ACK_WRITE_MISS_BASELINE) {
    assert(block->processing_request == RawMessageType::R_WRITE_MISS);
  } else if (m->mtype == RawMessageType::AGENT_ACK_WRITE_SHARED_BASELINE) {
    assert(block->processing_request == RawMessageType::R_WRITE_SHARED);
  } else {
    assert(false);
  }

  assert((m->mybitmap & block->bitmap) != 0);

  block->bitmap &= ~(m->mybitmap);

  if (block->processing_request == RawMessageType::R_WRITE_MISS) {
    if (block->bitmap == 0) {
      sendAck2AppByPassSwitch(m, RawMessageType::DIR_2_APP_WRITE_MISS_BASELINE);
      agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
      block->processing_request = RawMessageType::NOP_REQUEST;
    }
  } else if (block->processing_request == RawMessageType::R_WRITE_SHARED) {
    if (bits_in(block->bitmap) == 1) {
      sendAck2AppByPassSwitch(m,
                              RawMessageType::DIR_2_APP_WRITE_SHARED_BASELINE);
      agent_stats_inst.control_packet_send_count[MAX_APP_THREAD + sysID] += 1;
      block->processing_request = RawMessageType::NOP_REQUEST;
    }
  } else {
    assert(false);
  }

#endif
}