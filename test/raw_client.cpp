#include "RawMessageConnection.h"

#include <iostream>

int main() {

    RdmaContext ctx;
    createContext(&ctx);
    ibv_cq *cq = ibv_create_cq(ctx.ctx, 128, NULL, NULL, 0);

    uint8_t *mac = (uint8_t *)getMac();
    auto message = new RawMessageConnection(ctx, cq, 32, mac);

    message->initRecv();
    message->initSend();

    while (true) {
        struct ibv_wc wc;
        pollWithCQ(cq, 1, &wc);

        assert(wc.opcode == IBV_WC_RECV);

        RawMessage *m = (RawMessage *)message->getMessage();

        std::cout << "appID: " << (int)m->appID << std::endl;
        std::cout << "bitmap: " << (int)m->bitmap << std::endl;
    }


}
