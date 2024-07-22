#
bf_switchd指令什么时候用？


# 系统版本以及库需求
交换机：
  tofino SDE版本: bf-sde-8.2.0 (bf-sde-8.9.1实测也行)
服务器：
  Ubuntu 22.04.4 LTS
  ofed版本：MLNX_OFED_LINUX-4.7-3.2.9.0 (MLNX_OFED_LINUX-23.10-2.1.3.1实测也行)
  libmemcached：1.0.18
  memcached：1.6.14
  Thrift：0.10.0

# 系统相关的配置文件

ccDSM/host：ip、mac、p4 switch对应的port

# p4src中是p4代码
编译: ./build.sh 时间比较长
运行: sudo -E ./run.sh

运行并enable port： sudo -E ./auto_run.sh
auto_run.sh中[ $up_ports == "4" ] ；其中4是等待需要up的端口数目

注意：

1. 若出现下图generate_tofino_pd报错，可能是SDE版本不对，没能在指定位置找到相应文件
![no module error](images/generate_tofino_pd.png)

2. 若出现下图bf_switchd报错，可能是系统路径不对，需要export LD_LIBRARY_PATH=$SDE_INSTALL/lib
![shared file error](images/generate_tofino_pd.png)

# cd ccDSM/build; cmake ..; make -j;
编译C++代码 时间比较长

# 每台服务器执行ccDSM/arp.sh (需要sudo)
设置mtu为4200, 并加载arp条目

r1上执行arp-r1.sh
r2-r3上执行arp-r2-3.sh
因为不同服务器网卡name不同

# 设置大页
在build目录下：sudo bash ../hugepage.sh

#  运行
服务器export NIC_NAME=XXXX (例如enp65s0np0)

第一步：每次运行都需要初始化p4 switch

第二步：在r1上执行./restartMemc.sh，初始化memcached用于qp交换机信息

第三步：
sudo -E ./benchmark XXXX (需要sudo是因为使用了raw packet，需要root权限)

例子：
在r1上运行sudo  -E  ./benchmark 2 4 50 50 50；
在r2上运行sudo  -E  ./benchmark 2 4 50 50 50；

注意：
1）如果发现了“XXXX: Connection timed out failed to modify QP state to RTS”
一般是arp缓存有问题, 执行arp-*.sh，使用arp -a查看

2) 在DSMKepper.cpp里进行了远程ssh的执行:
  if (this->getMyNodeID() == 0) {
    system("ssh wq@192.168.189.34 /home/wq/nfs/ccDSM/p4src/table.py");
    system("ssh wq@192.168.189.34 /home/wq/nfs/ccDSM/p4src/mc.py");
  }
需要考虑sudo -E导致的ssh问题

# 自动化运行
查看script/benchmark.sh
还没仔细看，应该需要看看权限问题，使用mpi跑