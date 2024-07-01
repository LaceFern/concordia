#include "DirectoryConnection.h"

#include "Connection.h"

DirectoryConnection::DirectoryConnection(uint16_t dirID, void *dsmPool,
                                         uint64_t dsmSize, uint32_t machineNR,
                                         RemoteConnection *remoteInfo, 
                                         const uint8_t mac[8])
    : dirID(dirID), remoteInfo(remoteInfo) {
    createContext(&ctx);
    cq = ibv_create_cq(ctx.ctx, RAW_RECV_CQ_COUNT, NULL, NULL, 0);
    message = new RawMessageConnection(ctx, cq, AGENT_MESSAGE_NR, mac);
    message->initRecv();
    message->initSend();

    // dsm memory
    this->dsmPool = dsmPool;
    this->dsmSize = dsmSize;
    dsmMR = createMemoryRegion((uint64_t)dsmPool, dsmSize, &ctx);
    dsmLKey = dsmMR->lkey;

    // agent, RC
    for (int i = 0; i < NR_CACHE_AGENT; ++i) {
        data2cache[i] = new ibv_qp *[machineNR];
        for (size_t k = 0; k < machineNR; ++k) {
            createQueuePair(&data2cache[i][k], IBV_QPT_RC, cq, &ctx);
        }
    }

    // app, RC
    for (int i = 0; i < MAX_APP_THREAD; ++i) {
        data2app[i] = new ibv_qp *[machineNR];
        for (size_t k = 0; k < machineNR; ++k) {
            createQueuePair(&data2app[i][k], IBV_QPT_RC, cq, &ctx);
        }
    }
}
