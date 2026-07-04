#include "process/process_tree.h"

#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

#include <unordered_map>
#include <unordered_set>

namespace perfm
{
std::vector<std::uint64_t> enumerate_process_tree(std::uint64_t root_pid)
{
    std::vector<std::uint64_t> result;
    if (root_pid == 0)
    {
        return result;
    }

    result.push_back(root_pid);

    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
    {
        return result;
    }

    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> children_by_parent;
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            children_by_parent[entry.th32ParentProcessID].push_back(entry.th32ProcessID);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

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
