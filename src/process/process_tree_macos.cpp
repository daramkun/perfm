#include "process/process_tree.h"

namespace perfm
{
std::vector<std::uint64_t> enumerate_process_tree(std::uint64_t root_pid)
{
    if (root_pid == 0)
    {
        return {};
    }
    return {root_pid};
}
}
