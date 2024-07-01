#include "CacheAgentConnection.h"

CacheAgentConnection::CacheAgentConnection(uint16_t agentID, void *cachePool,
                                           uint64_t cacheSize,
                                           uint32_t machineNR,
                                           RemoteConnection *remoteInfo, const uint8_t mac[6])
    : agentID(agentID), remoteInfo(remoteInfo) {

    createContext(&ctx);
    cq = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);

    message = new RawMessageConnection(ctx, cq, DIR_MESSAGE_NR, mac);
    message->initRecv();
    message->initSend();

    // cache memory
    this->cachePool = cachePool;
    cacheMR = createMemoryRegion((uint64_t)cachePool, cacheSize, &ctx);
    cacheLKey = cacheMR->lkey;

    // dir, RC
    for (int i = 0; i < NR_DIRECTORY; ++i) {
        data[i] = new ibv_qp *[machineNR];
        for (size_t k = 0; k < machineNR; ++k) {
            createQueuePair(&data[i][k], IBV_QPT_RC, cq, &ctx);
        }
    }

    // app, RC
    for (int i = 0; i < MAX_APP_THREAD; ++i) {
        toApp[i] = new ibv_qp *[machineNR];
        for (size_t k = 0; k < machineNR; ++k) {
            createQueuePair(&toApp[i][k], IBV_QPT_RC, cq, &ctx);
        }
    }

}
