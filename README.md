concordia仓库原名ccDSM！

需要把代码中的wq改成当前用户名（e.g., zxy），然后把"/zxy/nfs/ccDSM/"改成当前工程所在目录（e.g., "/zxy/nfs/DSM_prj/concordia/ccDSM/"）

# 系统版本以及库需求
交换机：

  tofino SDE版本: bf-sde-8.2.0 (bf-sde-8.9.1实测也行)

  python-memcached：1.59

服务器：

  Ubuntu 22.04.4 LTS

  ofed版本：MLNX_OFED_LINUX-4.7-3.2.9.0 (MLNX_OFED_LINUX-23.10-2.1.3.1实测也行)

  libmemcached：1.0.18    安装指令：apt install memcached libmemcached-tools -y
  
  memcached：1.6.14       安装指令：（上一个安装指令一下装俩）
  
  Thrift：0.10.0

  如果要使用agent_stat的文件打印功能，需要执行 sudo apt-get install libboost-all-dev

# 系统相关的配置文件

1. ccDSM/host：ip、mac、p4 switch对应的port

2. NR_DIRECTORY 与 DIR_ID_MASK， NR_CACHE_AGENT 与 AGENT_ID_MASK 需要有对应关系，例如：

  NR_DIRECTORY：4

  NR_CACHE_AGENT：4

  #define DIR_ID_MASK 0x3 (注，=0b11)

  #define AGENT_ID_MASK 0x3 (注，=0b11)

3. 如果需要目录下放，需要打开 ENABLE_SWITCH_CC 宏

4. 如果需要使用highpara_benchmark.py脚本，需要配置sudo指令免密执行，完善py内的一些初始化参数和对应arp文件

# 编译运行交换机代码
进入./p4src文件夹

在当前用户根目录下（e.g., /home/zxy）运行 ln -s /root/Software/bf-sde-8.9.1 bf-sde-8.9.1

检查环境变量如下：

```
# SDE
export SDE=/home/zxy/bf-sde-8.9.1
export SDE_INSTALL=$SDE/install
export PATH=$PATH:$SDE_INSTALL/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$SDE_INSTALL/lib
```

编译: ./build.sh 时间比较长

运行: sudo -E ./run.sh

运行并enable port： sudo -E ./auto_run.sh

注意auto_run.sh中[ $up_ports == "4" ] ；其中4是等待需要up的端口数目

报错信息：

1. 若出现下图generate_tofino_pd报错，可能是SDE版本不对，没能在指定位置找到相应文件
![no module error](images/generate_tofino_pd.png)

2. 若出现下图bf_switchd报错，可能是系统路径不对，需要export LD_LIBRARY_PATH=$SDE_INSTALL/lib
![shared file error](images/generate_tofino_pd.png)


# 编译服务器代码
cd ccDSM/build; cmake ..; make -j;

编译C++代码，时间比较长

# 配置服务器网络（每台）
设置mtu为4200：sudo ifconfig enp65s0np0 mtu 4200 (已经写在arp-xxx.sh中了)

P.s., r1-r4网卡分别是 enp65s0np0, enp28s0np0, enp28s0np0, enp62s0np0

在build目录下, 加载arp条目 (需要sudo)：r1上执行sudo bash ../arp-r1.sh，r2-r3上执行sudo bash ../arp-r2-3.sh，r4上执行sudo bash ../arp-r4.sh，因为不同服务器网卡name不同

# 设置服务器大页（每台）
在build目录下：sudo bash ../hugepage.sh

# 运行服务器代码（每台）
服务器export NIC_NAME=XXXX (例如enp65s0np0)

第零步：每次运行都需要初始化p4 switch（运行sudo -E ./auto_run.sh进行初始化，进程在后台，可以通过直接运行指令bfshell查看进程是否跑着）

第一步：检查arp缓存，使用arp -a查看（里边内容需要是百G网卡的而不是普通网卡的），执行arp-*.sh解决

第二步：在r1上，在build目录下, 执行 bash ../script/restartMemc.sh，初始化memcached用于qp交换机信息 （注意，要在确保没有arp缓存问题的情况下运行）

第三步：sudo -E ./benchmark XXXX (需要sudo是因为使用了raw packet，需要root权限)

*2机*例子，原版代码：

在r1上运行sudo  -E  ./benchmark 4 4 50 0 100；

在r2上运行sudo  -E  ./benchmark 4 4 50 0 100；

*2机*例子，添加agent_stat，未开启目标链路测试：

在r1上运行sudo  -E  ./highpara_benchmark --no_node 2 --no_thread 1 --remote_ratio 50 --shared_ratio 50 --read_ratio 50 --is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 0 --result_dir /home/zxy/concordia_result

在r2上运行sudo  -E  ./highpara_benchmark --no_node 2 --no_thread 1 --remote_ratio 50 --shared_ratio 50 --read_ratio 50 --is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 0 --result_dir /home/zxy/concordia_result

*3机*例子，添加agent_stat，开启目标链路测试：

在r1上运行sudo  -E  ./highpara_benchmark --no_node 4 --no_thread 4 --locality 0 --shared_ratio 100 --read_ratio 50 --is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 0 --home_node_id 2 --result_dir /home/zxy/concordia_result_2

在r2上运行sudo  -E  ./highpara_benchmark --no_node 4 --no_thread 4 --locality 0 --shared_ratio 100 --read_ratio 50 --is_cache 0 --cache_rw 0 --is_request 1 --request_rw 1 --is_home 0 --home_node_id 2 --result_dir /home/zxy/concordia_result_2

在r3上运行sudo  -E  ./highpara_benchmark --no_node 4 --no_thread 4 --locality 0 --shared_ratio 100 --read_ratio 50 --is_cache 0 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 1 --home_node_id 2 --result_dir /home/zxy/concordia_result_2

在r4上运行sudo  -E  ./highpara_benchmark --no_node 4 --no_thread 4 --locality 0 --shared_ratio 100 --read_ratio 50 --is_cache 1 --cache_rw 0 --is_request 0 --request_rw 0 --is_home 0 --home_node_id 2 --result_dir /home/zxy/concordia_result_2


注意：

1. 如果发现了“XXXX: Connection timed out failed to modify QP state to RTS”

一般是arp缓存有问题，执行arp-*.sh，使用arp -a查看

如果在运行highpara_benchmark.py过程中遇到该问题，记得运行kill_all.py

2. 在DSMKepper.cpp里进行了远程ssh的执行:

```c
  if (this->getMyNodeID() == 0) {
    system("ssh wq@192.168.189.34 /home/wq/nfs/ccDSM/p4src/table.py");
    system("ssh wq@192.168.189.34 /home/wq/nfs/ccDSM/p4src/mc.py");
  }
```

需要考虑sudo -E导致的ssh问题（首次建立需要ssh yes一下，不然就算把公钥放进交换机，程序也无法正确执行）

#  自动化运行
查看script/benchmark.sh，还没仔细看，应该需要看看权限问题，使用mpi跑

# 参数配置 (benchmark, highpara_benchmark)
1. sys_total_size: 所有节点总共内存空间大小（所有空间其实都是可共享的）[初始值:32GB] [当前值:16GB(开太大会导致mmap-failed？冷重启)]
2. BLOCK_SIZE: 每个节点上所有App线程会访问的总工作集空间[初始值:31MB(注:GAM的工作集为204800*512B=104MB)] [可能值:128MB]
3. SUB_PAGE_SIZE: 用户内存操作地址按该值对齐[初始值:512B]
4. STEPS: 单App线程用户发起内存操作次数[初始值:BLOCK_SIZE*nodeNR/SUB_PAGE_SIZE=253,952(4机)]
5. DSM_CACHE_LINE_WIDTH: 缓存行大小（2的幂次）[初始值:12(4KB)]
6. DSM_CACHE_INDEX_WIDTH: 缓存每个组中缓存行数目（2的幂次）[初始值:16(65536个)]
7. CACHE_WAYS：组相联缓存的行数[初始值:8]
8. DSM_CACHE_INDEX_SIZE * CACHE_WAYS: 缓存大小[初始值:2GB(注：使用的空间不与sys_total_size重合)]
9. locality: 本地内存操作有多大概率与上一次访问的地址是连续的[初始值:0]
10. sharing: 所有内存操作中，针对不共享的空间（注意：系统支持共享，但应用避开了对这部分空间的共享访问）进行访问的内存操作占比[初始值:50]
11. readNR: 所有内存操作中，读操作占比[初始值:50]



# 测试记录

1. 4机，4 App thread，2 Sys thread，0 Queue thread，关闭交换机下放：1217804 total-ops

2. 4机，4 App thread，8 Sys thread，0 Queue thread，关闭交换机下放：1249507(1254000) total-ops

2. 4机，16 App thread，8 Sys thread，0 Queue thread，关闭交换机下放：(测试时间很久,20分钟起步)

3. 4机，4 App thread，8 Sys thread，1 Queue thread(目录)，关闭交换机下放：190858(统计有memfence，有锁)，1223500（统计无memfence，有锁），卡住（统计无memfence，无锁）

3. 4机，4 App thread，8 Sys thread，2 Queue thread(目录与缓存)，关闭交换机下放：229498（统计无memfence，有锁）

3. 4机，4 App thread，8 Sys thread，1 Queue thread(缓存)，关闭交换机下放：218049（统计无memfence，有锁）

3. 4机，4 App thread，8 Sys thread，2 Queue thread(目录与缓存)，关闭交换机下放，ibv_poll轮询线程（2 queue-thread 与 6 sys-thread）有绑核，其他未绑核：1197398（统计无memfence，有锁）

以上，验证统计功能无误且不影响性能
------------------------


