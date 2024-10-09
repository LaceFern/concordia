#ifndef __RAWMESSAGECONNECTION_H__
#define __RAWMESSAGECONNECTION_H__

#include "AbstractMessageConnection.h"

#include "Hash.h"

#include <thread>

#define SIZE_PER_STAGE 70000

#define NO_PRINT

// #pragma GCC diagnostic ignored "-Wwrite-strings"
enum RawMessageType : uint8_t {

  // to dir
  R_READ_MISS = 1,
  R_WRITE_MISS = 2,
  R_WRITE_SHARED = 3,
  R_EVICT_SHARED = 4,
  R_EVICT_DIRTY = 5,

  R_UNLOCK_EVICT = 7,
  R_UNLOCK = 8,
  R_READ_MISS_UNLOCK = 9,

  ADD_DIR = 12,
  DEL_DIR = 13,
  DEL_DIR_FAIL = 14,
  DEL_DIR_SUCC = 15,
  ADD_DIR_FAIL = 16,
  ADD_DIR_SUCC = 17,

  PRIMITIVE_R_LOCK = 20,
  PRIMITIVE_R_UNLOCK = 21,
  PRIMITIVE_W_LOCK = 22,
  PRIMITIVE_W_UNLOCK = 23,

  PRIMITIVE_ALLOC = 24,
  PRIMITIVE_FREE = 25,

  AGENT_ACK_WRITE_MISS_BASELINE = 30,
  AGENT_ACK_WRITE_SHARED_BASELINE = 31,

  // to app
  DIR_2_APP_WRITE_SHARED = 63,
  N_DIR_ACK_APP_READ_MISS_DIRTY = 64,
  DIR_2_APP_MISS_SWITCH = 65,
  AGENT_ACK_WRITE_MISS = 67,
  AGENT_ACK_WRITE_SHARED = 68,
  M_LOCK_FAIL = 69,
  M_CHECK_FAIL = 70,
  DIR_2_APP_EVICT_SHARED = 71,
  DIR_2_APP_EVICT_DIRTY = 72,
  UNLOCK_ACK = 73,

  PRIMITIVE_R_LOCK_SUCC = 74,
  PRIMITIVE_R_LOCK_FAIL = 75,
  PRIMITIVE_W_LOCK_SUCC = 76,
  PRIMITIVE_W_LOCK_FAIL = 77,
  PRIMITIVE_ALLOC_ACK = 78,
  PRIMITIVE_UNLOCK_ACK = 79,

  DIR_2_APP_WRITE_MISS_BASELINE = 80,
  DIR_2_APP_WRITE_SHARED_BASELINE = 81,

  // to agent
  DIR_2_AGENT_READ_MISS_DIRTY = 91,
  DIR_2_AGENT_WRITE_MISS_DIRTY = 92,
  DIR_2_AGENT_WRITE_MISS_SHARED = 93,
  DIR_2_AGENT_WRITE_SHARED = 94,

  //
  NOP_REQUEST = 111,
};

enum RawState : uint8_t {
  S_UNSHARED = 0,
  S_SHARED = 1,
  S_DIRTY = 2,
  S_UNDEFINED = 3,
};

struct RawMessage {
  uint16_t qpn;
  uint8_t mtype;

  uint32_t dirKey;

  // LITTLE ENDIAN
  uint8_t nodeID : 4; // request node id
  uint8_t dirNodeID : 4; // dir node id; PS. dir thread id is randomly assigned

  uint8_t appID; //app thread id

  union {
    uint16_t mybitmap; //cache agent node id
    uint16_t agentID; //cache agent node id; PS. dir thread id is randomly assigned
  };

  uint8_t state;
  uint16_t bitmap;

  uint8_t is_app_req;

  uint32_t index;
  uint32_t tag;

  uint64_t destAddr;

  // 1 : 27 : 4 (1 | dirKey | dirNodeID)
  void set_tag_and_index() {
    tag = get_tag(dirKey, dirNodeID);
    index = toBigEndian32(hash::murmur2(&tag, sizeof(tag)) % SIZE_PER_STAGE);
  }

  void invalidate_tag() {
    // tag = 0x1;
    // index = 0;
  }

  static uint32_t get_tag(uint32_t dirKey, uint8_t dirNodeID) {
    return (1ull << 31) | ((dirKey << 4) + dirNodeID);
  }

  static uint32_t get_dirKey_from_tag(uint32_t tag) {
    return (tag & (~(1ull << 31))) >> 4;
  }

  // uint64_t get_tag() { return (dirKey << 8) + dirNodeID; }
} __attribute__((packed));

struct RawImm {
  union {
    uint32_t imm;
    struct {
      uint16_t bitmap;
      uint8_t state;
      uint8_t padding;
    };
    struct {
      uint8_t mtype;
      uint8_t nodeID;
      uint8_t appID;
      uint8_t agentNodeID;
    };
  };
} __attribute__((packed));

static_assert(sizeof(RawImm) == 4, "XX");

inline const char *strType(uint8_t type) {
  switch (type) {
  case RawMessageType::R_READ_MISS:
    return "read_miss";
  case RawMessageType::R_WRITE_MISS:
    return "write miss";
  case RawMessageType::R_WRITE_SHARED:
    return "write shared";
  case RawMessageType::R_EVICT_SHARED:
    return "evict shared";
  case RawMessageType::R_EVICT_DIRTY:
    return "evict dirty";
  case RawMessageType::R_UNLOCK:
    return "unlock";
  case RawMessageType::R_READ_MISS_UNLOCK:
    return "read miss unlock";
  case RawMessageType::M_LOCK_FAIL:
    return "lock failed";
  // case RawMessageType::ACK_UNLOCK:
  // return "ack unlock";
  case RawMessageType::M_CHECK_FAIL:
    return "check fail";
  case RawMessageType::AGENT_ACK_WRITE_MISS:
    return "agent ack write miss";
  case RawMessageType::AGENT_ACK_WRITE_SHARED:
    return "agent ack write shared";
  case RawMessageType::N_DIR_ACK_APP_READ_MISS_DIRTY:
    return "dir ack app read miss dirty";
  case RawMessageType::DIR_2_APP_MISS_SWITCH:
    return "dir to app miss switch";
  case RawMessageType::DIR_2_AGENT_READ_MISS_DIRTY:
    return "dir to agent read miss dirty";
  case RawMessageType::DIR_2_AGENT_WRITE_MISS_DIRTY:
    return "dir to agent write miss dirty";
  case RawMessageType::DIR_2_AGENT_WRITE_MISS_SHARED:
    return "dir to agent write miss shared";
  case RawMessageType::DIR_2_AGENT_WRITE_SHARED:
    return "dir to agent write shared";
  case RawMessageType::DIR_2_APP_WRITE_SHARED:
    return "dir to app write shared";
  case RawMessageType::ADD_DIR:
    return "add dir";
  case RawMessageType::DEL_DIR:
    return "del dir";
  case RawMessageType::ADD_DIR_FAIL:
    return "add dir fail";
  case RawMessageType::ADD_DIR_SUCC:
    return "add dir succ";
  case RawMessageType::DEL_DIR_FAIL:
    return "del dir fail";
  case RawMessageType::DEL_DIR_SUCC:
    return "del dir succ";
  case RawMessageType::DIR_2_APP_EVICT_SHARED:
    return "dir 2 app evict shared";
  case RawMessageType::DIR_2_APP_EVICT_DIRTY:
    return "dir 2 app evict dirty";
  case RawMessageType::PRIMITIVE_R_LOCK:
    return "primitive r lock";
  case RawMessageType::PRIMITIVE_R_UNLOCK:
    return "primitive r unlock";
  case RawMessageType::PRIMITIVE_W_LOCK:
    return "primitive w lock";
  case RawMessageType::PRIMITIVE_W_UNLOCK:
    return "primitive w unlock";
  case RawMessageType::PRIMITIVE_R_LOCK_SUCC:
    return "primitive r lock ack succ";
  case RawMessageType::PRIMITIVE_R_LOCK_FAIL:
    return "primitive r lock ack fail";
  case RawMessageType::PRIMITIVE_W_LOCK_SUCC:
    return "primitive w lock ack succ";
  case RawMessageType::PRIMITIVE_W_LOCK_FAIL:
    return "primitive w lock ack fail";
  default:
    Debug::notifyError("- %d -", (int)type);
    assert(false);
  };
}

inline const char *strState(uint8_t state) {
  switch (state) {
  case RawState::S_DIRTY:
    return "dirty";
  case RawState::S_SHARED:
    return "shared";
  case RawState::S_UNSHARED:
    return "unshared";
  case RawState::S_UNDEFINED:
    return "undefined";
  default:
    assert(false);
  }
}

inline void printRawImm(RawImm *imm) {
#ifndef NO_PRINT
  Debug::notifyInfo("-----------------------------");
  Debug::notifyInfo("state: %s", strState(imm->state));
  Debug::notifyInfo("bitmap: 0x%x", imm->bitmap);
  Debug::notifyInfo("-----------------------------");
#endif
}

inline void alwaysPrintMessage(RawMessage *m, const char *s) {
  Debug::notifyInfo("");
  Debug::notifyError("---------------%s---------", s);
  Debug::notifyInfo("thread id: %d", std::this_thread::get_id());
  Debug::notifyInfo("mtype: %s", strType(m->mtype));
  Debug::notifyInfo("qpn: %d", m->qpn);
  Debug::notifyInfo("dirKey: 0x%x", m->dirKey);

  Debug::notifyInfo("dirNodeID: %d", m->dirNodeID);
  Debug::notifyInfo("nodeID: %d", m->nodeID);
  Debug::notifyInfo("appID: %d", m->appID);
  Debug::notifyInfo("mybitmap: 0x%x", m->mybitmap);

  Debug::notifyInfo("state: %s", strState(m->state));
  Debug::notifyInfo("bitmap: 0x%x", m->bitmap);
  Debug::notifyInfo("index : 0x%x", m->index);
  Debug::notifyInfo("destAddr: 0x%x", m->destAddr);
  Debug::notifyError("-------------%s-----------", s);
  Debug::notifyInfo("");
}

inline void printRawMessage(RawMessage *m, const char *s = "app") {
#ifndef NO_PRINT
  alwaysPrintMessage(m, s);
#endif
}

class RawMessageConnection : public AbstractMessageConnection {

public:
  RawMessageConnection(RdmaContext &ctx, ibv_cq *cq, uint32_t messageNR,
                       const uint8_t mac[6]);

  void initSend();
  void sendRawMessage(RawMessage *m);
};

#endif /* __RAWMESSAGECONNECTION_H__ */
