#include <DSM.h>
#include <bits/stdc++.h>
// #include <gperftools/profiler.h>
#define S2N (1e9)
#include <mpi.h>

#define MAX_THREAD 24

#define LNODESIZE (1 + 8 + 8)
#define MB (1024 * 1024)
#define TOLERANCE (1e-2)
typedef uint64_t ull;
std::thread th[MAX_THREAD];
DSM *dsm;

char *graph_dir;
int nodeNR;
int nodeID;
ull node_Num, edge_Num, mynode_Num, myedge_Num, defnode_Num, defedge_Num,
    lnode_Num, onode_Num;
ull totedge;

struct Edge {
  ull from;
  ull to;
  long long nextf;
  long long nextt;
} edge[30 * MB];

ull activeE[30 * MB];
struct Node {
  ull id;
  double rank;
  bool active;
  std::vector<std::pair<int, ull>> Addr;
} node[6 * MB];
struct LocalNode {
  double rank;
  ull id;
  long long nextf;
  long long nextt;
} lnode[6 * MB];
ull outnum[6 * MB];
ull activeN[6 * MB];
long long storeid[6 * MB];
void parserArgs(int argc, char **argv) {
  graph_dir = argv[1];
  nodeNR = std::atoi(argv[2]);
}

inline uint64_t read64(GlobalAddress addr) {
  uint64_t ans = 0;
  // std::cout << "node "<<nodeID <<" want to read:"<<(int)addr.nodeID+1 <<" +
  // "<< addr.addr <<std::endl;
  // if ((int)addr.nodeID > 4 )
  // std::cout << "node "<<nodeID <<" want to write:"<<(int)addr.nodeID+1 <<" +
  // "<< addr.addr << " shit " <<ans<< std::endl;
  dsm->read(addr, 8, (uint8_t *)&ans);
  return ans;
}
inline void write64(GlobalAddress addr, uint64_t ans) {
  // if ((int)addr.nodeID > 4 )
  // std::cout << "node "<<nodeID <<" want to write:"<<(int)addr.nodeID+1 <<" +
  // "<< addr.addr << " shit " <<ans<< std::endl;
  dsm->write(addr, 8, (uint8_t *)&ans);
  return;
}
inline double readdouble(int nodeid, uint64_t addressP) {
  GlobalAddress addr;
  addr.nodeID = nodeid - 1;
  addr.addr = addressP;
  return (read64(addr) * 1.0 / 1e10);
}
inline void writedouble(uint8_t nodeid, uint64_t addressP, double x) {
  ull shit = (ull)(x * 1e10);
  GlobalAddress addr;
  addr.nodeID = nodeid - 1;
  addr.addr = addressP;
  // std::cout <<"nodeid ?????"<<shit <<  std::endl;
  write64(addr, shit);
  return;
} /*
 inline void writeactive(int nodeid,ull addressP,bool ans){
     GlobalAddress addr;
     addr.nodeID = nodeid-1;
     addr.addr = addressP;
     dsm->write(addr,1,(uint8_t *)(&ans));
     return;
 }
 inline bool readactive(int nodeid,ull addressP){
     GlobalAddress addr;
     addr.nodeID = nodeid -1;
     addr.addr = addressP;
     bool ans;
     dsm->read(addr,1,(uint8_t *) (&ans));
     return ans;
 }*/
uint64_t active_N;
bool lnode_act[6 * MB];
double add[6 * MB];
int iii = 0;
void pagerank_gather() {
  for (ull i = 0; i < lnode_Num; i++) {
    // BETTER : for list of the edges active
    // localnode[i.to].add += i.from.rank / i.from.toN;
    // read rank
    // if (!lnode_act[i]) continue;
    // lnode_act[i] = readactive(nodeID,i*LNODESIZE+16);
    // if (!lnode_act[i]) continue;
    double rank = readdouble(nodeID, (i)*LNODESIZE + 8);
    if (std::fabs(rank - lnode[i].rank) < TOLERANCE && iii > 1)
      continue;
    for (int j = lnode[i].nextt; (j = edge[j].nextt); j != -1) {
      add[storeid[edge[j].to]] += (rank - node[i].rank) / outnum[edge[j].from];
    }
  }
  for (ull i = 0; i < lnode_Num; i++) {
    // write add
    // if (!lnode_act[i]) continue;
    writedouble(nodeID, i * LNODESIZE, add[i]);
  }
  // gather
  //
  return;
}
void pagerank_apply() {
  // same as gather
  // apply
  Debug::notifyError("test ");
  for (ull i = 0; i < mynode_Num; i++) {
    if (node[i].active == 0)
      continue;
    // std::cout <<"nodeId " <<nodeID <<" i " << i <<"  node " << node[i].id <<
    // std::endl;
    ull Fnode = node[i].Addr.size();
    double newadd = 0.0;
    for (ull j = 0; j < Fnode; j++) {
      // readadd !!!!!!!!!
      newadd += readdouble((nodeID + 1) % nodeNR + 1, node[i].Addr[j].second);
    }
    // std::cout <<nodeID<<" test point1" << std::endl;
    double newrank = newadd * 0.85 + 0.15;
    // std::cout <<" fabs " <<std::fabs(newrank - node[i].rank ) << " <? "<<
    // TOLERANCE <<"res" <<(std::fabs(newrank - node[i].rank ) < TOLERANCE )
    // <<std::endl;
    if (std::fabs(newrank - node[i].rank) < TOLERANCE) {
      node[i].active = 0;
      active_N--;
      // for (int j= 0 ; j < Fnode ; j++){
      //    // write unactive !!!!!!!!!!!!!
      //    writeactive(node[i].Addr[j].first,node[i].Addr[j].second+16,0);
      //}
    } else {
      // write rank !!!!!!!!!!!!!!!
      for (ull j = 0; j < Fnode; j++) {
        writedouble((nodeID + 1) % nodeNR + 1, node[i].Addr[j].second + 8,
                    newrank);
      }
    }
    node[i].rank = newrank;
    // std::cout << nodeID <<" test point2"<<std::endl;
  }
  return;
}
void pagerank_scatter() {
  // for (int i = 0 ; i < lnode_Num; i++){
  // if Lnode[i].active = 0 update the edge list
  // update the Lnode list
  // read unactive
  //}
  return;
}
// std::vector<ull> lnodesort;
void addedge(ull f, ull t) {
  if (storeid[f] == -1) {
    lnode[lnode_Num].id = f;
    lnode[lnode_Num].nextf = -1;
    lnode[lnode_Num].nextt = -1;
    lnode[lnode_Num].rank = 1.0;
    storeid[f] = lnode_Num++;
    // std::cout <<nodeID <<" oooooooooooooooooooooooooooooooooooooooooo"<<
    // std::endl;
    writedouble(nodeID, storeid[f] * LNODESIZE + 8, 1.0);
    // writeactive(nodeID,storeid[f]*LNODESIZE+16,1);
    lnode_act[storeid[f]] = 1;
    // lnodesort.push_back(f);
    // std::cout << nodeID <<" okllllllllllllllllllllllllllllllllllllllll" <<
    // std::endl;
  }
  if (storeid[t] == -1) {
    lnode[lnode_Num].id = t;
    lnode[lnode_Num].nextf = -1;
    lnode[lnode_Num].nextt = -1;
    lnode[lnode_Num].rank = 1.0;
    storeid[t] = lnode_Num++;
    writedouble(nodeID, storeid[t] * LNODESIZE + 8, 1.0);
    // writeactive(nodeID,storeid[t]*LNODESIZE+16,1);
    lnode_act[storeid[t]] = 1;
    // lnodesort.push_back(t);
  }
  edge[totedge].from = f;
  edge[totedge].to = t;
  edge[totedge].nextf = lnode[storeid[f]].nextf;
  lnode[storeid[f]].nextf = totedge;
  edge[totedge].nextt = lnode[storeid[t]].nextt;
  lnode[storeid[t]].nextt = totedge++;
}
int Set[5 * MB];
void gao(int i, ull n) {
  ull id = n % ((nodeNR - 1) * defnode_Num);
  if (Set[n] == 0) {
    Set[n] = 1;
    ull addr = (onode_Num++) * LNODESIZE;
    node[id].Addr.push_back(std::make_pair(i, addr));
  }
}
bool check(ull n) {
  if (nodeID == nodeNR) {
    if (n >= (nodeNR - 1) * defnode_Num) {
      return true;
    }

  } else if (n >= (nodeID - 1) * defnode_Num && n < nodeID * defnode_Num) {
    return true;
  }
  return false;
}

void build_graph() {
  freopen(graph_dir, "r", stdin);
  ull nodef, nodet;
  std::memset(storeid, -1, sizeof(storeid));
  lnode_Num = 0;
  std::memset(outnum, 0, sizeof(outnum));
  totedge = 0;
  active_N = 0;
  std::cin >> node_Num;
  std::cin >> edge_Num;
  // get the nodeNum and edgeNum
  mynode_Num = defnode_Num = node_Num / nodeNR;
  myedge_Num = defedge_Num = edge_Num / nodeNR;
  if (nodeID == nodeNR) {
    mynode_Num = node_Num - (nodeNR - 1) * defnode_Num;
    myedge_Num = edge_Num - (nodeNR - 1) * defedge_Num;
  }
  // init the main node in my core
  for (ull i = 0; i < mynode_Num; i++) {
    node[i].id = i + nodeID * defnode_Num;
    node[i].active = 1;
    node[i].rank = 1.0;
    node[i].Addr.clear();
  }
  // read the edge
  // lnodesort.clear();
  for (int i = 1; i <= nodeNR; i++) {
    // divided the edge to core
    // virtual storeid in core i , to get the global addr of (copy node addr)
    memset(Set, 0, sizeof(Set));
    onode_Num = 0;
    ull J = defedge_Num; // the edgeNum of core i
    if (i == nodeNR)
      J = edge_Num - (nodeNR - 1) * defedge_Num;
    for (ull j = 0; j < J; j++) {
      std::cin >> nodef >> nodet;
      // std::cout << nodeID <<":::::::" << nodef <<" " << nodet << std::endl;
      outnum[nodef]++;
      if (i == nodeID) {
        // store the edge in my core, edge and lnode
        addedge(nodef, nodet);
      }
      // store the main node's (copy node addr) if the nodef/t is my main
      // node
      if (check(nodef))
        gao(i, nodef);
      if (check(nodet))
        gao(i, nodet);
    }
  }
}
void display() {
  std::cout << "NodeID" << nodeID << "\n";
  std::cout << " node_Num : " << node_Num << " edge_Num : " << edge_Num
            << " mynode_Num: " << mynode_Num << " myedge_Num: " << myedge_Num
            << " lnode_Num : " << lnode_Num << std::endl;
}
void display2() {
  std::cout << "NodeID :" << nodeID << "    di  ";
  for (size_t i = 0; i < mynode_Num; i++) {
    std::cout << i << " dian " << node[i].id << " rankkkkkkkkkk" << node[i].rank
              << std::endl;
  }
}
int main(int argc, char **argv) {
  parserArgs(argc, argv);
  MPI_Init(&argc, &argv);
  timespec s, e, S, E;
  clock_gettime(CLOCK_REALTIME, &s);
  clock_gettime(CLOCK_REALTIME, &S);
  DSMConfig conf(CacheConfig(), nodeNR, 1);
  dsm = DSM::getInstance(conf);
  nodeID = dsm->getMyNodeID();
  nodeID++;
  dsm->registerThread();
  // build the graph
  std::cout << "[INFO]NODEID<<" << nodeID << ">> load from " << graph_dir
            << std::endl;
  build_graph();
  // run
  std::cout << "[INFO]NODEID<<" << nodeID
            << ">> Load Graph Over RUN BEGIN mynode[" << mynode_Num
            << "] myedge_Num[" << myedge_Num << "]" << std::endl;

  // MPI_Barrier(MPI_COMM_WORLD);
  dsm->barrier("page-init");

  clock_gettime(CLOCK_REALTIME, &e);
  if (nodeID == 1)
    std::cout
        << "-----------------------[INFO]--------------------LOAD GRAPH TIME   "
        << (e.tv_nsec - s.tv_nsec) * 1.0 / S2N + (e.tv_sec - s.tv_sec) << "\n";
  active_N = mynode_Num;
  while (iii <= 50) {
    iii++;
    clock_gettime(CLOCK_REALTIME, &s);
    std::cout << "[INFO]NODEID <<" << nodeID << ">> in iteration [" << iii
              << "] and the active_N is [" << active_N << "]" << std::endl;
    // begin time
    pagerank_gather();
    // tongbu yi xia
    // std::cout <<"nodeID "<< nodeID <<"in rout " <<iii <<" gather gather
    // gather  "<< std::endl;
    // MPI_Barrier(MPI_COMM_WORLD);
    dsm->barrier(std::string("g") + std::to_string(iii));

    pagerank_apply();

    dsm->barrier(std::string("a") + std::to_string(iii));
    // display2();
    // tong bu yi xia
    // zhao yi ge  jie dian tong ji xin xi
    // end_time
    // active_N = 0;
    // MPI_Barrier(MPI_COMM_WORLD);
    std::cout << "[INFO]NODEID <<" << nodeID << ">> in iteration [" << iii
              << "] down " << std::endl;
    clock_gettime(CLOCK_REALTIME, &e);
    if (nodeID == 1) {
      std::cout
          << "-----------------------[INFO]--------------------ITERATION ["
          << iii << "]   "
          << (e.tv_nsec - s.tv_nsec) * 1.0 / S2N + e.tv_sec - s.tv_sec << "\n";
    }
  }
  Debug::notifyError("OKFINE");
  std::cout << "NODEID:" << nodeID << " Active :" << active_N << std::endl;
  MPI_Finalize();
  clock_gettime(CLOCK_REALTIME, &E);
  std::cout << "--------------TOTEL TIME  "
            << (E.tv_nsec - S.tv_nsec) * 1.0 / S2N + E.tv_sec - S.tv_sec
            << std::endl;
  return 0;
}
