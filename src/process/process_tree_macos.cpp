#include "process/process_tree.h"

#include <sys/sysctl.h>

#include <unordered_map>
#include <unordered_set>

namespace perfm
{
std::vector<std::uint64_t> enumerate_process_tree(std::uint64_t root_pid)
{
    if (root_pid == 0)
    {
        return {};
    }
    std::vector<std::uint64_t> result{root_pid};

    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
    std::size_t buffer_size = 0;
    if (sysctl(mib, 4, nullptr, &buffer_size, nullptr, 0) != 0 || buffer_size == 0)
    {
        return result;
    }

    std::vector<kinfo_proc> processes(buffer_size / sizeof(kinfo_proc));
    if (sysctl(mib, 4, processes.data(), &buffer_size, nullptr, 0) != 0)
    {
        return result;
    }
    processes.resize(buffer_size / sizeof(kinfo_proc));

    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> children_by_parent;
    for (const auto& process : processes)
    {
        const auto pid = static_cast<std::uint64_t>(process.kp_proc.p_pid);
        const auto parent_pid = static_cast<std::uint64_t>(process.kp_eproc.e_ppid);
        if (pid != 0 && parent_pid != 0)
        {
            children_by_parent[parent_pid].push_back(pid);
        }
    }

    std::unordered_set<std::uint64_t> seen;
    seen.insert(root_pid);
    for (std::size_t index = 0; index < result.size(); ++index)
    {
        const auto found = children_by_parent.find(result[index]);
        if (found == children_by_parent.end())
        {
            continue;
        }
        for (const auto child_pid : found->second)
        {
            if (seen.insert(child_pid).second)
            {
                result.push_back(child_pid);
            }
        }
    }

    return result;
}
}
