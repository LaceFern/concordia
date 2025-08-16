#ifndef SCOPED_PERF_PROFILER_H
#define SCOPED_PERF_PROFILER_H

#include <string>
#include <vector>
#include <unistd.h> // for pid_t

class ScopedPerfProfiler {
public:
    /**
     * @brief 构造函数。在对象创建时启动perf并附加到当前进程。
     * @param perf_events 要分析的性能事件列表。
     */
    ScopedPerfProfiler(const std::vector<std::string>& perf_events = {
                           "cycles", "instructions", "cache-references", "cache-misses"
                       });

    /**
     * @brief 析构函数。在对象销毁时停止perf并打印报告。
     */
    ~ScopedPerfProfiler();

    // 禁止拷贝和赋值，因为每个实例都管理一个唯一的子进程
    ScopedPerfProfiler(const ScopedPerfProfiler&) = delete;
    ScopedPerfProfiler& operator=(const ScopedPerfProfiler&) = delete;

private:
    pid_t perf_pid_ = -1; // 用于存储perf子进程的PID
};

class FlameGraphProfiler {
public:
    /**
     * @brief 构造函数，在对象创建时启动`perf record`。
     * @param sampling_frequency 采样频率 (Hz)。99Hz是一个好选择，可以避免与系统定时器同步。
     * @param output_svg_path 生成的火焰图SVG文件的最终路径。
     */
    explicit FlameGraphProfiler(int sampling_frequency = 99, 
                                std::string output_svg_path = "flamegraph.svg");

    /**
     * @brief 析构函数，在对象销毁时停止`perf record`，并自动执行后续步骤生成火焰图。
     */
    ~FlameGraphProfiler();

    // 禁止拷贝和赋值
    FlameGraphProfiler(const FlameGraphProfiler&) = delete;
    FlameGraphProfiler& operator=(const FlameGraphProfiler&) = delete;

private:
    void execute_command(const std::string& command);

    pid_t perf_pid_ = -1;
    std::string output_svg_path_;
    std::string perf_data_file_ = "perf.data.tmp";
    std::string stacks_file_ = "stacks.tmp";
};

class PerfLockProfiler {
public:
    /**
     * @brief 构造函数。在创建对象时，会自动fork一个子进程并执行 `perf lock record`
     *        来开始录制锁事件。
     * @param output_filename 要保存录制数据的输出文件名。
     * @param include_callchain 是否包含调用栈信息 (强烈推荐)。
     */
    explicit PerfLockProfiler(std::string& output_filename, bool include_callchain = true);

    /**
     * @brief 析构函数。在对象作用域结束时，会自动向 `perf` 子进程发送 SIGINT 信号
     *        以优雅地停止录制，并等待其退出。
     */
    ~PerfLockProfiler();

    // 禁止拷贝和赋值，因为这个类管理着一个子进程资源
    PerfLockProfiler(const PerfLockProfiler&) = delete;
    PerfLockProfiler& operator=(const PerfLockProfiler&) = delete;

private:
    pid_t perf_pid_ = -1; // 用于保存 perf 子进程的 PID
};

class OffCPUFlameGraphProfiler {
public:
    /**
     * @brief 构造函数，在创建时启动 `perf record -e sched:sched_switch`。
     * @param output_svg_path 最终生成的Off-CPU火焰图SVG文件的路径。
     */
    explicit OffCPUFlameGraphProfiler(std::string output_svg_path);

    /**
     * @brief 析构函数，停止perf，处理数据，并生成最终的火焰图。
     */
    ~OffCPUFlameGraphProfiler();

    // 禁止拷贝和赋值
    OffCPUFlameGraphProfiler(const OffCPUFlameGraphProfiler&) = delete;
    OffCPUFlameGraphProfiler& operator=(const OffCPUFlameGraphProfiler&) = delete;

private:
    void execute_command(const std::string& cmd);

    pid_t perf_pid_ = -1;
    std::string output_svg_path_;
    std::string perf_data_file_;
    std::string stacks_file_;
};


#endif // SCOPED_PERF_PROFILER_H