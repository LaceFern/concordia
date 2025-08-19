#pragma once

#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <atomic>
#include <infiniband/verbs.h>
// #include <experimental/filesystem> 
#include <boost/filesystem.hpp> 
#include <fstream>

#include <cstdio>
#include <regex>

#include <mutex>
#include <iostream>
#include <string>
#include <iomanip>

#include "histogram.h"
#include "atomic_queue/atomic_queue.h"
#include "numautil.h"
#include <queue>
#include <mutex>
#include <condition_variable>

#include "Common.h"

#define MAX_WORKER_PENDING_MSG 1024
#define MAX_SYS_THREAD 12
#define MAX_GLB_THREAD 48
#define GLB_INVALID 0xff

namespace fs = boost::filesystem;

// using GAddr = uint64_t;

// class queue_entry {
// public:
//     struct ibv_wc wc;
//     uint64_t starting_point; // created by rdtsc();
//     uint64_t dummy;
// };

struct queue_entry {
    struct ibv_wc wc;
    uint64_t starting_point; // created by rdtsc();
    uint64_t dummy;
};

/**************************/
/*****SAFE QUEUE START*****/
template <typename T>
class SafeQueue {
public:
    void enqueue(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(value);
        cond_var_.notify_one();
    }

    T dequeue() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this]() { return !queue_.empty(); });
        T value = queue_.front();
        queue_.pop();
        return value;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_var_;
};
/*****SAFE QUEUE END*******/
/**************************/

using SPSC_QUEUE = atomic_queue::AtomicQueueB2<queue_entry, std::allocator<queue_entry>, false, false, false>;
// using SPSC_QUEUE = atomic_queue::AtomicQueueB2<queue_entry, std::allocator<queue_entry>, true, false, true>;

enum class MEMACCESS_TYPE {
    WITH_CC,
    WITHOUT_CC,
    DONT_DISTINGUISH,
    _count,
};

enum class APP_THREAD_OP {
    NONE,
    AFTER_PROCESS_LOCAL_REQUEST_LOCK,
    AFTER_PROCESS_LOCAL_REQUEST_UNLOCK,
    AFTER_PROCESS_LOCAL_REQUEST_READ,
    AFTER_PROCESS_LOCAL_REQUEST_READP2P,
    AFTER_PROCESS_LOCAL_REQUEST_WRITE,
    AFTER_PROCESS_LOCAL_REQUEST_OTHER,
    WAIT_ASYNC_FINISH_LOCK,
    WAIT_ASYNC_FINISH_UNLOCK,
    WAIT_ASYNC_FINISH,
    WAKEUP_2_LOCK_RETURN,
    WAKEUP_2_UNLOCK_RETURN,
    WAKEUP_2_READ_RETURN,
    WAKEUP_2_WRITE_RETURN,
    MEMSET,
    _count,
};

enum class MULTI_APP_THREAD_OP {
    NONE,
    READ,
    WRITE,
    RLOCK,
    RUNLOCK,
    WLOCK,
    WUNLOCK,
    MALLOC,
    _count,
};

enum class SYS_THREAD_OP {
    NONE,
    _count,
};

enum class MULTI_SYS_THREAD_OP {
    NONE,
    PROCESS_IN_HOME_NODE, // home node receive request node, and forward to cache node
    PROCESS_PENDING_IN_HOME, // home node and request node receive rdma_write_with_imm response from cache node
    PROCESS_IN_CACHE_NODE, // process in cache node
    PROCESS_NOT_TARGET,
    _count,
};

enum class MULTI_POLL_THREAD_OP {
    NONE,
    WAITING_IN_SYSTHREAD_QUEUE,
    WAITING_NOT_TARGET,
    _count,
};


extern thread_local std::thread::id now_thread_id;

class agent_stats {
private:
    // only collect mem access stat from 1 special app thread 
    uint64_t memaccess_counter;
    MEMACCESS_TYPE memaccess_type;
    std::unordered_map<MEMACCESS_TYPE, Histogram *> memaccess_type_stats;

    // only collect 1 special app thread stat
    Histogram *app_thread_stats;
    volatile uint64_t app_thread_counter;
    std::unordered_map<APP_THREAD_OP, Histogram *> app_thread_op_stats;

    // maybe more than 1 sys thread, need use atomic op
    // I don't know why we need sys_thread_stats and multi_sys_thread_stats now(2024/3/5)
    Histogram *sys_thread_stats;
    volatile uint64_t sys_thread_counter;
    std::unordered_map<SYS_THREAD_OP, Histogram *> sys_thread_op_stats;

    // used for multi sys thread situation
    Histogram *multi_sys_thread_stats[MAX_SYS_THREAD];
    uint64_t multi_sys_thread_counter[MAX_SYS_THREAD];
    std::unordered_map<MULTI_SYS_THREAD_OP, Histogram *> multi_sys_thread_op_stats[MAX_SYS_THREAD];


    // only 1 sys thread to poll rdma CQ
    Histogram *multi_poll_thread_stats[MAX_SYS_THREAD];
    std::unordered_map<MULTI_POLL_THREAD_OP, Histogram *> multi_poll_thread_op_stats[MAX_SYS_THREAD];


    // std::unordered_set <GAddr> valid_gaddrs;

    std::atomic<int> start;

    volatile int thread_init[48] = {0};
    std::mutex init_mtx;
    std::mutex app_mtx;
    std::mutex sys_mtx[MAX_SYS_THREAD];

    int home_process_count[MAX_SYS_THREAD] = {0};

    // only sys threads recv packets
    int home_recv_count[MAX_SYS_THREAD] = {0};
    int request_recv_count[MAX_SYS_THREAD] = {0};
    int cache_recv_count[MAX_SYS_THREAD] = {0};

    // // both app threads and sys threads send packets, app thread id follows sys thread id
    // int home_send_count[MAX_GLB_THREAD] = {0};
    // int request_send_count[MAX_GLB_THREAD] = {0};
    // int cache_send_count[MAX_GLB_THREAD] = {0};

    std::mutex access_size_mtx_;
    std::unordered_map<size_t, uint64_t> write_size_stats_;
    std::unordered_map<size_t, uint64_t> read_size_stats_;

public:

    int control_packet_send_count[MAX_APP_THREAD + MAX_SYS_THREAD] = {0};
    int data_packet_send_count[MAX_APP_THREAD + MAX_SYS_THREAD] = {0};
    std::atomic<int> cachehit{0};
    std::unordered_map<MULTI_APP_THREAD_OP, Histogram *> multi_app_thread_op_stats[MAX_APP_THREAD];
    

    uint64_t rdtsc() {
      unsigned int lo, hi;
      __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
      return ((uint64_t)hi << 32) | lo;
    }

    uint64_t rdtscp() {
      unsigned int lo, hi;
      __asm__ __volatile__("rdtscp" : "=a" (lo), "=d" (hi));
      return ((uint64_t)hi << 32) | lo;
    }

    // inline void update_home_send_count(uint64_t thread_id) {
    //     if(start && thread_id != GLB_INVALID) home_send_count[thread_id]++;
    // }
    // inline void update_request_send_count(uint64_t thread_id) {
    //     if(start && thread_id != GLB_INVALID) request_send_count[thread_id]++;
    // }
    // inline void update_cache_send_count(uint64_t thread_id) {
    //     if(start && thread_id != GLB_INVALID) cache_send_count[thread_id]++;
    // }

    inline void update_home_recv_count(uint64_t thread_id) {
        // if(start && thread_id != GLB_INVALID) home_recv_count[thread_id]++;
        home_recv_count[thread_id]++;
    }
    inline void update_request_recv_count(uint64_t thread_id) {
        // if(start && thread_id != GLB_INVALID) request_recv_count[thread_id]++;
        request_recv_count[thread_id]++;
    }
    inline void update_cache_recv_count(uint64_t thread_id) {
        // if(start && thread_id != GLB_INVALID) cache_recv_count[thread_id]++;
        cache_recv_count[thread_id]++;
    }

    inline void update_home_process_count(uint64_t thread_id) {
        // if(start && thread_id != GLB_INVALID) home_process_count[thread_id]++;
        home_process_count[thread_id]++;
    }

    int get_home_recv_count(uint64_t thread_id) {
        return home_recv_count[thread_id];
    }
    int get_home_process_count(uint64_t thread_id) {
        return home_process_count[thread_id];
    }

    void set1_thread_init_flag(int id){
        std::lock_guard<std::mutex> lock(init_mtx);
        thread_init[id] = 1;
    }
    int read_thread_init_flag(int id){
        std::lock_guard<std::mutex> lock(init_mtx);
        return thread_init[id];
    }

    int debug_info = 0;
    int debug_info_1 = 0;
    int debug_info_2 = 0;
    int debug_info_3 = 0;
    int debug_info_4 = 0;
    int debug_info_5 = 0;
    int debug_info_6 = 0;


    int is_request = 0;
    int is_cache = 0;
    int is_home = 0;

    int home_node_id = 0;

    SafeQueue<queue_entry> * safe_queues[MAX_SYS_THREAD];
    SPSC_QUEUE * queues[MAX_SYS_THREAD];
    int dir_queue_num = 1;
    int cache_queue_num = 1;
    // COMMENT: there is no need to enable queue for request node, 
    // because each app thread only process the response of the queries it send,
    // which is far less than packets that need process in cache node and home node.
    // Besides, waiting time in request node can't be reduced by increasing sys threads
    double microseconds = 0;
    int nr_app = 0;
    int nr_dir = 0;
    int nr_cache_agent = 0;
    uint64_t sys_thread_num = 0;
    uint64_t sys_with_queue_num = dir_queue_num + cache_queue_num;
    uint64_t lcores_num_per_numa = 12;
    explicit agent_stats() {
        // TODO
        start_collection();
        for(int j = 0; j < static_cast<int>(MEMACCESS_TYPE::_count); j++){
            memaccess_type_stats[(MEMACCESS_TYPE) j] = new Histogram(1, 10000000, 3, 10);
        }

        app_thread_stats = new Histogram(1, 10000000, 3, 10);
        for(int j = 0; j < static_cast<int>(APP_THREAD_OP::_count); j++){
            app_thread_op_stats[(APP_THREAD_OP) j] = new Histogram(1, 10000000, 3, 10);
        }

        for(int i = 0; i < MAX_APP_THREAD; i++){
            for(int j = 0; j < static_cast<int>(MULTI_APP_THREAD_OP::_count); j++){
                multi_app_thread_op_stats[i][(MULTI_APP_THREAD_OP) j] = new Histogram(1, 10000000, 3, 10);
            }
        }

        sys_thread_stats = new Histogram(1, 10000000, 3, 10);
        for (int i = 0; i < MAX_SYS_THREAD; i++) {
            multi_sys_thread_stats[i] = new Histogram(1, 10000000, 3, 10);
            
            for(int j = 0; j < static_cast<int>(MULTI_SYS_THREAD_OP::_count); j++){
                multi_sys_thread_op_stats[i][(MULTI_SYS_THREAD_OP) j] = new Histogram(1, 10000000, 3, 10);
            }            
        }

        for (int i = 0; i < MAX_SYS_THREAD; i++) {
            multi_poll_thread_stats[i] = new Histogram(1, 10000000, 3, 10);
            for(int j = 0; j < static_cast<int>(MULTI_POLL_THREAD_OP::_count); j++){
                multi_poll_thread_op_stats[i][(MULTI_POLL_THREAD_OP) j] = new Histogram(1, 10000000, 3, 10);
            }
        }

        std::vector<size_t>numa_node_list = get_lcores_for_numa_node(0);
        lcores_num_per_numa = numa_node_list.size();
        std::cout << "NUMA 0 have " << lcores_num_per_numa << " lcores" << std::endl;
    }

    ~agent_stats() {
        // TODO
        // print_access_size_stats();
        // print_all_false_count(stdout,48);
        // std::cout << "cache hit" << cachehit.load() << std::endl;
        // print_app_thread_stat();
        // print_multi_app_thread_op_stats();
    }

    void print_multi_app_thread_op_stats() {
        for(int i = 0; i < MAX_APP_THREAD; i++){
            std::cout << "app thread " << i << " multi app thread op stats: " << std::endl;
            for(int j = 0; j < static_cast<int>(MULTI_APP_THREAD_OP::_count); j++){
                multi_app_thread_op_stats[i][(MULTI_APP_THREAD_OP) j]->print(stdout, 5);
            }
        }
    }

    void print_app_thread_stat() {
        std::cout << "\nAFTER_PROCESS_LOCAL_REQUEST_LOCK: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_LOCK]->print(stdout, 5);
        std::cout << "\nAFTER_PROCESS_LOCAL_REQUEST_UNLOCK: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_UNLOCK]->print(stdout, 5);
        std::cout << "\nAFTER_PROCESS_LOCAL_REQUEST_READ: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_READ]->print(stdout, 5);
        // std::cout << "\nAFTER_PROCESS_LOCAL_REQUEST_READP2P: " << std::endl;
        // app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_READP2P]->print(stdout, 5);
        std::cout << "\nAFTER_PROCESS_LOCAL_REQUEST_WRITE: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_WRITE]->print(stdout, 5);
        // std::cout << "\nAFTER_PROCESS_LOCAL_REQUEST_OTHER: " << std::endl;
        // app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_OTHER]->print(stdout, 5);

        std::cout << "\nWAIT_ASYNC_FINISH: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::WAIT_ASYNC_FINISH]->print(stdout, 5);
        std::cout << "\nWAIT_ASYNC_FINISH_LOCK: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::WAIT_ASYNC_FINISH_LOCK]->print(stdout, 5);
        std::cout << "\nWAIWAIT_ASYNC_FINISH_UNLOCK: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::WAIT_ASYNC_FINISH_UNLOCK]->print(stdout, 5);

        std::cout << "\nWAKEUP_2_READ_RETURN: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_READ_RETURN]->print(stdout, 5);
        std::cout << "\nWAKEUP_2_WRITE_RETURN: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_WRITE_RETURN]->print(stdout, 5);
        std::cout << "\nWAKEUP_2_LOCK_RETURN: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_LOCK_RETURN]->print(stdout, 5);
        // std::cout << "\nWAKEUP_2_RLOCK_RETURN: " << std::endl;
        // app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_RLOCK_RETURN]->print(stdout, 5);
        // std::cout << "\nWAKEUP_2_WLOCK_RETURN: " << std::endl;
        // app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_WLOCK_RETURN]->print(stdout, 5);
        std::cout << "\nWAKEUP_2_UNLOCK_RETURN: " << std::endl;
        app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_UNLOCK_RETURN]->print(stdout, 5);
        // std::cout << "\nMEMSET: " << std::endl;
        // app_thread_op_stats[APP_THREAD_OP::MEMSET]->print(stdout, 5);
        
        int control_packet_send_count_total = 0;
        int data_packet_send_count_total = 0;
        for(int i = 0; i < MAX_APP_THREAD + MAX_SYS_THREAD; i++){
            control_packet_send_count_total += control_packet_send_count[i];
            data_packet_send_count_total += data_packet_send_count[i];
        }
        fprintf(stdout, "packet count:\t");
        for(int i = 0; i < MAX_APP_THREAD + MAX_SYS_THREAD; i++){
            if(i < nr_app){
                fprintf(stdout, "app[%d]=(%d, %d)\t", i, control_packet_send_count[i], data_packet_send_count[i]);
            }
            else if(i >= MAX_APP_THREAD && i < MAX_APP_THREAD + sys_thread_num){
                fprintf(stdout, "sys[%d]=(%d, %d)\t", i - MAX_APP_THREAD, control_packet_send_count[i], data_packet_send_count[i]);
            }
        }
        fprintf(stdout, "total=(%d, %d)\n", control_packet_send_count_total, data_packet_send_count_total);
        
        fprintf(stdout, "net throughput (MBps):\t");
        for(int i = 0; i < MAX_APP_THREAD + MAX_SYS_THREAD; i++){
            if(i < nr_app){
                fprintf(stdout, "app[%d]=(%.2lf, %.2lf)\t", i, 1.0 * control_packet_send_count[i] * 74 / microseconds, 1.0 * data_packet_send_count[i] * DSM_CACHE_LINE_SIZE / microseconds);
            }
            else if(i >= MAX_APP_THREAD && i < MAX_APP_THREAD + sys_thread_num){
                fprintf(stdout, "sys[%d]=(%.2lf, %.2lf)\t", i - MAX_APP_THREAD, 1.0 * control_packet_send_count[i] * 74 / microseconds, 1.0 * data_packet_send_count[i] * DSM_CACHE_LINE_SIZE / microseconds);
            }
        }
        fprintf(stdout, "total=(%.2lf, %.2lf)\n", 1.0 * control_packet_send_count_total * 74 / microseconds, 1.0 * data_packet_send_count_total * DSM_CACHE_LINE_SIZE / microseconds);
    }

    void print_multi_sys_thread_stat() {
        for (size_t i = 0;i < sys_with_queue_num;i++) {
            std::cout << "\nSYS_THREAD_" << i << " PROCESS_IN_HOME_NODE: " << std::endl;
            multi_sys_thread_op_stats[i][MULTI_SYS_THREAD_OP::PROCESS_IN_HOME_NODE]->print(stdout, 5);
            std::cout << "\nSYS_THREAD_" << i << " PROCESS_PENDING_IN_HOME: " << std::endl;
            multi_sys_thread_op_stats[i][MULTI_SYS_THREAD_OP::PROCESS_PENDING_IN_HOME]->print(stdout, 5);
            std::cout << "\nSYS_THREAD_" << i << " PROCESS_IN_CACHE_NODE: " << std::endl;
            multi_sys_thread_op_stats[i][MULTI_SYS_THREAD_OP::PROCESS_IN_CACHE_NODE]->print(stdout, 5);
        }
    }

    void print_multi_poll_thread_stat() {
        for (size_t i = 0;i < sys_with_queue_num;i++) {
            std::cout << "\nPOLL_THREAD_" << i << " WAITING_IN_SYSTHREAD_QUEUE: " << std::endl;
            multi_poll_thread_op_stats[i][MULTI_POLL_THREAD_OP::WAITING_IN_SYSTHREAD_QUEUE]->print(stdout, 5);
            std::cout << "\nPOLL_THREAD_" << i << " WAITING_NOT_TARGET: " << std::endl;
            multi_poll_thread_op_stats[i][MULTI_POLL_THREAD_OP::WAITING_NOT_TARGET]->print(stdout, 5);
        }
    }


    void save_clean_stat(std::string result_dir, std::string Tag){
        std::string common_suffix = ".txt";
        double result_99 = -1;
        double result_avg = -1;
        double result_count = -1;
        
        std::ifstream file;
        std::string file_name = result_dir + "/" + Tag + common_suffix;
        file.open(file_name);
        if (!file.is_open()) {
            std::cerr << "Error: Unable to open file" << file_name << std::endl;
        }

        std::string line;
        std::getline(file, line); // Skip first line
        std::getline(file, line); // Skip second line

        while (std::getline(file, line)) {
            if (line[0] != '#') {
                std::istringstream iss(line);
                std::string word;
                std::vector<std::string> wordlist;

                while (iss >> word) {
                    wordlist.push_back(word);
                }

                double value = std::stod(wordlist[0]);
                double percentile = std::stod(wordlist[1]);

                if (percentile >= 0.99 && result_99 == -1) {
                    result_99 = value;
                }
            } else {
                std::smatch match;
                if (std::regex_search(line, match, std::regex(R"(\bMean\s*=\s*([-+]?\d*\.\d+|\d+))"))) {
                    result_avg = std::stod(match[1]);
                }
                if (std::regex_search(line, match, std::regex(R"(\bTotal count\s*=\s*([-+]?\d*\.\d+|\d+))"))) {
                    result_count = std::stod(match[1]);
                }
            }
        }
        file.close();

        int ret = remove(file_name.c_str());
        if (ret != 0) {
            std::cerr << "Error: Unable to delete file " << file_name << std::endl;
        }

        std::ofstream result;
        std::string result_name = result_dir + "/" + "clean_" + Tag + common_suffix;
        result.open(result_name, std::ios::app);
        if (!result.is_open()) {
            std::cerr << "Error: Unable to open file " << result_name << std::endl;
        }
        result << result_count << "\t" << result_avg << "\t" << result_99 << "\n";
        result.close();
    }

    void save_stat_to_file(std::string result_dir, size_t sys_threads, size_t bench_threads) {
        std::string common_suffix = ".txt";
        // std::string common_suffix = "_S" + std::to_string(sys_threads) + "_B" + std::to_string(bench_threads) + ".txt";
        if (!fs::exists(result_dir)) {
            if (!fs::create_directory(result_dir)) {
                std::cerr << "Error creating folder " << result_dir << std::endl;
                exit(1);
            }
        }
        FILE *file;
        fs::path result_directory(result_dir);
        fs::path filePath;

        filePath = result_directory / fs::path("WITH_CC" + common_suffix);
        file = fopen(filePath.c_str(), "w");
        assert(file != nullptr);
        memaccess_type_stats[MEMACCESS_TYPE::WITH_CC]->print(file, 5);
        fclose(file);

        filePath = result_directory / fs::path("WITHOUT_CC" + common_suffix);
        file = fopen(filePath.c_str(), "w");
        assert(file != nullptr);
        memaccess_type_stats[MEMACCESS_TYPE::WITHOUT_CC]->print(file, 5);
        fclose(file);

        filePath = result_directory / fs::path("DONT_DISTINGUISH" + common_suffix);
        file = fopen(filePath.c_str(), "w");
        assert(file != nullptr);
        memaccess_type_stats[MEMACCESS_TYPE::DONT_DISTINGUISH]->print(file, 5);
        fclose(file);

        save_clean_stat(result_dir, "WITH_CC");
        save_clean_stat(result_dir, "WITHOUT_CC");
        save_clean_stat(result_dir, "DONT_DISTINGUISH");


        if(is_request){
            filePath = result_directory / fs::path("AFTER_PROCESS_LOCAL_REQUEST_LOCK" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_LOCK]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "AFTER_PROCESS_LOCAL_REQUEST_LOCK");

            filePath = result_directory / fs::path("AFTER_PROCESS_LOCAL_REQUEST_UNLOCK" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_UNLOCK]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "AFTER_PROCESS_LOCAL_REQUEST_UNLOCK");

            filePath = result_directory / fs::path("AFTER_PROCESS_LOCAL_REQUEST_READ" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_READ]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "AFTER_PROCESS_LOCAL_REQUEST_READ");

            filePath = result_directory / fs::path("AFTER_PROCESS_LOCAL_REQUEST_WRITE" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::AFTER_PROCESS_LOCAL_REQUEST_WRITE]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "AFTER_PROCESS_LOCAL_REQUEST_WRITE");

            filePath = result_directory / fs::path("WAIT_ASYNC_FINISH" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::WAIT_ASYNC_FINISH]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "WAIT_ASYNC_FINISH");

            filePath = result_directory / fs::path("WAKEUP_2_READ_RETURN" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_READ_RETURN]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "WAKEUP_2_READ_RETURN");

            filePath = result_directory / fs::path("WAKEUP_2_WRITE_RETURN" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_WRITE_RETURN]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "WAKEUP_2_WRITE_RETURN");

            filePath = result_directory / fs::path("WAKEUP_2_LOCK_RETURN" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_LOCK_RETURN]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "WAKEUP_2_LOCK_RETURN");

            filePath = result_directory / fs::path("WAKEUP_2_UNLOCK_RETURN" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::WAKEUP_2_UNLOCK_RETURN]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "WAKEUP_2_UNLOCK_RETURN");

            filePath = result_directory / fs::path("MEMSET" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            app_thread_op_stats[APP_THREAD_OP::MEMSET]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "MEMSET");
        }

        for (size_t i = 0;i < sys_with_queue_num; i++) {
            if(i < dir_queue_num){

                if(is_home){
                    filePath = result_directory / fs::path("QTST_" + std::to_string(i) + "_WAITING_IN_DIR_QUEUE" + common_suffix);
                    file = fopen(filePath.c_str(), "w");
                    assert(file != nullptr);
                    multi_poll_thread_op_stats[i][MULTI_POLL_THREAD_OP::WAITING_IN_SYSTHREAD_QUEUE]->print(file, 5);
                    fclose(file);
                    save_clean_stat(result_dir, "QTST_" + std::to_string(i) + "_WAITING_IN_DIR_QUEUE");
                }

                filePath = result_directory / fs::path("QTST_" + std::to_string(i) + "_WAITING_IN_DIR_QUEUE(NOT TARGET)" + common_suffix);
                file = fopen(filePath.c_str(), "w");
                assert(file != nullptr);
                multi_poll_thread_op_stats[i][MULTI_POLL_THREAD_OP::WAITING_NOT_TARGET]->print(file, 5);
                fclose(file);
                save_clean_stat(result_dir, "QTST_" + std::to_string(i) + "_WAITING_IN_DIR_QUEUE(NOT TARGET)");
            }
            else{

                if(is_cache){
                    filePath = result_directory / fs::path("QTST_" + std::to_string(i) + "_WAITING_IN_CACHE_QUEUE" + common_suffix);
                    file = fopen(filePath.c_str(), "w");
                    assert(file != nullptr);
                    multi_poll_thread_op_stats[i][MULTI_POLL_THREAD_OP::WAITING_IN_SYSTHREAD_QUEUE]->print(file, 5);
                    fclose(file);
                    save_clean_stat(result_dir, "QTST_" + std::to_string(i) + "_WAITING_IN_CACHE_QUEUE");
                }

                filePath = result_directory / fs::path("QTST_" + std::to_string(i) + "_WAITING_IN_CACHE_QUEUE(NOT TARGET)" + common_suffix);
                file = fopen(filePath.c_str(), "w");
                assert(file != nullptr);
                multi_poll_thread_op_stats[i][MULTI_POLL_THREAD_OP::WAITING_NOT_TARGET]->print(file, 5);
                fclose(file);
                save_clean_stat(result_dir, "QTST_" + std::to_string(i) + "_WAITING_IN_CACHE_QUEUE(NOT TARGET)");
            }
        }

        filePath = result_directory / fs::path("RECV_PACKET_COUNT" + common_suffix);
        file = fopen(filePath.c_str(), "a");
        assert(file != nullptr);
        int home_recv_count_total = 0;
        int request_recv_count_total = 0;
        int cache_recv_count_total = 0;
        int recv_count_total = 0;

        sys_thread_num = nr_dir + nr_cache_agent;
        for(int i = 0; i < sys_thread_num; i++){
            home_recv_count_total += home_recv_count[i];
            request_recv_count_total += request_recv_count[i];
            cache_recv_count_total += cache_recv_count[i];
            recv_count_total += home_recv_count[i] + request_recv_count[i] + cache_recv_count[i];
            
            if(i < nr_dir){
                fprintf(file, "%d\t", home_recv_count[i]);
                printf("sysID = %d, home_recv_count = %d\t", i, home_recv_count[i]);
            }
            else{
                fprintf(file, "%d\t", cache_recv_count[i]);
                printf("sysID = %d, cache_recv_count = %d\t", i, cache_recv_count[i]);
            }
            
        }
        fprintf(file, "\t%d\t%d\t%d\t%d\t\n", home_recv_count_total, request_recv_count_total, cache_recv_count_total, recv_count_total);
        fclose(file);        


        filePath = result_directory / fs::path("SEND_PACKET_COUNT" + common_suffix);
        file = fopen(filePath.c_str(), "a");
        assert(file != nullptr);
        int control_packet_send_count_total = 0;
        int data_packet_send_count_total = 0;
        for(int i = 0; i < MAX_APP_THREAD + MAX_SYS_THREAD; i++){
            control_packet_send_count_total += control_packet_send_count[i];
            data_packet_send_count_total += data_packet_send_count[i];
        }


        fprintf(file, "\nmicroseconds:%.2lf\n", microseconds);
        fprintf(file, "MBps total=(%.2lf, %.2lf)\t", 1.0 * control_packet_send_count_total * 74 / microseconds, 1.0 * data_packet_send_count_total * DSM_CACHE_LINE_SIZE / microseconds);
        fprintf(file, "MPps total=(%.2lf, %.2lf)\n", 1.0 * control_packet_send_count_total / microseconds, 1.0 * data_packet_send_count_total / microseconds);

        // fprintf(file, "packet count:\t");
        // for(int i = 0; i < MAX_APP_THREAD + MAX_SYS_THREAD; i++){
        //     if(i < nr_app){
        //         fprintf(file, "app[%d]=(%d, %d)\t", i, control_packet_send_count[i], data_packet_send_count[i]);
        //     }
        //     else if(i >= MAX_APP_THREAD && i < MAX_APP_THREAD + sys_thread_num){
        //         fprintf(file, "sys[%d]=(%d, %d)\t", i - MAX_APP_THREAD, control_packet_send_count[i], data_packet_send_count[i]);
        //     }
        // }
        // fprintf(file, "total=(%d, %d)\n", control_packet_send_count_total, data_packet_send_count_total);
        
        // fprintf(file, "net throughput (MBps):\t");
        // for(int i = 0; i < MAX_APP_THREAD + MAX_SYS_THREAD; i++){
        //     if(i < nr_app){
        //         fprintf(file, "app[%d]=(%.2lf, %.2lf)\t", i, 1.0 * control_packet_send_count[i] * 74 / microseconds, 1.0 * data_packet_send_count[i] * DSM_CACHE_LINE_SIZE / microseconds);
        //     }
        //     else if(i >= MAX_APP_THREAD && i < MAX_APP_THREAD + sys_thread_num){
        //         fprintf(file, "sys[%d]=(%.2lf, %.2lf)\t", i - MAX_APP_THREAD, 1.0 * control_packet_send_count[i] * 74 / microseconds, 1.0 * data_packet_send_count[i] * DSM_CACHE_LINE_SIZE / microseconds);
        //     }
        // }
        // fprintf(file, "total=(%.2lf, %.2lf)\n", 1.0 * control_packet_send_count_total * 74 / microseconds, 1.0 * data_packet_send_count_total * DSM_CACHE_LINE_SIZE / microseconds);
        
        fclose(file); 
        

        
        for (size_t i = 0;i < sys_with_queue_num; i++) {
            if(is_home){
                filePath = result_directory / fs::path("QTST_" + std::to_string(i) + "_PROCESS_IN_HOME_NODE" + common_suffix);
                file = fopen(filePath.c_str(), "w");
                assert(file != nullptr);
                multi_sys_thread_op_stats[i][MULTI_SYS_THREAD_OP::PROCESS_IN_HOME_NODE]->print(file, 5);
                fclose(file);
                save_clean_stat(result_dir, "QTST_" + std::to_string(i) + "_PROCESS_IN_HOME_NODE");
            }

            if(is_request || is_home){
                filePath = result_directory / fs::path("QTST_" + std::to_string(i) + "_PROCESS_PENDING_IN_HOME" + common_suffix);
                file = fopen(filePath.c_str(), "w");
                assert(file != nullptr);
                multi_sys_thread_op_stats[i][MULTI_SYS_THREAD_OP::PROCESS_PENDING_IN_HOME]->print(file, 5);
                fclose(file);
                save_clean_stat(result_dir, "QTST_" + std::to_string(i) + "_PROCESS_PENDING_IN_HOME");
            }

            if(is_cache){
                filePath = result_directory / fs::path("QTST_" + std::to_string(i) + "_PROCESS_IN_CACHE_NODE" + common_suffix);
                file = fopen(filePath.c_str(), "w");
                assert(file != nullptr);
                multi_sys_thread_op_stats[i][MULTI_SYS_THREAD_OP::PROCESS_IN_CACHE_NODE]->print(file, 5);
                fclose(file);
                save_clean_stat(result_dir, "QTST_" + std::to_string(i) + "_PROCESS_IN_CACHE_NODE");
            }

            filePath = result_directory / fs::path("QTST_" + std::to_string(i) + "_PROCESS_NOT_TARGET" + common_suffix);
            file = fopen(filePath.c_str(), "w");
            assert(file != nullptr);
            multi_sys_thread_op_stats[i][MULTI_SYS_THREAD_OP::PROCESS_NOT_TARGET]->print(file, 5);
            fclose(file);
            save_clean_stat(result_dir, "QTST_" + std::to_string(i) + "_PROCESS_NOT_TARGET");
        }
    }

    // bool is_valid_gaddr_without_start(GAddr gaddr) {
    //     return valid_gaddrs.count(gaddr);
    // }

    // bool is_valid_gaddr(GAddr gaddr) {
    //     // if(!start){
    //     //     return false;
    //     // }
    //     // else{
    //     //     return valid_gaddrs.count(gaddr);
    //     // }
    //     return false;
    // }

    // void push_valid_gaddr(GAddr gaddr) {
    //     valid_gaddrs.insert(gaddr);
    // }

    // void print_valid_gaddr() {
    //     printf("valid Gaddr:");
    //     for (const auto& gaddr : valid_gaddrs) {
    //         std::cout << gaddr << std::endl;
    //     }
    // }

    // void pop_valid_gaddr(GAddr gaddr) {
    //     valid_gaddrs.erase(gaddr);
    // }

    void start_collection() {
        start = 1;
    }

    void end_collection() {
        start = 0;
    }

    bool is_start() {
        return start;
    }

    inline void start_record_with_memaccess_type() {
        if (start) {
            app_thread_counter = rdtsc();
        }
    }
    inline void set_memaccess_type(MEMACCESS_TYPE type = MEMACCESS_TYPE::DONT_DISTINGUISH) {
        if (start) {
            memaccess_type = type;
        }
    }
    inline void stop_record_with_memaccess_type() {
        if (start) {
            uint64_t ns = rdtscp() - app_thread_counter;
            memaccess_type_stats[memaccess_type]->record(ns);
            memaccess_type_stats[MEMACCESS_TYPE::DONT_DISTINGUISH]->record(ns);
        }
    }

    // inline void start_record_app_thread(GAddr gaddr) {
        // if (is_valid_gaddr(gaddr) && start) {
        //     std::lock_guard<std::mutex> lock(app_mtx);
        //     app_thread_counter = rdtsc();
        // }
    // }

    inline void start_record_app_thread() {
        app_thread_counter = rdtsc();
    }

    // inline void stop_record_app_thread_with_op(GAddr gaddr, APP_THREAD_OP op = APP_THREAD_OP::NONE) {
    //     if (is_valid_gaddr(gaddr) && start) {
    //         std::lock_guard<std::mutex> lock(app_mtx);
    //         // printf("stop_record_app_thread_with_op: gaddr = %ld, op = %d\n", gaddr, static_cast<int>(op));
    //         uint64_t ns = rdtscp() - app_thread_counter;
    //         if (op == APP_THREAD_OP::NONE) {
    //             app_thread_stats->record(ns);
    //         } else {
    //             app_thread_op_stats[op]->record(ns);
    //         }
    //     }
    // }
    inline void stop_record_app_thread_with_op(APP_THREAD_OP op = APP_THREAD_OP::NONE){
        uint64_t ns = rdtscp() - app_thread_counter;
        if (op == APP_THREAD_OP::NONE) {
            app_thread_stats->record(ns);
        } else {
            app_thread_op_stats[op]->record(ns);
        }
    }

    // inline void start_record_sys_thread(GAddr gaddr) {
    //     if (is_valid_gaddr(gaddr) && start) {
    //         sys_thread_counter = rdtsc();
    //     }
    // }
    // inline void stop_record_sys_thread_with_op(GAddr gaddr, SYS_THREAD_OP op = SYS_THREAD_OP::NONE) {
    //     if (is_valid_gaddr(gaddr) && start) {
    //         uint64_t ns = rdtscp() - sys_thread_counter;
    //         if (op == SYS_THREAD_OP::NONE) {
    //             sys_thread_stats->record_atomic(ns);
    //         } else {
    //             sys_thread_op_stats[op]->record_atomic(ns);
    //         }
    //     }
    // }

    inline void start_record_multi_sys_thread(uint64_t thread_id) {
        std::lock_guard<std::mutex> lock(sys_mtx[thread_id]);
        multi_sys_thread_counter[thread_id] = rdtsc();
    }
    inline void stop_record_multi_sys_thread_with_op(uint64_t thread_id, MULTI_SYS_THREAD_OP op = MULTI_SYS_THREAD_OP::NONE) {
        std::lock_guard<std::mutex> lock(sys_mtx[thread_id]);
        uint64_t ns = rdtscp() - multi_sys_thread_counter[thread_id];
        if (op == MULTI_SYS_THREAD_OP::NONE) {
            multi_sys_thread_stats[thread_id]->record(ns);
        } else {
            multi_sys_thread_op_stats[thread_id][op]->record(ns);
        }
    }

    inline void record_multi_sys_thread_with_op(uint64_t thread_id, uint64_t ns, MULTI_SYS_THREAD_OP op = MULTI_SYS_THREAD_OP::NONE) {
        if (op == MULTI_SYS_THREAD_OP::NONE) {
            multi_sys_thread_stats[thread_id]->record(ns);
        } else {
            multi_sys_thread_op_stats[thread_id][op]->record(ns);
        }
    }

    // TODO whether need add gaddr check?
    inline void record_poll_thread_with_op(uint64_t thread_id, uint64_t ns, MULTI_POLL_THREAD_OP op = MULTI_POLL_THREAD_OP::NONE) {
        if (op == MULTI_POLL_THREAD_OP::NONE) {
            multi_poll_thread_stats[thread_id]->record_atomic(ns);
        } else {
            multi_poll_thread_op_stats[thread_id][op]->record_atomic(ns);
        }
    }

    void clear_acecss_size_stats() {
        std::lock_guard<std::mutex> lock(access_size_mtx_);
        cachehit.store(0);
        read_size_stats_.clear();
        write_size_stats_.clear();
    }

    void waitForSpace() {
        char input;
        std::cout << "Please press the space bar to continue: ";
        while (true) {
            input = getchar();
            if (input == ' ') {
                break;
            } else {
                std::cout << "Invalid input. Please press the space bar: ";
                // Clear the input buffer
                while (getchar() != '\n');
            }
        }
    }

    int readMiss[48] = {0};
    int readMiss_1[48] = {0};
    int readMiss_2[48] = {0};

    int writeMiss[48] = {0};
    int writeMiss_1[48] = {0};

    int writeShared[48] = {0};
    int writeShared_1[48] = {0};
    int writeShared_2[48] = {0};

    int evictLine[48] = {0};
    int evictLine_1[48] = {0};
    int evictLine_2[48] = {0};
    int evictLine_3[48] = {0};
    int evictLine_4[48] = {0};


    void print_all_false_count(FILE *file, int threadNR){

        fprintf(file, "\nreadMiss:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", readMiss[i]);
        }

        fprintf(file, "\nreadMiss_1:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", readMiss_1[i]);
        }

        fprintf(file, "\nreadMiss_2:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", readMiss_2[i]);
        }

        //-----

        fprintf(file, "\nwriteMiss:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", writeMiss[i]);
        }

        fprintf(file, "\nwriteMiss_1:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", writeMiss_1[i]);
        }

        //-----

        fprintf(file, "\nwriteShared:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", writeShared[i]);
        }

        fprintf(file, "\nwriteShared_1:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", writeShared_1[i]);
        }

        fprintf(file, "\nwriteShared_2:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", writeShared_2[i]);
        }

        //-----

        fprintf(file, "\nevictLine:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", evictLine[i]);
        }

        fprintf(file, "\nevictLine_1:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", evictLine_1[i]);
        }

        fprintf(file, "\nevictLine_2:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", evictLine_2[i]);
        }

        fprintf(file, "\nevictLine_3:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", evictLine_2[i]);
        }

        fprintf(file, "\nevictLine_4:\n");
        for(int i = 0; i < threadNR; i++){
            fprintf(file, "%d\t", evictLine_2[i]);
        }

    }

    inline void RecordWrite(size_t size) {
        std::lock_guard<std::mutex> lock(access_size_mtx_);
        write_size_stats_[size]++;
    }

    /**
     * @brief 记录一次读操作的大小。线程安全。
     * @param size 读取的数据字节数。
     */
    inline void RecordRead(size_t size) {
        std::lock_guard<std::mutex> lock(access_size_mtx_);
        read_size_stats_[size]++;
    }
    
    /**
     * @brief 打印读写操作大小的统计报告。
     */
    void print_access_size_stats() {
        std::lock_guard<std::mutex> lock(access_size_mtx_);

        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << std::setw(55) << "Memory Access Size Profile Report" << "\n";
        std::cout << std::string(80, '=') << "\n";

        // 内部辅助函数，用于打印报告的单个部分
        auto report_section = [](const std::string& title, const std::unordered_map<size_t, uint64_t>& stats) {
            uint64_t total_ops = 0;
            uint64_t total_bytes = 0;
            
            // 排序以便于查看
            std::vector<std::pair<size_t, uint64_t>> sorted_stats(stats.begin(), stats.end());
            std::sort(sorted_stats.begin(), sorted_stats.end());

            // std::cout << "\n--- " << title << " ---\n";
            // if (sorted_stats.empty()) {
            //     std::cout << "No operations recorded.\n";
            //     return;
            // }

            // std::cout << std::left << std::setw(20) << "Size (bytes)"
            //           << std::setw(25) << "Count"
            //           << std::setw(25) << "Percentage" << "\n";
            // std::cout << std::string(70, '-') << "\n";

            for (const auto& pair : sorted_stats) {
                total_ops += pair.second;
            }

            if (total_ops == 0) {
                 std::cout << "No operations recorded.\n";
                 return;
            }

            // for (const auto& pair : sorted_stats) {
            //     size_t size = pair.first;
            //     uint64_t count = pair.second;
            //     total_bytes += size * count;
            //     double percentage = static_cast<double>(count) / total_ops * 100.0;

            //     std::cout << std::left << std::setw(20) << size
            //               << std::setw(25) << count
            //               << std::fixed << std::setprecision(2) << percentage << " %" << "\n";
            // }

            std::cout << std::string(70, '-') << "\n";
            std::cout << "Summary for " << title << ":\n";
            std::cout << "  - Total Operations: " << total_ops << "\n";
            // std::cout << "  - Total Bytes Transferred: " << total_bytes << " bytes (" 
            //           << std::fixed << std::setprecision(2) << static_cast<double>(total_bytes) / (1024 * 1024) << " MB)\n";
            // std::cout << "  - Average Size per Op: " << std::fixed << std::setprecision(2) 
            //           << static_cast<double>(total_bytes) / total_ops << " bytes\n";
        };
        
        // 报告写操作
        report_section("WRITE Operations Size", write_size_stats_);
        
        // 报告读操作
        report_section("READ Operations Size", read_size_stats_);
    }

};

extern agent_stats agent_stats_inst;

class RAII_Timer {
    public:
    RAII_Timer(MULTI_APP_THREAD_OP code,int iid){
        start_time = rdtsc();
        this->iid = iid;
        multi_app_thread_op = code;
    }

    ~RAII_Timer() {
        uint64_t end_time = rdtscp();
        uint64_t elapsed_time = end_time - start_time;
        agent_stats_inst.multi_app_thread_op_stats[iid][multi_app_thread_op]->record(elapsed_time);
    }
    private:
    uint64_t start_time;
    int iid;
    MULTI_APP_THREAD_OP multi_app_thread_op = MULTI_APP_THREAD_OP::NONE;

    uint64_t rdtsc() {
      unsigned int lo, hi;
      __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
      return ((uint64_t)hi << 32) | lo;
    }

    uint64_t rdtscp() {
      unsigned int lo, hi;
      __asm__ __volatile__("rdtscp" : "=a" (lo), "=d" (hi));
      return ((uint64_t)hi << 32) | lo;
    }
};
