#ifndef __CACHEAGENTCONNECTION_H__
#define __CACHEAGENTCONNECTION_H__

#include "Common.h"
#include "RawMessageConnection.h"

struct RemoteConnection;

// cache agent thread
struct CacheAgentConnection {

    uint16_t agentID;

    RdmaContext ctx;
    ibv_cq *cq;

    RawMessageConnection *message;

    // for each dir
    ibv_qp **data[NR_DIRECTORY];

    ibv_qp **toApp[MAX_APP_THREAD];

    ibv_mr *cacheMR;
    void *cachePool;
    uint32_t cacheLKey;
    RemoteConnection *remoteInfo;

    CacheAgentConnection(uint16_t agentID, void *cachePool, uint64_t cacheSize,
                         uint32_t machineNR, RemoteConnection *remoteInfo,
                         const uint8_t mac[6]);

    void sendMessage(RawMessage *m) { message->sendRawMessage(m); }
};

#endif /* __CACHEAGENTCONNECTION_H__ */
