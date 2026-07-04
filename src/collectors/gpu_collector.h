#pragma once

#include "core/collector.h"

namespace perfm
{
collector_ptr make_gpu_collector(bool include_gpu_percent, bool include_vram);
}
