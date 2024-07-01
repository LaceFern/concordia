#include <DSM.h>
#include <bits/stdc++.h>
// #include <gperftools/profiler.h>
#define S2N (1e9)
#include <mpi.h>
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
ull node_Num, edge_Num, mynode_Num, myedge_Num, defnode_Num, defedge_Num,
    lnode_Num, onode_Num;
// addedge in my core
long long totedge;
// the edge in my core
struct Edge {
  ull from;
  ull to;
  bool active;
  long long nextf;
  long long nextt;
};
Edge *edge = new struct Edge[100 * MB];
struct Node {
  ull id;
  double rank;
} node[5 * MB];
double nodeoldrank[5 * MB];
bool nodeactive[5 * MB];
struct LocalNode {
  int mainID;
  ull id;
  bool active;
  double rank;
  double oldrank;
  double add;
  long long nextf;
  long long nextt;
} lnode[5 * MB];
ull outnum[5 * MB];
long long storeid[5 * MB];
ull gatherid[MAXCORE][5 * MB];
std::vector<ull> lnodeon[2];
int lnodeonv;
void collect() {
  // bian li mei ge thread shu chu duo shao ge jieshu de jiedian
}
void parserArgs(int argc, char **argv) {
  graph_dir = argv[1];
  nodeNR = std::atoi(argv[2]);
}
inline uint64_t read64(GlobalAddress addr) {
  uint64_t ans = 0;
  // if (addr.nodeID >= (uint8_t)nodeNR)
  // std::cout<<nodeID<<"------------ want read " <<(int)addr.nodeID <<"+"
  // <<addr.addr<< std::endl;
  dsm->read(addr, 8, (uint8_t *)&ans);
  return ans;
}
inline void write64(GlobalAddress addr, uint64_t ans) {
  // if (addr.nodeID >= (uint8_t)nodeNR)
  // std::cout<<nodeID<<"------------ want write " <<(int)addr.nodeID <<"+"
  // <<addr.addr<< std::endl;

  dsm->write(addr, 8, (uint8_t *)&ans);
  // uint64_t tmp = read64(addr);
  // if (addr.addr <= 0 ){
  //    Debug::notifyError("11111111111111111111111111");
  //    std::cout <<(int)addr.nodeID<<"+"<<addr.addr <<" de "<< ans <<"  and  "
  //    << tmp << std::endl;

  //}
  return;
}
double gatheradd[5 * MB];
inline void AWMR(GlobalAddress addr, ull count_n, uint8_t *addrB) {
  ull j = 0;
  for (j = 0; j + (READSIZE) < count_n; j += READSIZE) {
    dsm->read(addr, READSIZE, (uint8_t *)&addrB[j]);
    addr.addr += READSIZE;
  }
  dsm->read(addr, (count_n - j), (uint8_t *)&addrB[j]);
}

inline void AWMW(GlobalAddress addr, ull count_n, uint8_t *addrB) {
  ull j = 0;
  for (j = 0; j + (READSIZE) < count_n; j += READSIZE) {
    dsm->write(addr, READSIZE, (uint8_t *)&addrB[j]);
    addr.addr += READSIZE;
  }
  dsm->write(addr, (count_n - j), (uint8_t *)&addrB[j]);
}

inline void shitgatheradd(int i, ull count_a, ull count_b, ull count_n) {
  GlobalAddress addr;
  addr.nodeID = i;
  addr.addr = (24 * nodeNR) + count_b * 8 + count_a * 8;
  ull j = 0;
  for (j = 0; j + (READSIZE / 8) < count_n; j += (READSIZE / 8)) {
    dsm->read(addr, READSIZE, (uint8_t *)&gatheradd[j]);
    addr.addr += READSIZE;
  }
  dsm->read(addr, (count_n - j) * 8, (uint8_t *)&gatheradd[j]);
}
inline void shitgatherid(int i, ull count_b, ull count_n) {
  GlobalAddress addr;
  addr.nodeID = i;
  addr.addr = (24 * nodeNR) + count_b * 8;
  ull j = 0;
  for (j = 0; j + (READSIZE / 8) < count_n; j += (READSIZE / 8)) {
    dsm->read(addr, READSIZE, (uint8_t *)&gatherid[i][j]);
    addr.addr += READSIZE;
  }
  dsm->read(addr, (count_n - j) * 8, (uint8_t *)&gatherid[i][j]);

  // for (int j = 0; j < count_n ; j++){
  //    gatherid[i][j] = read64(addr);
  //    addr.addr+=8;
  //}

  // the id of array
  // gatherid[i][j] =  gatherid[i][j] - (nodeID) * defnode_Num;
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
  // if (addressP <= 88) {
  //    Debug::notifyError(">>>>>>>>>>>");
  //}
  // std::cout <<(int)nodeid <<"+" <<addressP<< " writedouble" << shit
  // <<std::endl;
  dsm->write(addr, 8, (uint8_t *)&x);
  return;
}

inline void writeactive(int nodeid, ull addressP, bool ans) {
  GlobalAddress addr;
  addr.nodeID = nodeid;
  addr.addr = addressP;
  // if (nodeid >= nodeNR)
  // std::cout<<nodeID<<"------------ want write " <<nodeid <<"+"<<addressP<<
  // std::endl;
  dsm->write(addr, 1, (uint8_t *)(&ans));
  return;
}

inline bool readactive(int nodeid, ull addressP) {
  GlobalAddress addr;
  addr.nodeID = nodeid;
  addr.addr = addressP;
  bool ans;
  // if (nodeid >= nodeNR)
  // std::cout<<nodeID<<"------------ want read " <<nodeid <<"+"<<addressP<<
  // std::endl;
  dsm->read(addr, 1, (uint8_t *)(&ans));
  return ans;
}
uint64_t active_N;
double add[6 * MB];
inline int check(ull n) {
  if (n >= (nodeNR - 1) * defnode_Num) {
    return nodeNR - 1;
  }
  return (n / defnode_Num);
}
double tmpdouble[5 * MB];
bool tmpbool[5 * MB];
ull tmpull[5 * MB];
ull info_num[MAXCORE + 2];
ull count = 0;
std::vector<ull> edgev[2], lnodev[2], nodev[2];
int edgevn, lnodevn, nodevn;

void pagerank_gather() {

  lnodev[!lnodevn].clear();
  for (ull I = 0; I < lnodev[lnodevn].size(); I++) {
    ull i = lnodev[lnodevn][I];
    if (!lnode[i].active) {
      // Debug::notifyError("cnm");
      // exit(0);
      continue;
    }
    if (UUU > BBBBBBBBB)
      lnodev[!lnodevn].push_back(i);
    add[i] = (-lnode[i].oldrank + lnode[i].rank) / outnum[lnode[i].id];
    // std::cout << "nodeID :" << nodeID << " lnodeid "<<lnode[i].id << " addi
    // "<<add[i]  <<" outdu "<< outnum[lnode[i].id ]<<"\n";
  }
  if (UUU > BBBBBBBBB)
    lnodevn = !lnodevn;
  // Debug::notifyError("eat?");

  edgev[!edgevn].clear();
  for (ull I = 0; I < edgev[edgevn].size(); I++) {
    ull i = edgev[edgevn][I];
    if (!edge[i].active)
      continue;
    if (UUU > BBBBBBBBB)
      edgev[!edgevn].push_back(i);
    lnode[storeid[edge[i].to]].add += add[storeid[edge[i].from]];
  }
  if (UUU > BBBBBBBBB)
    edgevn = !edgevn;

  // Debug::notifyError("eat shit");
  count = 0;
  ull iii = 0;
  // Debug::notifyError("test1");
  // num1 ---- numNR
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

    // addr.addr = ( 24 * nodeNR ) + ( count * 8 );
    // std::cout << "nodeID " <<nodeID <<" want write "  << addr.addr <<" the
    // IDID " << lnode[nowi].id <<"\n"; write64(addr,lnode[nowi].id);
    tmpull[count] = lnodeon[lnodeonv][i];
    count++;

    info_num[lnode[nowi].mainID]++;
  }
  if (UUU > BBBBBBBBB)
    lnodeonv = !lnodeonv;
  addr.addr = (24 * nodeNR);
  AWMW(addr, count * 8, (uint8_t *)tmpull);

  // Debug::notifyError("test2");
  ull tmp = 0;
  for (int i = 0; i < nodeNR; i++) {

    addr.addr = (i)*24;
    write64(addr, count);

    addr.addr += 8;
    write64(addr, tmp);

    addr.addr += 8;
    write64(addr, info_num[i]);
    // std::cout <<"node write " << nodeID <<" i " << i <<" [address] " <<
    // (int)addr.nodeID<< "+" <<addr.addr<<" count " << count << " tmp "  << tmp
    // <<" num " << info_num[i] <<"\n";
    tmp += info_num[i];
  }

  for (ull i = 0; i < lnodeon[lnodeonv].size(); i++) {
    if (!lnode[storeid[lnodeon[lnodeonv][i]]].active)
      continue;
    ull nowi = storeid[lnodeon[lnodeonv][i]];

    // ull addaddr = ( 24ULL * nodeNR ) + ( count * 8ULL ) + (iii * 8ULL);

    // lnode[nowi].rankaddr =  ( 24ULL * nodeNR ) + ( count * 16ULL ) + (iii *
    // 8ULL); lnode[nowi].activeaddr = ( 24ULL * nodeNR ) + ( count * 24ULL ) +
    // (iii); addr.addr = 0;

    tmpdouble[iii] = lnode[nowi].add;
    // writedouble(nodeID,addaddr,lnode[nowi].add);

    // std::cout <<"nodeID "<< nodeID <<" write adddddddddddddddddd " << addaddr
    // << " add " << lnode[nowi].add  <<" is for node " <<lnode[nowi].id << "\n";
    //

    tmpbool[iii] = 1;
    // writeactive(nodeID,lnode[nowi].activeaddr,1);

    /*if (read64(addr) != count ){
        Debug::notifyError("nimasssssssssssssssssssssssssssssssssssssss");
        std::cout << (int)addr.nodeID <<"+" << addr.addr <<" is  "
    <<read64(addr) <<"   "<<  nodeID <<"+" << addaddr <<" zjbns "<<
    lnode[nowi].add << std::endl; exit(0);
    }*/
    iii++;
  }
  addr.addr = (24 * nodeNR) + count * 8ULL;
  AWMW(addr, count * 8, (uint8_t *)tmpdouble);
  addr.addr = (24 * nodeNR) + count * 24ULL;
  AWMW(addr, count, (uint8_t *)tmpbool);
  return;
}

void pagerank_apply() {
  // Debug::notifyError("test4");

  GlobalAddress addr;
  ull infoall[MAXCORE + 1];
  ull infob[MAXCORE + 1];
  ull infonum[MAXCORE + 1];

  for (int i = 0; i < nodeNR; i++) {

    addr.nodeID = i;
    addr.addr = 24 * (nodeID);
    infoall[i] = read64(addr);
    addr.addr += 8;
    infob[i] = read64(addr);
    addr.addr += 8;
    infonum[i] = read64(addr);

    // std::cout <<"node "<< nodeID <<"  i  "<< i  << "[address] "
    // <<(int)addr.nodeID << "+"<<addr.addr << " all  " << infoall[i] <<" begin
    // "<< infob[i] <<" num " << infonum[i] << "\n";
    addr.addr = (24 * nodeNR) + infob[i] * 8;
    AWMR(addr, infonum[i] * 8, (uint8_t *)gatherid[i]);
    addr.addr = (24 * nodeNR) + infob[i] * 8 + infoall[i] * 8;
    // shitgatherid(i,infob[i],infonum[i]);
    // shitgatheradd(i,infoall[i],infob[i],infonum[i]);
    AWMR(addr, infonum[i] * 8, (uint8_t *)gatheradd);
    for (ull j = 0; j < infonum[i]; j++) {
      // addr.addr = ( 24 * nodeNR ) + (j+infob[i]) * 8;
      // gatherid[i][j] = read64(addr);

      // addr.addr += infoall[i] * 8;
      // double gatheradd = readdouble(i,addr.addr);

      // the id of array
      gatherid[i][j] = gatherid[i][j] - (nodeID)*defnode_Num;

      node[gatherid[i][j]].rank += gatheradd[j] * 0.85;
      // node[gatherid].write[i] = 1;
      // node[gatherid].rankaddr[i] = addr.addr + (infoall[i] * 8);
      // node[gatherid].activeaddr[i] = (24 * nodeNR ) + (infoall[i] * 8 * 3) +
      // infob[i] + j; std::cout <<"node "<< nodeID <<" dui i fu shu jie dian  "
      // << i << " gatherlocal " <<gatherid << " gatherid  " << gatherid +
      // (nodeID) * defnode_Num <<" gatheradd " << gatheradd << "  after add rank
      // :=  " <<node[gatherid].rank <<" rankaddr " <<   addr.addr + (infoall[i]
      // * 8) <<" activeaddr " <<    (24 * nodeNR ) + (infoall[i] * 8 * 3) +
      // infob[i] + j << "\n";
    }
  }
  //
  // Debug::notifyError("test1");
  /*
  for (int i = 0; i < mynode_Num ; i++){
      if (!node[i].active) {
          //Debug::notifyError("fuck???");
          continue;
      }

      node[i].rank += 0.15;
      if (fabs(node[i].rank - node[i].oldrank) < TOLERANCE ) {
          node[i].active = 0;
          active_N--;
          for (int j = 0; j<nodeNR ; j++){
              if ( node[i].write[j] ){
                  writeactive(j,node[i].activeaddr[j],0);
              }
          }
      }
      else {
          for (int j = 0; j<nodeNR ; j++){
              if ( node[i].write[j] ){
                  writedouble(j,node[i].rankaddr[j],node[i].rank);
              }
          }
      }
      node[i].oldrank = node[i].rank;
      node[i].rank = 0;
      for (int j = 0; j < nodeNR ; j++){
          node[i].write[j] = 0;
      }
  }
  */
  nodev[!nodevn].clear();
  for (ull I = 0; I < nodev[nodevn].size(); I++) {
    ull i = nodev[nodevn][I];
    if (!nodeactive[i]) {
      // Debug::notifyError("fuck???");
      continue;
    }
    if (UUU > BBBBBBBBB)
      nodev[!nodevn].push_back(i);
    node[i].rank += 0.15;
    if (fabs(node[i].rank - nodeoldrank[i]) < TOLERANCE) {
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
    // std::cout <<"node "<< nodeID <<"  i  "<< i  << "[address] "
    // <<(int)addr.nodeID << "+"<<addr.addr << " all  " << infoall[i] <<" begin
    // "<< infob[i] <<" num " << infonum[i] << "\n";

    addr.addr = (24ULL * nodeNR) + (infob[i]) * 8ULL + (infoall[i] * 16);
    AWMW(addr, infonum[i] * 8, (uint8_t *)tmpdouble);
    addr.addr = (24ULL * nodeNR) + (infoall[i] * 8ULL * 3) + infob[i];
    AWMW(addr, infonum[i], (uint8_t *)tmpbool);

    /*
    for (int j = 0 ; j < infonum[i] ; j++ ){
        addr.addr = ( 24 * nodeNR ) + (j+infob[i]) * 8;
        if (!nodeactive[gatherid[i][j]])
            writeactive(i,(24ULL * nodeNR ) + (infoall[i] * 8ULL * 3) + infob[i]
    + j,nodeactive[gatherid[i][j]]); else writedouble( i,addr.addr + (infoall[i]
    * 16ULL ),nodeoldrank[gatherid[i][j]]);
    }
    */
  }
  // Debug::notifyError("test apply");
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
    /*
    lnode[nowi].active = readactive(nodeID,lnode[nowi].activeaddr);
    if (lnode[nowi].active == 1){
        lnode[nowi].rank = readdouble(nodeID,lnode[nowi].rankaddr);
    }
    */
    lnode[nowi].active = tmpbool[Dsmid];
    lnode[nowi].rank = tmpdouble[Dsmid];
    Dsmid++;
    // Debug::notifyError("RW OK");

    if (lnode[nowi].active == 0) {
      for (long long j = lnode[nowi].nextf; j != -1; j = edge[j].nextf) {
        edge[j].active = 0;
      }
      for (long long j = lnode[nowi].nextt; j != -1; j = edge[j].nextt) {
        edge[j].active = 0;
      }
    }
  }
  // Debug::notifyError("testscatter");
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
int Set[5 * MB];

void build_graph() {
  freopen(graph_dir, "r", stdin);
  ull nodef, nodet;
  std::memset(storeid, -1, sizeof(storeid));
  lnode_Num = 0;
  std::memset(outnum, 0, sizeof(outnum));
  totedge = 0;
  active_N = 0;
  lnodevn = nodevn = edgevn = lnodeonv = 0;
  lnodev[0].clear();
  nodev[0].clear();
  edgev[0].clear();
  lnodeon[0].clear();

  std::cin >> node_Num;
  std::cin >> edge_Num;

  // get the nodeNum and edgeNum
  mynode_Num = defnode_Num = node_Num / nodeNR;
  myedge_Num = defedge_Num = edge_Num / nodeNR;
  if (nodeID == nodeNR - 1) {
    mynode_Num = node_Num - (nodeNR - 1) * defnode_Num;
    myedge_Num = edge_Num - (nodeNR - 1) * defedge_Num;
  }

  // init the main node in my core
  for (ull i = 0; i < mynode_Num; i++) {
    node[i].id = i + nodeID * defnode_Num;
    nodeactive[i] = 1;
    node[i].rank = 0;
    nodeoldrank[i] = 1;
    nodev[0].push_back(i);
    // for (int j = 0 ; j<nodeNR ; j++){
    //     node[i].write[j] = 0;
    // }
  }

  // init the edge
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
    // std::cout << i <<" dian "<< node[i].id <<" rankkkkkkkkkk" <<
    // node[i].oldrank << std::endl;
  }
}
int main(int argc, char **argv) {
  std::ios::sync_with_stdio(false);
  parserArgs(argc, argv);
  // MPI_Init(&argc,&argv);

  timespec s, e;
  clock_gettime(CLOCK_REALTIME, &s);
  // clock_gettime(CLOCK_REALTIME,&S);
  UUU = 0;
  DSMConfig conf(CacheConfig(), nodeNR, 4);
  dsm = DSM::getInstance(conf);
  nodeID = dsm->getMyNodeID();
  dsm->registerThread();
  double tottime = 0.0;
  // build the graph
  std::cout << "[INFO]NODEID<<" << nodeID << ">> load from " << graph_dir
            << std::endl;
  build_graph();
  std::cout << "[INFO]NODEID<<" << nodeID
            << ">> Load Graph Over RUN BEGIN mynode[" << mynode_Num
            << "] myedge_Num[" << myedge_Num << "]" << std::endl;
  // MPI_Barrier(MPI_COMM_WORLD);
  dsm->barrier("PAGE");

  clock_gettime(CLOCK_REALTIME, &e);
  if (nodeID == 0)
    std::cout
        << "-----------------------[INFO]--------------------LOAD GRAPH TIME   "
        << (e.tv_nsec - s.tv_nsec) * 1.0 / S2N + (e.tv_sec - s.tv_sec) << "\n";

  int iii = 0;
  active_N = mynode_Num;

  while (iii < 51) {

    iii++;
    UUU++;
    clock_gettime(CLOCK_REALTIME, &s);
    std::cout << "------[INFO]NODEID <<" << nodeID << ">> in iteration [" << iii
              << "] and the active_N is [" << active_N << "]" << std::endl;

    pagerank_gather();
    dsm->barrier(std::string("a") + std::to_string(iii));
    // MPI_Barrier(MPI_COMM_WORLD);
    // Debug::notifyError("gather ok");
    pagerank_apply();
    // MPI_Barrier(MPI_COMM_WORLD);
    //
    dsm->barrier(std::string("v") + std::to_string(iii));
    // Debug::notifyError("apply ok");
    pagerank_scatter();
    // MPI_Barrier(MPI_COMM_WORLD);
    //
    dsm->barrier(std::string("c") + std::to_string(iii));

    // std::cout <<"------[INFO]NODEID <<"<< nodeID <<">> in iteration [" <<iii
    // <<"] down "<< std::endl; display2();

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
  // MPI_Finalize();

  // clock_gettime(CLOCK_REALTIME,&E);
  if (nodeID == 0) {
    Debug::notifyError("----------------tot time");
    std::cout << tottime << std::endl;
  }
  // std::cout<<"--------------TOTAL TIME  "<<(E.tv_nsec-S.tv_nsec) * 1.0 / S2N
  // + E.tv_sec - S.tv_sec<< std::endl;

  return 0;
}
