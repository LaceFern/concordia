
#include <infiniband/verbs.h>

#include "Debug.h"

bool rdmaReceive(ibv_qp *qp, uint64_t source, uint64_t size, uint32_t lkey,
                 uint64_t wr_id);

class RecvImmBatch {
public:
  RecvImmBatch(ibv_qp *qp, int size) : qp(qp), size(size) {
    for (int n = 0; n < size; ++n) {
      rdmaReceive(qp, 0, 0, 0, (uint64_t)this);
    }
    left_size = size;

    recvs = new ibv_recv_wr[size / 2];
    sgl = new ibv_sge[size / 2];

    for (int i = 0; i < size / 2; ++i) {
      auto &s = sgl[i];
      memset(&s, 0, sizeof(s));

      auto &r = recvs[i];
      memset(&r, 0, sizeof(r));
      r.sg_list = &s;
      r.num_sge = 1;
      r.wr_id = (uint64_t)this;
      r.next = (i == size / 2 - 1) ? NULL : &recvs[i + 1];
    }
  }

  ~RecvImmBatch() {
    delete[] recvs;
    delete[] sgl;
  }

  void try_recv() {

    left_size--;
    if (left_size < size / 2) {
      struct ibv_recv_wr *bad;
      if (ibv_post_recv(qp, recvs, &bad)) {
        Debug::notifyError("Receive failed.");
      }
      left_size += size / 2;
    }
  }

private:
  ibv_qp *qp;
  int size;
  int left_size;
  ibv_recv_wr *recvs;
  ibv_sge *sgl;
};