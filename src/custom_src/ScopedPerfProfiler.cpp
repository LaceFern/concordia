#include "ScopedPerfProfiler.h"
#include <iostream>
#include <sys/wait.h>
#include <csignal>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <cstring>

ScopedPerfProfiler::ScopedPerfProfiler(const std::vector<std::string>& perf_events) {
    pid_t parent_pid = getpid(); // 获取当前进程（即父进程）的PID

    perf_pid_ = fork(); // fork一个子进程
    if (perf_pid_ == -1) {
        int err = errno;
        throw std::runtime_error("Failed to fork process for perf: " + std::string(strerror(err)));
    }

    if (perf_pid_ == 0) {
        // === 子进程代码 ===
        // 构造 `perf` 命令
        std::vector<const char*> perf_args;
        perf_args.push_back("perf");
        perf_args.push_back("stat");
        perf_args.push_back("-p");
        std::string parent_pid_str = std::to_string(parent_pid);
        perf_args.push_back(parent_pid_str.c_str());

        // 添加事件
        if (!perf_events.empty()) {
            perf_args.push_back("-e");
            static std::string event_string; // 必须是静态或在更高作用域，以保证其生命周期
            for (size_t i = 0; i < perf_events.size(); ++i) {
                event_string += perf_events[i] + (i == perf_events.size() - 1 ? "" : ",");
            }
            perf_args.push_back(event_string.c_str());
        }
        perf_args.push_back(nullptr);

        // 用perf替换子进程
        execvp("perf", (char* const*)perf_args.data());
        
        // 如果execvp成功，下面的代码不会执行
        perror("execvp(perf) failed. Is 'perf' installed and in your PATH?");
        exit(127);
    } else {
        // === 父进程代码 ===
        // 等待一小会儿，以确保perf有足够的时间附加成功
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        std::cout << "[Profiler] Perf (PID: " << perf_pid_ << ") is now attached to TPC-C process (PID: " << parent_pid << ").\n";
    }
}

ScopedPerfProfiler::~ScopedPerfProfiler() {
    if (perf_pid_ <= 0) {
        return; // 如果fork失败或在子进程中，则不执行任何操作
    }

    std::cout << "\n[Profiler] Test function finished. Stopping perf (PID: " << perf_pid_ << ") to generate report...\n";
    
    // 1. 发送SIGINT (Ctrl+C)信号给perf进程，使其停止并打印报告
    if (kill(perf_pid_, SIGINT) != 0) {
        perror("[Profiler] Failed to send SIGINT to perf process");
    }

    // 2. 等待子进程（perf）完全终止，并回收它以避免僵尸进程
    int status;
    waitpid(perf_pid_, &status, 0);

    std::cout << "[Profiler] Profiling session has ended.\n";
}

FlameGraphProfiler::FlameGraphProfiler(int sampling_frequency, std::string output_svg_path) 
    // 将临时文件路径改为绝对路径，指向/tmp
    : output_svg_path_(std::move(output_svg_path)),
      perf_data_file_("/tmp/perf.data.tmp." + std::to_string(getpid())), // 添加PID防止多实例冲突
      stacks_file_("/tmp/stacks.tmp." + std::to_string(getpid()))
{
    // ... 构造函数的其余部分保持不变 ...
    pid_t parent_pid = getpid();
    perf_pid_ = fork();
    
    if (perf_pid_ == -1) {
        throw std::runtime_error("Failed to fork process for perf record.");
    }

    if (perf_pid_ == 0) {
        std::string freq_str = std::to_string(sampling_frequency);
        const char* const perf_args[] = {
            "perf", "record",
            "-F", freq_str.c_str(),
            "-p", std::to_string(parent_pid).c_str(),
            "-g",
            "--call-graph", "dwarf",
            "-o", perf_data_file_.c_str(), // 使用绝对路径
            nullptr
        };
        
        execvp("perf", (char* const*)perf_args);
        perror("execvp(perf record) failed. Is 'perf' installed and in PATH?");
        exit(127);
    } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::cout << "[Profiler] perf record (PID: " << perf_pid_ << ") is now sampling TPC-C process (PID: " << parent_pid << ").\n";
    }
}

FlameGraphProfiler::~FlameGraphProfiler() {
    // ... 析构函数的前半部分保持不变 ...
    if (perf_pid_ <= 0) return;

    std::cout << "\n[Profiler] Test function finished. Stopping perf record...\n";
    
    if (kill(perf_pid_, SIGINT) != 0) {
        perror("[Profiler] Failed to send SIGINT to perf record");
    }
    waitpid(perf_pid_, nullptr, 0);

    // 从这里开始，所有的命令都使用绝对路径
    try {
        std::cout << "[Profiler] Generating stacks from " << perf_data_file_ << "...\n";
        // perf script 命令也使用绝对路径
        std::string perf_script_cmd = "perf script -i " + perf_data_file_ + " -f > " + stacks_file_ + ".raw";
        execute_command(perf_script_cmd);

        std::cout << "[Profiler] Collapsing stacks using stackcollapse-perf.pl...\n";
        std::string collapse_cmd = "../../FlameGraph/stackcollapse-perf.pl " + stacks_file_ + ".raw > " + stacks_file_ + ".folded";
        execute_command(collapse_cmd);

        std::cout << "[Profiler] Creating Flame Graph: " << output_svg_path_ << "...\n";
        // flamegraph.pl 命令也使用绝对路径
        std::string flamegraph_cmd = "../../FlameGraph/flamegraph.pl " + stacks_file_ + ".folded > " + output_svg_path_;
        execute_command(flamegraph_cmd);

        std::cout << "[Profiler] Flame Graph successfully generated!\n";

    } catch (const std::exception& e) {
        std::cerr << "[Profiler] An error occurred during Flame Graph generation: " << e.what() << std::endl;
    }

    std::cout << "[Profiler] Cleaning up temporary files...\n";
    remove(perf_data_file_.c_str()); // 使用绝对路径删除
    remove(stacks_file_.c_str());   // 使用绝对路径删除
}

void FlameGraphProfiler::execute_command(const std::string& command) {
    int ret = system(command.c_str());
    if (ret != 0) {
        throw std::runtime_error("Command failed with exit code " + std::to_string(WEXITSTATUS(ret)) + ": " + command);
    }
}

PerfLockProfiler::PerfLockProfiler(std::string& output_filename, bool include_callchain) {
    pid_t parent_pid = getpid(); // 获取当前进程 (父进程) 的 PID

    perf_pid_ = fork(); // Fork 一个子进程来运行 perf

    if (perf_pid_ == -1) {
        int err = errno;
        throw std::runtime_error("Failed to fork process for perf: " + std::string(strerror(err)));
    }

    if (perf_pid_ == 0) {
        // === 子进程代码 ===
        // 构造 `perf lock record` 命令的参数列表
        std::vector<const char*> perf_args;
        perf_args.push_back("perf");
        perf_args.push_back("lock");
        perf_args.push_back("record");
        
        // 附加到父进程
        perf_args.push_back("-p");
        std::string parent_pid_str = std::to_string(parent_pid);
        perf_args.push_back(parent_pid_str.c_str());

        // 添加调用栈信息 (如果需要)
        if (include_callchain) {
            perf_args.push_back("-g");
        }

        // 指定输出文件
        std::string prefix = "./";
        output_filename = prefix + output_filename; // 确保输出文件在当前目录
        perf_args.push_back("-o");
        perf_args.push_back(output_filename.c_str());

        // 参数列表结束符
        perf_args.push_back(nullptr);

        // 使用 execvp 启动 perf。它会替换当前子进程的映像。
        execvp("perf", (char* const*)perf_args.data());
        
        // 如果 execvp 成功执行，下面的代码将永远不会被执行。
        // 如果执行到这里，说明发生了错误。
        perror("execvp(perf) failed. Is 'perf' installed and in your PATH?");
        exit(127); // 退出子进程，返回一个错误码
    } else {
        // === 父进程代码 ===
        // 等待一小会儿，以确保 perf 子进程有足够的时间附加到父进程上。
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::cout << "[PerfLockProfiler] Started recording lock events. Perf PID: " << perf_pid_ 
                  << ", Target PID: " << parent_pid << std::endl;
        std::cout << "[PerfLockProfiler] Data will be saved to '" << output_filename << "' on exit." << std::endl;
    }
}

PerfLockProfiler::~PerfLockProfiler() {
    if (perf_pid_ > 0) {
        std::cout << "[PerfLockProfiler] Stopping perf process (PID: " << perf_pid_ << ")..." << std::endl;
        
        // 向 perf 子进程发送 SIGINT (中断) 信号。
        // 这是 `perf record` 的标准、优雅的停止方式。
        if (kill(perf_pid_, SIGINT) == 0) {
            int status;
            // 等待子进程完全终止，避免产生僵尸进程
            waitpid(perf_pid_, &status, 0); 
            std::cout << "[PerfLockProfiler] Perf process terminated." << std::endl;
            std::cout << "[PerfLockProfiler] To analyze the data, run: 'perf lock report -i <output_filename>'" << std::endl;
        } else {
            // 如果发送信号失败，打印错误信息
            perror("[PerfLockProfiler] Failed to send SIGINT to perf process");
        }
    }
}

OffCPUFlameGraphProfiler::OffCPUFlameGraphProfiler(std::string output_svg_path)
    : output_svg_path_(std::move(output_svg_path)),
      perf_data_file_("/tmp/perf.offcpu.data.tmp." + std::to_string(getpid())),
      stacks_file_("/tmp/stacks.offcpu.tmp." + std::to_string(getpid()))
{
    pid_t parent_pid = getpid();
    perf_pid_ = fork();

    if (perf_pid_ == -1) {
        throw std::runtime_error("Failed to fork process for perf record.");
    }

    if (perf_pid_ == 0) {
        // === 子进程代码 ===
        std::string parent_pid_str = std::to_string(parent_pid);

        const char* const perf_args[] = {
            "perf", "record",
            "-e", "sched:sched_switch", // 追踪调度事件，而不是CPU采样
            "-p", parent_pid_str.c_str(),
            "-g",                      // 依然需要调用栈
            "--call-graph", "dwarf",   // 使用 DWARF 可以得到更准确的栈 (可选但推荐)
            "-o", perf_data_file_.c_str(),
            nullptr
        };

        execvp("perf", (char* const*)perf_args);
        perror("execvp(perf record) for Off-CPU failed. Is 'perf' installed and in PATH?");
        exit(127);
    } else {
        // === 父进程代码 ===
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::cout << "[OffCPUProfiler] perf record (PID: " << perf_pid_ << ") is now recording scheduler events for TPC-C process (PID: " << parent_pid << ").\n";
    }
}

// 析构函数：修改 stackcollapse-perf.pl 命令
OffCPUFlameGraphProfiler::~OffCPUFlameGraphProfiler() {
    if (perf_pid_ <= 0) return;

    std::cout << "\n[OffCPUProfiler] Stopping perf record...\n";

    if (kill(perf_pid_, SIGINT) != 0) {
        perror("[OffCPUProfiler] Failed to send SIGINT to perf record");
    }
    waitpid(perf_pid_, nullptr, 0);

    try {
        std::cout << "[OffCPUProfiler] Generating stacks from " << perf_data_file_ << "...\n";
        std::string perf_script_cmd = "perf script -i " + perf_data_file_ + " > " + stacks_file_ + ".raw";
        execute_command(perf_script_cmd);

        std::cout << "[OffCPUProfiler] Collapsing stacks for Off-CPU analysis...\n";
        // 假设 FlameGraph 脚本位于上一层的目录
        std::string collapse_cmd = "../../FlameGraph/stackcollapse-perf.pl " + stacks_file_ + ".raw > " + stacks_file_ + ".folded";
        execute_command(collapse_cmd);

        std::cout << "[OffCPUProfiler] Creating Off-CPU Flame Graph: " << output_svg_path_ << "...\n";
        // flamegraph.pl 命令本身不需要改变，但可以改变标题和颜色方案
        std::string flamegraph_cmd = "../../FlameGraph/flamegraph.pl --color=io --title='Off-CPU Time Flame Graph' " + stacks_file_ + ".folded > " + output_svg_path_;
        execute_command(flamegraph_cmd);

        std::cout << "[OffCPUProfiler] Off-CPU Flame Graph successfully generated at: " << output_svg_path_ << "\n";

    } catch (const std::exception& e) {
        std::cerr << "[OffCPUProfiler] An error occurred during Flame Graph generation: " << e.what() << std::endl;
    }

    // 清理临时文件
    std::cout << "[OffCPUProfiler] Cleaning up temporary files...\n";
    remove(perf_data_file_.c_str());
    std::string raw_stacks = stacks_file_ + ".raw";
    std::string folded_stacks = stacks_file_ + ".folded";
    remove(raw_stacks.c_str());
    remove(folded_stacks.c_str());
}

// 辅助函数，用于执行shell命令
void OffCPUFlameGraphProfiler::execute_command(const std::string& cmd) {
    std::cout << "[CMD] " << cmd << std::endl;
    int ret = system(cmd.c_str());
    if (ret != 0) {
        throw std::runtime_error("Command failed with exit code " + std::to_string(ret) + ": " + cmd);
    }
}