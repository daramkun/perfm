#include "collectors/gpu_collector.h"
#include "process/process_tree.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#endif

#include <cctype>
#include <cwctype>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>

namespace perfm
{
namespace
{
#ifdef _WIN32
std::optional<std::uint64_t> parse_pid_from_instance(const wchar_t* instance_name)
{
    if (instance_name == nullptr)
    {
        return std::nullopt;
    }

    const std::wstring instance(instance_name);
    const std::wstring marker = L"pid_";
    const auto start = instance.find(marker);
    if (start == std::wstring::npos)
    {
        return std::nullopt;
    }

    std::size_t cursor = start + marker.size();
    std::uint64_t pid = 0;
    bool has_digit = false;
    while (cursor < instance.size() && std::iswdigit(instance[cursor]) != 0)
    {
        has_digit = true;
        pid = pid * 10 + static_cast<std::uint64_t>(instance[cursor] - L'0');
        ++cursor;
    }

    if (!has_digit)
    {
        return std::nullopt;
    }
    return pid;
}

std::unordered_set<std::uint64_t> make_pid_set(std::uint64_t root_pid, collector_scope scope)
{
    std::unordered_set<std::uint64_t> pids;
    if (scope == collector_scope::process_only)
    {
        pids.insert(root_pid);
        return pids;
    }

    for (const auto pid : enumerate_process_tree(root_pid))
    {
        pids.insert(pid);
    }
    return pids;
}

class windows_gpu_collector final : public collector
{
public:
    windows_gpu_collector(bool include_gpu_percent, bool include_vram)
        : include_gpu_percent_(include_gpu_percent),
          include_vram_(include_vram)
    {
    }

    void start(std::uint64_t pid) override
    {
        start(pid, collector_scope::process_tree);
    }

    void start(std::uint64_t pid, collector_scope scope) override
    {
        root_pid_ = pid;
        scope_ = scope;
        open_query();
    }

    std::vector<metric_value> sample() override
    {
        if (query_ != nullptr)
        {
            PdhCollectQueryData(query_);
        }

        const auto target_pids = make_pid_set(root_pid_, scope_);
        std::vector<metric_value> values;
        if (include_gpu_percent_)
        {
            if (utilization_counter_ == nullptr)
            {
                values.push_back(make_unsupported_metric("gpu_percent", metric_unit::percent));
            }
            else
            {
                values.push_back(make_metric("gpu_percent", metric_unit::percent, sum_double_counter(utilization_counter_, target_pids)));
            }
        }
        if (include_vram_)
        {
            if (dedicated_memory_counter_ == nullptr)
            {
                values.push_back(make_unsupported_metric("gpu_vram_bytes", metric_unit::bytes));
            }
            else
            {
                values.push_back(make_metric("gpu_vram_bytes", metric_unit::bytes, static_cast<double>(sum_large_counter(dedicated_memory_counter_, target_pids))));
            }
        }
        return values;
    }

    void stop() override
    {
        if (query_ != nullptr)
        {
            PdhCloseQuery(query_);
            query_ = nullptr;
            utilization_counter_ = nullptr;
            dedicated_memory_counter_ = nullptr;
        }
    }

    ~windows_gpu_collector() override
    {
        stop();
    }

private:
    void open_query()
    {
        stop();
        if (PdhOpenQueryW(nullptr, 0, &query_) != ERROR_SUCCESS)
        {
            query_ = nullptr;
            return;
        }

        if (include_gpu_percent_ &&
            PdhAddEnglishCounterW(query_, L"\\GPU Engine(*)\\Utilization Percentage", 0, &utilization_counter_) != ERROR_SUCCESS)
        {
            utilization_counter_ = nullptr;
        }
        if (include_vram_ &&
            PdhAddEnglishCounterW(query_, L"\\GPU Process Memory(*)\\Dedicated Usage", 0, &dedicated_memory_counter_) != ERROR_SUCCESS)
        {
            dedicated_memory_counter_ = nullptr;
        }

        PdhCollectQueryData(query_);
    }

    static double sum_double_counter(PDH_HCOUNTER counter, const std::unordered_set<std::uint64_t>& target_pids)
    {
        DWORD buffer_size = 0;
        DWORD item_count = 0;
        auto status = PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &buffer_size, &item_count, nullptr);
        if (status != PDH_MORE_DATA || buffer_size == 0 || item_count == 0)
        {
            return 0.0;
        }

        auto buffer = std::make_unique<unsigned char[]>(buffer_size);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.get());
        status = PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &buffer_size, &item_count, items);
        if (status != ERROR_SUCCESS)
        {
            return 0.0;
        }

        double total = 0.0;
        for (DWORD index = 0; index < item_count; ++index)
        {
            const auto pid = parse_pid_from_instance(items[index].szName);
            if (pid.has_value() && target_pids.contains(*pid) && items[index].FmtValue.CStatus == ERROR_SUCCESS)
            {
                total += items[index].FmtValue.doubleValue;
            }
        }
        return total;
    }

    static unsigned long long sum_large_counter(PDH_HCOUNTER counter, const std::unordered_set<std::uint64_t>& target_pids)
    {
        DWORD buffer_size = 0;
        DWORD item_count = 0;
        auto status = PdhGetFormattedCounterArrayW(counter, PDH_FMT_LARGE, &buffer_size, &item_count, nullptr);
        if (status != PDH_MORE_DATA || buffer_size == 0 || item_count == 0)
        {
            return 0;
        }

        auto buffer = std::make_unique<unsigned char[]>(buffer_size);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.get());
        status = PdhGetFormattedCounterArrayW(counter, PDH_FMT_LARGE, &buffer_size, &item_count, items);
        if (status != ERROR_SUCCESS)
        {
            return 0;
        }

        unsigned long long total = 0;
        for (DWORD index = 0; index < item_count; ++index)
        {
            const auto pid = parse_pid_from_instance(items[index].szName);
            if (pid.has_value() && target_pids.contains(*pid) && items[index].FmtValue.CStatus == ERROR_SUCCESS &&
                items[index].FmtValue.largeValue > 0)
            {
                total += static_cast<unsigned long long>(items[index].FmtValue.largeValue);
            }
        }
        return total;
    }

    bool include_gpu_percent_{false};
    bool include_vram_{false};
    std::uint64_t root_pid_{0};
    collector_scope scope_{collector_scope::process_tree};
    PDH_HQUERY query_{nullptr};
    PDH_HCOUNTER utilization_counter_{nullptr};
    PDH_HCOUNTER dedicated_memory_counter_{nullptr};
};
#endif

#ifdef __APPLE__
std::optional<double> read_number_property(CFDictionaryRef dictionary, CFStringRef key)
{
    const auto value = CFDictionaryGetValue(dictionary, key);
    if (value == nullptr || CFGetTypeID(value) != CFNumberGetTypeID())
    {
        return std::nullopt;
    }

    double number = 0.0;
    if (!CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberDoubleType, &number))
    {
        return std::nullopt;
    }
    return number;
}

std::optional<double> first_number_property(CFDictionaryRef dictionary, const CFStringRef* keys, std::size_t key_count)
{
    for (std::size_t index = 0; index < key_count; ++index)
    {
        const auto value = read_number_property(dictionary, keys[index]);
        if (value.has_value())
        {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<double> first_number_property_containing(CFDictionaryRef dictionary, const CFStringRef* needles, std::size_t needle_count)
{
    const auto count = CFDictionaryGetCount(dictionary);
    if (count <= 0)
    {
        return std::nullopt;
    }

    std::vector<const void*> keys(static_cast<std::size_t>(count));
    std::vector<const void*> values(static_cast<std::size_t>(count));
    CFDictionaryGetKeysAndValues(dictionary, keys.data(), values.data());

    for (CFIndex index = 0; index < count; ++index)
    {
        const auto key = static_cast<CFTypeRef>(keys[static_cast<std::size_t>(index)]);
        const auto value = static_cast<CFTypeRef>(values[static_cast<std::size_t>(index)]);
        if (key == nullptr || value == nullptr || CFGetTypeID(key) != CFStringGetTypeID() ||
            CFGetTypeID(value) != CFNumberGetTypeID())
        {
            continue;
        }

        const auto key_string = static_cast<CFStringRef>(key);
        for (std::size_t needle_index = 0; needle_index < needle_count; ++needle_index)
        {
            if (CFStringFind(key_string, needles[needle_index], kCFCompareCaseInsensitive).location == kCFNotFound)
            {
                continue;
            }

            double number = 0.0;
            if (CFNumberGetValue(static_cast<CFNumberRef>(value), kCFNumberDoubleType, &number))
            {
                return number;
            }
        }
    }

    return std::nullopt;
}

class macos_system_gpu_collector final : public collector
{
public:
    macos_system_gpu_collector(bool include_gpu_percent, bool include_vram)
        : include_gpu_percent_(include_gpu_percent),
          include_vram_(include_vram)
    {
    }

    void start(std::uint64_t) override
    {
    }

    std::vector<metric_value> sample() override
    {
        double gpu_percent = 0.0;
        double vram_bytes = 0.0;
        bool has_gpu_percent = false;
        bool has_vram = false;

        io_iterator_t iterator = IO_OBJECT_NULL;
        if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IOAccelerator"), &iterator) == KERN_SUCCESS)
        {
            io_object_t service = IO_OBJECT_NULL;
            while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL)
            {
                const auto statistics =
                    IORegistryEntryCreateCFProperty(service, CFSTR("PerformanceStatistics"), kCFAllocatorDefault, 0);
                if (statistics != nullptr && CFGetTypeID(statistics) == CFDictionaryGetTypeID())
                {
                    read_statistics(static_cast<CFDictionaryRef>(statistics), gpu_percent, vram_bytes, has_gpu_percent, has_vram);
                }
                if (statistics != nullptr)
                {
                    CFRelease(statistics);
                }
                IOObjectRelease(service);
            }
            IOObjectRelease(iterator);
        }

        std::vector<metric_value> values;
        if (include_gpu_percent_)
        {
            if (has_gpu_percent)
            {
                values.push_back(make_metric("gpu_percent", metric_unit::percent, gpu_percent));
            }
            else
            {
                values.push_back(make_unsupported_metric("gpu_percent", metric_unit::percent));
            }
        }
        if (include_vram_)
        {
            if (has_vram)
            {
                values.push_back(make_metric("gpu_vram_bytes", metric_unit::bytes, vram_bytes));
            }
            else
            {
                values.push_back(make_unsupported_metric("gpu_vram_bytes", metric_unit::bytes));
            }
        }
        return values;
    }

    void stop() override
    {
    }

private:
    static void read_statistics(CFDictionaryRef statistics,
                                double& gpu_percent,
                                double& vram_bytes,
                                bool& has_gpu_percent,
                                bool& has_vram)
    {
        const CFStringRef utilization_keys[] = {
            CFSTR("Device Utilization %"),
            CFSTR("Renderer Utilization %"),
            CFSTR("Tiler Utilization %"),
        };
        const auto utilization = first_number_property(statistics, utilization_keys, std::size(utilization_keys));
        if (utilization.has_value())
        {
            gpu_percent += *utilization;
            has_gpu_percent = true;
        }

        const CFStringRef vram_keys[] = {
            CFSTR("vramUsedBytes"),
            CFSTR("In use video memory"),
            CFSTR("In use system memory"),
            CFSTR("Alloc system memory"),
        };
        const auto vram = first_number_property(statistics, vram_keys, std::size(vram_keys));
        const CFStringRef vram_needles[] = {
            CFSTR("vram"),
            CFSTR("video memory"),
            CFSTR("in use system memory"),
            CFSTR("alloc system memory"),
        };
        const auto fallback_vram = vram.has_value()
                                       ? vram
                                       : first_number_property_containing(statistics, vram_needles, std::size(vram_needles));
        if (fallback_vram.has_value())
        {
            vram_bytes += *fallback_vram;
            has_vram = true;
        }
    }

    bool include_gpu_percent_{false};
    bool include_vram_{false};
};
#endif

class unsupported_gpu_collector final : public collector
{
public:
    unsupported_gpu_collector(bool include_gpu_percent, bool include_vram)
        : include_gpu_percent_(include_gpu_percent),
          include_vram_(include_vram)
    {
    }

    void start(std::uint64_t) override
    {
    }

    std::vector<metric_value> sample() override
    {
        std::vector<metric_value> values;
        if (include_gpu_percent_)
        {
            values.push_back(make_unsupported_metric("gpu_percent", metric_unit::percent));
        }
        if (include_vram_)
        {
            values.push_back(make_unsupported_metric("gpu_vram_bytes", metric_unit::bytes));
        }
        return values;
    }

    void stop() override
    {
    }

private:
    bool include_gpu_percent_{false};
    bool include_vram_{false};
};
}

collector_ptr make_gpu_collector(bool include_gpu_percent, bool include_vram)
{
#ifdef _WIN32
    return std::make_unique<windows_gpu_collector>(include_gpu_percent, include_vram);
#elif defined(__APPLE__)
    return std::make_unique<macos_system_gpu_collector>(include_gpu_percent, include_vram);
#else
    return std::make_unique<unsupported_gpu_collector>(include_gpu_percent, include_vram);
#endif
}
}
