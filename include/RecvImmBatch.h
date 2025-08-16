
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

// bool rdmaReceive(ibv_qp *qp, uint64_t source, uint64_t size, uint32_t lkey,
//                  uint64_t wr_id);


// class RecvImmBatch {
// public:
//     RecvImmBatch(ibv_qp *qp, int size);

//     ~RecvImmBatch();

//     RecvImmBatch(const RecvImmBatch&) = delete;
//     RecvImmBatch& operator=(const RecvImmBatch&) = delete;

//     /**
//      * @brief 当一个接收操作完成时调用此函数，用于补充一个新的接收请求到硬件队列。
//      *        这维持了可用接收缓冲区的数量，确保系统在高负载下稳定。
//      */
//     void try_recv();

// private:
//     ibv_qp *qp;
//     int size;

//     ibv_recv_wr *recvs; // 工作请求(WR)数组，大小为 'size'
//     ibv_sge *sgl;       // SGE数组，大小为 'size'
//     ibv_mr* dummy_mr;   // 用于接收立即数的“虚拟”缓冲区的内存句柄

//     char* dummy_buffer;       // 1字节的虚拟缓冲区
//     int next_wr_to_repost;  // 用于循环使用WR结构体的索引
// };

// // --- 实现 ---

// RecvImmBatch::RecvImmBatch(ibv_qp *qp, int size)
//     : qp(qp), size(size), recvs(nullptr), sgl(nullptr), 
//       dummy_mr(nullptr), dummy_buffer(nullptr), next_wr_to_repost(0) {

//     dummy_buffer = new char[1];
//     dummy_mr = ibv_reg_mr(qp->pd, dummy_buffer, 1, IBV_ACCESS_LOCAL_WRITE);
//     if (!dummy_mr) {
//         Debug::notifyError("Failed to register dummy buffer for RecvImmBatch.");
//         return;
//     }

//     recvs = new ibv_recv_wr[size];
//     sgl = new ibv_sge[size];

//     for (int i = 0; i < size; ++i) {
//         sgl[i].addr = (uint64_t)dummy_buffer;
//         sgl[i].length = 1;
//         sgl[i].lkey = dummy_mr->lkey;

//         recvs[i].wr_id = (uint64_t)this;
//         recvs[i].sg_list = &sgl[i];
//         recvs[i].num_sge = 1;
//         recvs[i].next = (i == size - 1) ? nullptr : &recvs[i + 1];
//     }

//     // 4. 【修复】一次性、高效地批量提交整个链表
//     struct ibv_recv_wr *bad_wr = nullptr;
//     if (ibv_post_recv(qp, &recvs[0], &bad_wr)) {
//         Debug::notifyError("Initial batch post_recv failed.");
//     }
// }

// RecvImmBatch::~RecvImmBatch() {
//     // 按相反顺序安全地释放所有资源
//     if (dummy_mr) {
//         ibv_dereg_mr(dummy_mr);
//     }
//     delete[] dummy_buffer;
//     delete[] recvs;
//     delete[] sgl;
// }

// void RecvImmBatch::try_recv() {
//     // 这个函数的逻辑被完全重构，以实现正确、高效的“一对一”补充

//     // 1. 从我们的WR数组中获取下一个可用的WR结构体
//     //    我们循环使用这些预先分配好的结构体
//     ibv_recv_wr* wr_to_post = &recvs[next_wr_to_repost];
    
//     // 2. 【关键】确保我们只提交一个WR，而不是一个链表
//     wr_to_post->next = nullptr;
    
//     // 3. 提交这个单独的接收请求
//     struct ibv_recv_wr *bad_wr = nullptr;
//     if (ibv_post_recv(qp, wr_to_post, &bad_wr)) {
//         // 在高负载下，这里的失败通常意味着QP进入了错误状态
//         Debug::notifyError("Failed to repost a single receive WR.");
//     }

//     // 4. 更新索引，准备下一次调用
//     next_wr_to_repost = (next_wr_to_repost + 1) % size;
// }