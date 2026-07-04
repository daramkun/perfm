#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace perfm
{
class child_process
{
public:
    child_process() = default;
    child_process(const child_process&) = delete;
    child_process& operator=(const child_process&) = delete;
    child_process(child_process&& other) noexcept;
    child_process& operator=(child_process&& other) noexcept;
    ~child_process();

    std::uint64_t pid() const;
    bool is_running();
    int exit_code();
    int wait();
    void terminate();

private:
    friend child_process launch_child_process(const std::string& target_path, const std::vector<std::string>& target_args);

#ifdef _WIN32
    void* process_handle_{nullptr};
    void* thread_handle_{nullptr};
    std::uint64_t process_id_{0};
#else
    int process_id_{-1};
#endif
    std::optional<int> exit_code_;
};

child_process launch_child_process(const std::string& target_path, const std::vector<std::string>& target_args);
}
