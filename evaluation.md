###个人理解
1. 在Concordia中，BLOCK_SIZE是单机共享空间大小，当本次操作要访问共享空间时，appt会随机对一个节点上的共享空间发起操作

2. #define DSM_CACHE_INDEX_WIDTH (18) 对应着8GBcache空间

3. const uint64_t BLOCK_SIZE = uint64_t(MB * 1024); 对应着1机可以开出1GB共享空间，8机一共可以提供8GB共享空间

4. UNSHARED_BLOCK_SIZE = BLOCK_SIZE / (threadNR * nodeNR); 对应着1机可以开出 UNSHARED_BLOCK_SIZE * threadNR * nodeNR = 1GB 非共享空间
    
5. if(offset == UNSHARED_BLOCK_SIZE) offset = 0; 每个线程只会在 node_num * UNSHARED_BLOCK_SIZE 空间中进行非共享空间的访问

7. uint64_t sys_total_size = 16; 表示单个节点的sb分配器能够支配的大页内存空间有16GB

8. DSM_CACHE_INDEX_SIZE = (long)BLOCK_SIZE * nodeNR * 0.5 / (DSM_CACHE_LINE_SIZE * CACHE_WAYS); 表示cache set的数量随block size变动，使得总的cache大小总是等于共享gmem空间（BLOCK_SIZE * nodeNR）的一半

6. (1)所有线程都绑在numa0上跑了很久都没有跑出来,明天试一下appt绑numa1,syst绑numa0,论文里可以说,FPGA是对syst功能的加速,因此我们尽可能的提高软件baseline的syst的性能,以避免不公平的比较,具体来说我们把syst绑在离网卡更近的numa0上,并且syst跟appt分布在不同numa上(已完成); (2)打印信息中增加当次测试参数记录; (3)检查为什么打印出来的cache size=0(已修复); (4)appt需要调成24再测(已加入脚本); (5)为什么没有打印收到的包的分布?因为给dir和cache agent加了条件限制,只有目标链路上的pkt才会收集;没有统计req(已为所有包增加统计); (6)为什么无论什么节点类型都有两个QTST(等待队列)的统计信息?因为一个用于统计dir线程,一个用于统计cache agent线程;clean_WITHOUT_CC竟然是0吗,不太可能啊 (已修复,在 start_record_with_memaccess_type 就给 memaccess_type 初始化成默认值 MEMACCESS_TYPE::WITHOUT_CC)

7. Concordia没有自带对延迟的统计,需要去clean_DONT_DISTINGUISH里面找

8. 给 set_evict_flag 加上了地址校验,只会搜集目标链路信息; 给 evictLine 函数增加了一个  addr_in_waiting 形参,用于将等待驱逐的地址传进来(正确的情况下after process数量跟wake数量是一致的)

9. 

