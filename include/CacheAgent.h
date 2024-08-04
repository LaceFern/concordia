#ifndef __CACHEAGENT_H__
#define __CACHEAGENT_H__

#include <thread>
#include "RawMessageConnection.h"

enum AgentPendingReason: uint8_t {
    NOP,
    WAIT_WRITE_BACK_2_SHARED,
    WAIT_WRITE_BACK_2_INVALID,
};

union AgentWrID {
    uint64_t wrId;
    struct {
        uint32_t padding1;
        uint16_t padding2;
        uint8_t  fingerprint;
        AgentPendingReason type;
        // uint8_t nodeID;
    };
} __attribute__((packed));

static_assert(sizeof(AgentWrID) == sizeof(uint64_t), "XXX");

class CacheAgentConnection;
class RemoteConnection;
class Cache;

struct LineInfo;

class CacheAgent {

   public:
    CacheAgent(CacheAgentConnection *cCon, RemoteConnection *remoteInfo,
               Cache *cache, uint32_t machineNR, uint16_t agentID, uint16_t nodeID);

   private:
    CacheAgentConnection *cCon;
    RemoteConnection *remoteInfo;
    Cache *cache;

    uint16_t mybitmap;

    uint32_t machineNR;
    uint16_t agentID;
    uint16_t nodeID;

    uint16_t queueID;

    std::thread *agent;

    std::thread *queueTh;
    std::thread *processTh;

    void agentThread();

    void processThread();
    void queueThread();

    void processSwitchMessage(RawMessage *m);
    void sendAck2App(RawMessage *m, RawMessageType type);

    void processMessage(RawMessage *m);
    void processReadMissInv(RawMessage *m);
    void processWriteMissInvShared(RawMessage *m);
    void processWriteMissInvDirty(RawMessage *m);
    void processWriteSharedInv(RawMessage *m);

    void sendData2Dir(RawMessage *m, LineInfo *l, RawMessageType type, uint64_t wrId);
    void sendData2App(RawMessage *m, LineInfo *l, uint64_t wrId);
    void ack2Dir(RawMessage *m, RawMessageType type);

    void processImmRet(AgentWrID w);

};

#endif /* __CACHEAGENT_H__ */
