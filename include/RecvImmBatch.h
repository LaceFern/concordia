
#include <infiniband/verbs.h>

#include "Debug.h"

bool rdmaReceive(ibv_qp *qp, uint64_t source, uint64_t size, uint32_t lkey,
                 uint64_t wr_id);

class RecvImmBatch {
public:

  RecvImmBatch(ibv_qp *qp, int size, void *rq_buffer, uint32_t lkey) : qp(qp), size(size), rq_buffer(rq_buffer), messageLkey(lkey) {
    assert(size < 1024);
    messageNR = size;
    subNR = messageNR / kBatchCount;
    messagePool = rq_buffer;
    message = qp;
    messageLkey = lkey;
    for (int i = 0; i < kBatchCount; ++i) {
      recvs[i] = new ibv_recv_wr[subNR];
      recv_sgl[i] = new ibv_sge[subNR];
    }
    for (int k = 0; k < kBatchCount; ++k) {
      for (size_t i = 0; i < subNR; ++i) {
        auto &s = recv_sgl[k][i];
        memset(&s, 0, sizeof(s));
        s.addr = (uint64_t)messagePool + (k * subNR + i) * MESSAGE_SIZE;
        s.length = MESSAGE_SIZE;
        s.lkey = messageLkey;

        auto &r = recvs[k][i];
        memset(&r, 0, sizeof(r));
        r.sg_list = &s;
        r.num_sge = 1;
        r.wr_id = (uint64_t)this;
        r.next = (i == subNR - 1) ? NULL : &recvs[k][i + 1];
      }
    }

    struct ibv_recv_wr *bad;
    for (int i = 0; i < kBatchCount; ++i) {
      if (ibv_post_recv(message, &recvs[i][0], &bad)) {
        Debug::notifyError("Receive failed.");
      }
    }
  }

  // RecvImmBatch(ibv_qp *qp, int size) : qp(qp), size(size) {
  //   for (int n = 0; n < size; ++n) {
  //     //post 1 slot to rq in rdmaReceive function
  //     rdmaReceive(qp, 0, 0, 0, (uint64_t)this);
  //   }
  //   left_size = size;

  //   recvs = new ibv_recv_wr[size / 2];
  //   sgl = new ibv_sge[size / 2];

  //   for (int i = 0; i < size / 2; ++i) {
  //     auto &s = sgl[i];
  //     memset(&s, 0, sizeof(s));

  //     auto &r = recvs[i];
  //     memset(&r, 0, sizeof(r));
  //     r.sg_list = &s;
  //     r.num_sge = 1;
  //     r.wr_id = (uint64_t)this;
  //     r.next = (i == size / 2 - 1) ? NULL : &recvs[i + 1];
  //   }
  // }

  ~RecvImmBatch() {
    // delete[] recvs;
    // delete[] sgl;
    for (int i = 0; i < kBatchCount; ++i) {
      delete[] recvs[i];
      delete[] recv_sgl[i];
    }
    delete[] recvs;
    delete[] recv_sgl;
  }

  void try_recv() {
    struct ibv_recv_wr *bad;
    ADD_ROUND(curMessage, messageNR);
    if (curMessage % subNR == 0) {
      if (ibv_post_recv(
              message,
              &recvs[(curMessage / subNR - 1 + kBatchCount) % kBatchCount][0],
              &bad)) {
        Debug::notifyError("Receive failed.");
      }
    }
  }

  char * get_message_and_try_recv() {
    struct ibv_recv_wr *bad;
    char *m = (char *)messagePool + curMessage * MESSAGE_SIZE;
    ADD_ROUND(curMessage, messageNR);
    if (curMessage % subNR == 0) {
      if (ibv_post_recv(
              message,
              &recvs[(curMessage / subNR - 1 + kBatchCount) % kBatchCount][0],
              &bad)) {
        Debug::notifyError("Receive failed.");
      }
    }
    return m;
  }

  // void try_recv() {
  //   //post size/2 slots to rq
  //   left_size--;
  //   if (left_size < size / 2) {
  //     struct ibv_recv_wr *bad;
  //     if (ibv_post_recv(qp, recvs, &bad)) {
  //       Debug::notifyError("Receive failed.");
  //     }
  //     left_size += size / 2;
  //   }
  // }

  const static int kBatchCount = 4;
private:
  ibv_qp *qp;
  void *rq_buffer;
  int size;
  int left_size;
  // ibv_recv_wr *recvs;
  // ibv_sge *sgl;
  ibv_qp *message;
  void *messagePool;
  uint16_t curMessage;
  uint16_t messageNR;
  ibv_recv_wr *recvs[kBatchCount];
  ibv_sge *recv_sgl[kBatchCount];
  uint32_t subNR;
  uint32_t messageLkey;
};