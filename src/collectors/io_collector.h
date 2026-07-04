#pragma once

#include "core/collector.h"

namespace perfm
{
collector_ptr make_file_io_collector();
collector_ptr make_network_collector();
}
