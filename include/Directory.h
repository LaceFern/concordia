#ifndef __DIRECTORY_H__
#define __DIRECTORY_H__

#include <thread>

#include <unordered_map>

#include "Common.h"

#include "Connection.h"
#include "GlobalAllocator.h"

struct BlockInfo {
  WRLock lock;
  uint16_t bitmap;
  RawState state;

  bool in_switch;
  bool have_try;

  uint64_t invalidation_cnt;

#ifdef BASELINE

  RawMessageType processing_request;
#endif

  BlockInfo()
      : bitmap(0), state(RawState::S_UNSHARED), in_switch(false),
        have_try(false), invalidation_cnt(0) {
    lock.init();

#ifdef BASELINE
    processing_request = NOP_REQUEST;
#endif
  }
};

class SwitchManager;
class Controller;
class Directory {
public:
  Directory(DirectoryConnection *dCon, RemoteConnection *remoteInfo,
            uint32_t machineNR, uint16_t dirID, uint16_t nodeID,
            Controller *controller);

  ~Directory();

private:
  DirectoryConnection *dCon;
  RemoteConnection *remoteInfo;

  uint32_t machineNR;
  uint16_t dirID;
  uint16_t nodeID;

  uint16_t queueID;

  Controller *controller;
  SwitchManager *switchManager;

  std::thread *dirTh;

  std::thread *queueTh;
  std::thread *processTh;
  std::thread *countTh;

  std::unordered_map<uint64_t, BlockInfo *> dir_map;
  std::unordered_map<uint64_t, uint16_t> primitive_lock_table;

  GlobalAllocator *chunckAlloc;

  void dirThread();

  void processThread();
  void queueThread();
  void countThread();

  void processSwitchMiss(RawMessage *m);
  void processSwitchHit(RawMessage *m);
  void processOwnershipChange(RawMessage *m);
  void processAgentAckBaseline(RawMessage *m);

  void sendData2App(const RawMessage *m);
  void sendAck2AppByPassSwitch(const RawMessage *, RawMessageType type,
                               uint64_t value = 0);
  void sendMessage2Agent(const RawMessage *m, uint8_t agentID, RawMessageType);
  void addDir2Switch(uint32_t dirKey, const BlockInfo *block);
  void delDir2Switch(uint32_t dirKey);

  void in_host_read_miss(RawMessage *m, BlockInfo *block);
  void in_host_write_miss(RawMessage *m, BlockInfo *block);
  void in_host_write_share(RawMessage *m, BlockInfo *block);
  void in_host_unlock(RawMessage *m, BlockInfo *block, bool is_read_miss,
                      bool is_evict = false);
  void in_host_evict_share(RawMessage *m, BlockInfo *block);
  void in_host_evict_dirty(RawMessage *m, BlockInfo *block);

  // sync primitive, bind to cache line
  void primitive_r_lock(RawMessage *);
  void primitive_r_unlock(RawMessage *);
  void primitive_w_lock(RawMessage *);
  void primitive_w_unlock(RawMessage *);

  bool push_to_switch(uint32_t dirKey, BlockInfo *block);
  bool pull_from_switch(uint32_t dirKey);

  void init_switch();

  void test_change_ownership();
};

#endif /* __DIRECTORY_H__ */
