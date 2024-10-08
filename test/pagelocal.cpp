#include <DSM.h>
#include <bits/stdc++.h>
// #include <gperftools/profiler.h>
#include <mpi.h>
#include <sys/mman.h>

#define S2N (1e9)

#define MAXCORE (8 + 1)
#define MB (1024ULL * 1024ULL)
#define TOLERANCE (1e-2)
#define READSIZE (4096)
#define BBBBBBBBB (9)
typedef uint64_t ull;
DSM *dsm;
int UUU;
// the path of the graph
char *graph_dir;
// the num of core
int nodeNR;
// the ID of now core
int nodeID;
// the NUM
ull mynode_Num, myedge_Num, defnode_Num, defedge_Num, lnode_Num, onode_Num;

ull node_Num = 61578415;
ull edge_Num = 1468365182;
// ull node_Num = 4847571;
// ull edge_Num = 68993773;

// addedge in my core
long long totedge;
// the edge in my core

std::vector<int> edgev[2], lnodev[2], nodev[2];
std::vector<int> lnodeon[2];
ull info_num[MAXCORE + 2];
uint64_t active_N;
int edgevn, lnodevn, nodevn;
int lnodeonv;
ull count = 0;

struct Edge {
  int from;
  int to;
  bool active;
  int nextf;
  int nextt;
};

struct Node {
  int id;
  double rank;
};

struct LocalNode {
  int mainID;
  int id;
  bool active;
  double rank;
  double oldrank;
  double add;
  int nextf;
  int nextt;
};

// old 5M 100M
// big 41M 1000M
#define BIGNODENUM (64ull * MB)
#define BIGEDGENUM (1600ull * MB)

Edge *edge;
Node *node;
double *nodeoldrank;
bool *nodeactive;
LocalNode *lnode;
ull *outnum;
long long *storeid;
ull *gatherid[MAXCORE];
double *gatheradd;
double *add;
double *tmpdouble;
bool *tmpbool;
ull *tmpull;
int *Set;

inline void *hugeAlloc(size_t size) {
  void *res = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (res == MAP_FAILED) {
    LOG_ERR("%s mmap failed!\n", getIP());
  }
  return res;
}

void init_memory() {

  edge = (Edge *)hugeAlloc(BIGEDGENUM * sizeof(Edge));            // Edge
  node = (Node *)hugeAlloc(BIGNODENUM * sizeof(Node));            // Node
  nodeoldrank = (double *)hugeAlloc(BIGNODENUM * sizeof(double)); // double
  nodeactive = (bool *)hugeAlloc(BIGNODENUM * sizeof(bool));
  lnode = (LocalNode *)hugeAlloc(BIGNODENUM * sizeof(LocalNode));
  outnum = (ull *)hugeAlloc(BIGNODENUM * sizeof(ull));
  storeid = (long long *)hugeAlloc(BIGNODENUM * sizeof(long long));

  for (int i = 0; i < MAXCORE; i++) {
    gatherid[i] = (ull *)hugeAlloc(BIGNODENUM * sizeof(ull));
  }

  gatheradd = (double *)hugeAlloc(BIGNODENUM * sizeof(double));
  add = (double *)hugeAlloc(BIGNODENUM * sizeof(double));
  tmpdouble = (double *)hugeAlloc(BIGNODENUM * sizeof(double));
  tmpbool = (bool *)hugeAlloc(BIGNODENUM * sizeof(bool));
  tmpull = (ull *)hugeAlloc(BIGNODENUM * sizeof(ull));
  Set = (int *)hugeAlloc(BIGNODENUM * sizeof(int));
}

void parserArgs(int argc, char **argv) {
  graph_dir = argv[1];
  nodeNR = std::atoi(argv[2]);
}

inline uint64_t read64(GlobalAddress addr) {
  uint64_t ans = 0;
  dsm->read(addr, 8, (uint8_t *)&ans);
  return ans;
}

inline void write64(GlobalAddress addr, uint64_t ans) {
  dsm->write(addr, 8, (uint8_t *)&ans);
  return;
}

inline void AWMR(GlobalAddress addr, ull count_n, uint8_t *addrB) {
  int64_t j;

  int thread_nr = 4;
  if (count_n <= 8) {
    thread_nr = 1;
  }
#pragma omp parallel for num_threads(thread_nr)
  for (j = 0; j < count_n - READSIZE; j += READSIZE) {
    GlobalAddress addrT = addr;
    addrT.addr += j;
    dsm->read(addrT, READSIZE, (uint8_t *)&addrB[j]);
  }

  addr.addr += j;
  dsm->read(addr, (count_n - j), (uint8_t *)&addrB[j]);
}

inline void AWMW(GlobalAddress addr, ull count_n, uint8_t *addrB) {
  int64_t j;

  int thread_nr = 4;
  if (count_n <= 8) {
    thread_nr = 1;
  }
#pragma omp parallel for num_threads(thread_nr)
  for (j = 0; j < count_n - READSIZE; j += READSIZE) {
    GlobalAddress addrT = addr;
    addrT.addr += j;
    dsm->write(addrT, READSIZE, (uint8_t *)&addrB[j]);
  }

  addr.addr += j;
  dsm->write(addr, (count_n - j), (uint8_t *)&addrB[j]);
}

inline double readdouble(int nodeid, uint64_t addressP) {
  GlobalAddress addr;
  addr.nodeID = nodeid;
  addr.addr = addressP;
  double x;
  dsm->read(addr, 8, (uint8_t *)&x);
  return x;
}

inline void writedouble(uint8_t nodeid, uint64_t addressP, double x) {
  GlobalAddress addr;
  addr.nodeID = nodeid;
  addr.addr = addressP;
  dsm->write(addr, 8, (uint8_t *)&x);
  return;
}

inline int check(ull n) {
  if (n >= (nodeNR - 1) * defnode_Num) {
    return nodeNR - 1;
  }
  return (n / defnode_Num);
}

void pagerank_gather() {

  lnodev[!lnodevn].clear();
  for (ull I = 0; I < lnodev[lnodevn].size(); I++) {
    ull i = lnodev[lnodevn][I];
    if (!lnode[i].active) {
      continue;
    }
    if (UUU > BBBBBBBBB)
      lnodev[!lnodevn].push_back(i); //清理inactive的点
    add[i] = (-lnode[i].oldrank + lnode[i].rank) / outnum[lnode[i].id]; //记录一个local node可能造成的rank变化
  }
  if (UUU > BBBBBBBBB)
    lnodevn = !lnodevn;

  edgev[!edgevn].clear();
  for (ull I = 0; I < edgev[edgevn].size(); I++) {
    ull i = edgev[edgevn][I];
    if (!edge[i].active)
      continue;
    if (UUU > BBBBBBBBB)
      edgev[!edgevn].push_back(i); //清理inactive的边
    lnode[storeid[edge[i].to]].add += add[storeid[edge[i].from]]; //将local node造成的rank变化作用到点划分后区域中相关的点（通过边）
  }

  if (UUU > BBBBBBBBB)
    edgevn = !edgevn;

  count = 0;
  ull iii = 0;
  GlobalAddress addr;
  memset(info_num, 0, sizeof(info_num));
  addr.nodeID = nodeID;
  lnodeon[!lnodeonv].clear();

  for (ull i = 0; i < lnodeon[lnodeonv].size(); i++) {
    if (!lnode[storeid[lnodeon[lnodeonv][i]]].active)
      continue;
    if (UUU > BBBBBBBBB)
      lnodeon[!lnodeonv].push_back(lnodeon[lnodeonv][i]);
    ull nowi = storeid[lnodeon[lnodeonv][i]];
    tmpull[count] = lnodeon[lnodeonv][i]; //tmpull: DSM buffer，存local node 的 global id
    count++;

    info_num[lnode[nowi].mainID]++;
  }

  if (UUU > BBBBBBBBB)
    lnodeonv = !lnodeonv;

  addr.addr = (24 * nodeNR);
  AWMW(addr, count * 8, (uint8_t *)tmpull);  //把tmpull存入本设备在gmem中的对应空间
  ull tmp = 0;
  for (int i = 0; i < nodeNR; i++) {

    addr.addr = (i)*24;
    write64(addr, count); //把tmpull的长度存入本设备在gmem中的对应空间（用于告诉别人本设备上的local node的数量）

    addr.addr += 8;
    write64(addr, tmp);

    addr.addr += 8;
    write64(addr, info_num[i]);
    tmp += info_num[i];
  }

  for (ull i = 0; i < lnodeon[lnodeonv].size(); i++) {
    if (!lnode[storeid[lnodeon[lnodeonv][i]]].active)
      continue;
    ull nowi = storeid[lnodeon[lnodeonv][i]];
    tmpdouble[iii] = lnode[nowi].add;  //tmpdouble：DSM buffer，存local node的更新后的rank值
    tmpbool[iii] = 1;  //tmpbool：DSM buffer，表示active可能
    iii++;
  }

  addr.addr = (24 * nodeNR) + count * 8ULL;
  AWMW(addr, count * 8, (uint8_t *)tmpdouble);
  addr.addr = (24 * nodeNR) + count * 24ULL;
  AWMW(addr, count, (uint8_t *)tmpbool);
  return;
}

void pagerank_apply() {

  GlobalAddress addr;
  ull infoall[MAXCORE + 1];
  ull infob[MAXCORE + 1];
  ull infonum[MAXCORE + 1];

  for (int i = 0; i < nodeNR; i++) {

    addr.nodeID = i;
    addr.addr = 24 * (nodeID);
    infoall[i] = read64(addr); //对应count
    addr.addr += 8;
    infob[i] = read64(addr); //对应tmp
    addr.addr += 8;
    infonum[i] = read64(addr); //对应info_num

    addr.addr = (24 * nodeNR) + infob[i] * 8;
    AWMR(addr, infonum[i] * 8, (uint8_t *)gatherid[i]); //对应tmpull（每个设备被划分到的node的global id）
    addr.addr = (24 * nodeNR) + infob[i] * 8 + infoall[i] * 8;
    AWMR(addr, infonum[i] * 8, (uint8_t *)gatheradd);  //对应tmpdouble（每个设备被划分到的node的仅区域内更新后的权重）
    for (ull j = 0; j < infonum[i]; j++) {
      gatherid[i][j] = gatherid[i][j] - (nodeID)*defnode_Num; //把前面收到的node的global id转化成本地id
      node[gatherid[i][j]].rank += gatheradd[j] * 0.85;
    }
  }

  nodev[!nodevn].clear();
  for (ull I = 0; I < nodev[nodevn].size(); I++) {
    ull i = nodev[nodevn][I];
    if (!nodeactive[i]) {
      continue;
    }
    if (UUU > BBBBBBBBB)
      nodev[!nodevn].push_back(i);
    node[i].rank += 0.15;
    if (fabs(node[i].rank - nodeoldrank[i]) < TOLERANCE) { // nodeoldrank是用来存本地管理的global node的聚合后rank结果的本地数组
      nodeactive[i] = 0;
      active_N--;
    }
    nodeoldrank[i] = node[i].rank;
    node[i].rank = 0;
  }

  if (UUU > BBBBBBBBB)
    nodevn = !nodevn;

  for (int i = 0; i < nodeNR; i++) {

    addr.nodeID = i;
    for (ull j = 0; j < infonum[i]; j++) {
      tmpbool[j] = nodeactive[gatherid[i][j]];
      tmpdouble[j] = nodeoldrank[gatherid[i][j]];
    }

    addr.addr = (24ULL * nodeNR) + (infob[i]) * 8ULL + (infoall[i] * 16);
    AWMW(addr, infonum[i] * 8, (uint8_t *)tmpdouble);  // 把每个设备的local node的更新后的rank写入各设备对应的gmem
    addr.addr = (24ULL * nodeNR) + (infoall[i] * 8ULL * 3) + infob[i];
    AWMW(addr, infonum[i], (uint8_t *)tmpbool);
  }
  return;
}

void pagerank_scatter() {
  GlobalAddress addr;
  addr.nodeID = nodeID;
  addr.addr = (24 * nodeNR) + (count * 16);
  AWMR(addr, count * 8, (uint8_t *)tmpdouble);
  addr.addr = (24 * nodeNR) + (count * 24);
  AWMR(addr, count, (uint8_t *)tmpbool);
  ull Dsmid = 0;

  for (ull ij = 0; ij < lnodeon[lnodeonv].size(); ij++) {
    if (!lnode[storeid[lnodeon[lnodeonv][ij]]].active)
      continue;
    ull nowi = storeid[lnodeon[lnodeonv][ij]];
    lnode[nowi].oldrank = lnode[nowi].rank;
    lnode[nowi].active = tmpbool[Dsmid];
    lnode[nowi].rank = tmpdouble[Dsmid];
    Dsmid++;

    if (lnode[nowi].active == 0) { //某个节点失效后，这个点相连的所有边都会失效
      for (long long j = lnode[nowi].nextf; j != -1; j = edge[j].nextf) {
        edge[j].active = 0;
      }
      for (long long j = lnode[nowi].nextt; j != -1; j = edge[j].nextt) {
        edge[j].active = 0;
      }
    }
  }
  return;
}

void initlnode(ull n) {
  if (storeid[n] == -1) {
    lnode[lnode_Num].mainID = check(n);
    lnode[lnode_Num].id = n;
    lnode[lnode_Num].nextf = -1;
    lnode[lnode_Num].nextt = -1;
    lnode[lnode_Num].active = 1;
    lnode[lnode_Num].rank = 1.0;
    lnode[lnode_Num].add = 0.0;
    lnode[lnode_Num].oldrank = 0;
    lnodev[0].push_back(lnode_Num);
    storeid[n] = lnode_Num++;
    lnodeon[0].push_back(n);
  }
}

void addedge(ull f, ull t) {
  initlnode(f);
  initlnode(t);
  edgev[0].push_back(totedge);
  edge[totedge].from = f;
  edge[totedge].to = t;
  edge[totedge].nextf = lnode[storeid[f]].nextf;
  edge[totedge].active = 1;
  lnode[storeid[f]].nextf = totedge;
  edge[totedge].nextt = lnode[storeid[t]].nextt;
  lnode[storeid[t]].nextt = totedge++;
}

void build_graph() {
  freopen(graph_dir, "r", stdin);
  ull nodef, nodet;
  std::memset(storeid, -1, BIGNODENUM * sizeof(long long));
  lnode_Num = 0;
  std::memset(outnum, 0, BIGNODENUM * sizeof(ull));
  totedge = 0;
  active_N = 0;
  lnodevn = nodevn = edgevn = lnodeonv = 0;
  lnodev[0].clear();
  nodev[0].clear();
  edgev[0].clear();
  lnodeon[0].clear();

  // get the nodeNum and edgeNum
  mynode_Num = defnode_Num = node_Num / nodeNR;
  myedge_Num = defedge_Num = edge_Num / nodeNR;
  if (nodeID == nodeNR - 1) {
    mynode_Num = node_Num - (nodeNR - 1) * defnode_Num;
    myedge_Num = edge_Num - (nodeNR - 1) * defedge_Num;
  }

  // init the main node in my core
  for (ull i = 0; i < mynode_Num; i++) { // 这是一堆由该设备负责聚合的一部分全局node（区分于local node）
    node[i].id = i + nodeID * defnode_Num;
    nodeactive[i] = 1;
    node[i].rank = 0;
    nodeoldrank[i] = 1;
    nodev[0].push_back(i);
  }

  // init the edge 
  // 点划分：https://www.cnblogs.com/orion-orion/p/16340839.html
  for (int i = 0; i < nodeNR; i++) {
    ull J = defedge_Num; // the edgeNum of core i
    if (i == nodeNR - 1)
      J = edge_Num - (nodeNR - 1) * defedge_Num;
    for (ull j = 0; j < J; j++) {
      std::cin >> nodef >> nodet;
      outnum[nodef]++;
      if (i == nodeID) {
        addedge(nodef, nodet);
      }
    }
  }
  std::sort(lnodeon[0].begin(), lnodeon[0].end());
}

void display() {
  std::cout << "NodeID" << nodeID << "\n";
  std::cout << " node_Num : " << node_Num << " edge_Num : " << edge_Num
            << " mynode_Num: " << mynode_Num << " myedge_Num: " << myedge_Num
            << " lnode_Num : " << lnode_Num << std::endl;
  for (size_t i = 0; i < myedge_Num; i++) {
    std::cout << "nodeID: " << nodeID << " " << edge[i].from << " "
              << edge[i].to << " \t";
  }
  std::cout << std::endl;
}
void display2() {
  std::cout << "NodeID :" << nodeID << "    di  ";
  for (size_t i = 0; i < mynode_Num; i++) {
  }
}

int main(int argc, char **argv) {

  init_memory();
  std::ios::sync_with_stdio(false);
  parserArgs(argc, argv);

  timespec s, e;
  clock_gettime(CLOCK_REALTIME, &s);

  UUU = 0;
  DSMConfig conf(CacheConfig(), nodeNR);
  dsm = DSM::getInstance(conf);
  nodeID = dsm->getMyNodeID();

  dsm->registerThread();
#pragma omp parallel for num_threads(4)
  for (int i = 0; i < 4; ++i) {
    dsm->registerThread();
  }

  double tottime = 0.0;
  // build the graph
  std::cout << "[INFO]NODEID<<" << nodeID << ">> load from " << graph_dir
            << std::endl;
  build_graph();
  std::cout << "[INFO]NODEID<<" << nodeID
            << ">> Load Graph Over RUN BEGIN mynode[" << mynode_Num
            << "] myedge_Num[" << myedge_Num << "]" << std::endl;
  dsm->barrier("PAGE");

  clock_gettime(CLOCK_REALTIME, &e);
  if (nodeID == 0)
    std::cout
        << "-----------------------[INFO]--------------------LOAD GRAPH TIME   "
        << (e.tv_nsec - s.tv_nsec) * 1.0 / S2N + (e.tv_sec - s.tv_sec) << "\n";

  active_N = mynode_Num;

  int iii = 0;
  while (iii++ < 56) {

    UUU++;
    clock_gettime(CLOCK_REALTIME, &s);
    std::cout << "------[INFO]NODEID <<" << nodeID << ">> in iteration [" << iii
              << "] and the active_N is [" << active_N << "]" << std::endl;

    pagerank_gather();
    dsm->barrier(std::string("a") + std::to_string(iii));

    pagerank_apply();
    dsm->barrier(std::string("v") + std::to_string(iii));

    pagerank_scatter();
    dsm->barrier(std::string("c") + std::to_string(iii));

    clock_gettime(CLOCK_REALTIME, &e);

    if (nodeID == 0) {
      std::cout
          << "-----------------------[INFO]--------------------ITERATION ["
          << iii << "]   "
          << (e.tv_nsec - s.tv_nsec) * 1.0 / S2N + e.tv_sec - s.tv_sec << "\n";
      tottime += (e.tv_nsec - s.tv_nsec) * 1.0 / S2N + e.tv_sec - s.tv_sec;
    }
  }

  Debug::notifyError("OK FINE");
  std::cout << "NODEID:" << nodeID << " Active :" << active_N << std::endl;

  if (nodeID == 0) {
    Debug::notifyError("----------------total time");
    std::cout << tottime << std::endl;
  }

  return 0;
}
