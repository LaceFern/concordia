#include "RawMessageConnection.h"

#include <cassert>

#define DST_MAC 0x00, 0x01, 0x02, 0x03, 0x04, 0x05
#define SRC_MAC 0xe4, 0x1d, 0x2d, 0xf3, 0xdd, 0xcc
#define ETH_TYPE 0x08, 0x00
#define IP_HDRS                                                                \
  0x45, 0x00, 0x00, (32 + sizeof(RawMessage)), 0x00, 0x00, 0x40, 0x00, 0x40, 0x11, 0xaf, 0xb6
#define SRC_IP 0x0d, 0x07, 0x38, 0x66
#define DST_IP 0x0d, 0x07, 0x38, 0x7f
#define SRC_PORT 0x00, 0x00
#define DEST_PORT 0x22, 0xb8
#define UDP_OTHER 0x00, (8 + sizeof(RawMessage)), 0x00, 0x00

RawMessageConnection::RawMessageConnection(RdmaContext &ctx, ibv_cq *cq,
                                           uint32_t messageNR,
                                           const uint8_t mac[6])
    : AbstractMessageConnection(IBV_QPT_RAW_PACKET, 42, 42, ctx, cq,
                                messageNR) {

  assert(sizeof(RawMessage) == 31);
  assert(sizeof(RawMessage) + sendPadding <= MESSAGE_SIZE);
  steeringWithMacUdp(message, &ctx, mac, toBigEndian16(8888), message->qp_num);
}

void RawMessageConnection::initSend() {
  unsigned char header[] = {DST_MAC, SRC_MAC,  ETH_TYPE,  IP_HDRS,  SRC_IP,
                   DST_IP,  SRC_PORT, DEST_PORT, UDP_OTHER};
  assert(sizeof(header) == sendPadding);

  for (int i = 0; i < messageNR; ++i) {
    memcpy((char *)sendPool + i * MESSAGE_SIZE, header, sendPadding);
  }
}

void RawMessageConnection::sendRawMessage(RawMessage *m) {

  if ((sendCounter & SIGNAL_BATCH) == 0 && sendCounter > 0) {
    ibv_wc wc;
    char msg[MESSAGE_SIZE] = {0};
    pollWithCQ(send_cq, 1, &wc, msg);
  }

  rdmaRawSend(message, (uint64_t)m - sendPadding,
              sizeof(RawMessage) + sendPadding, messageLkey,
              (sendCounter & SIGNAL_BATCH) == 0);

  ++sendCounter;
}
