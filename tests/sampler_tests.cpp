#include "core/sampler.h"
#include "process/process_tree.h"

#include <cassert>
#include <chrono>
#include <string>
#include <thread>

namespace
{
class fake_collector final : public perfm::collector
{
public:
    void start(std::uint64_t pid) override
    {
        started = true;
        observed_pid = pid;
    }

    std::vector<perfm::metric_value> sample() override
    {
        ++sample_count;
        return {perfm::make_metric("fake_value", perfm::metric_unit::none, static_cast<double>(sample_count))};
    }

    void stop() override
    {
        stopped = true;
    }

    bool started{false};
    bool stopped{false};
    std::uint64_t observed_pid{0};
    int sample_count{0};
};

void sampler_runs_collectors_until_process_exits()
{
    auto process = perfm::launch_child_process(PERFM_SLEEPER_PATH, {"650"});
    auto fake = std::make_unique<fake_collector>();
    auto* fake_ptr = fake.get();

    perfm::collector_list collectors;
    collectors.push_back(std::move(fake));

    perfm::sampler_config config;
    config.frequency = std::chrono::milliseconds(200);
    config.include_elapsed_time = true;

    const auto samples = perfm::sample_process(process, collectors, config);
    assert(fake_ptr->started);
    assert(fake_ptr->stopped);
    assert(fake_ptr->observed_pid > 0);
    assert(fake_ptr->sample_count >= 2);
    assert(samples.size() >= 2);
    assert(samples.back().elapsed >= std::chrono::milliseconds(600));
    assert(process.exit_code() == 0);
}

bool has_metric_named(const perfm::sample& current, const std::string& name)
{
    for (const auto& value : current.values)
    {
        if (value.name == name)
        {
            return true;
        }
    }
    return false;
}

void sampler_records_time_only_as_final_sample()
{
    auto process = perfm::launch_child_process(PERFM_SLEEPER_PATH, {"250"});
    perfm::collector_list collectors;

    perfm::sampler_config config;
    config.frequency = std::chrono::milliseconds(50);
    config.include_elapsed_time = true;

    const auto samples = perfm::sample_process(process, collectors, config);
    assert(samples.size() == 1);
    assert(samples.front().elapsed >= std::chrono::milliseconds(200));
    assert(has_metric_named(samples.front(), "elapsed_time_ms"));
    assert(process.exit_code() == 0);
}

void sampler_records_elapsed_metric_only_on_final_sample()
{
    auto process = perfm::launch_child_process(PERFM_SLEEPER_PATH, {"450"});
    auto fake = std::make_unique<fake_collector>();

    perfm::collector_list collectors;
    collectors.push_back(std::move(fake));

    perfm::sampler_config config;
    config.frequency = std::chrono::milliseconds(150);
    config.include_elapsed_time = true;

    const auto samples = perfm::sample_process(process, collectors, config);
    assert(samples.size() >= 2);
    for (std::size_t index = 0; index + 1 < samples.size(); ++index)
    {
        assert(!has_metric_named(samples[index], "elapsed_time_ms"));
    }
    assert(has_metric_named(samples.back(), "elapsed_time_ms"));
    assert(process.exit_code() == 0);
}

void process_tree_includes_spawned_descendants()
{
    auto process = perfm::launch_child_process(PERFM_SPAWNER_PATH, {PERFM_SLEEPER_PATH, "700"});

    bool found_descendant = false;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        const auto pids = perfm::enumerate_process_tree(process.pid());
        if (pids.size() >= 2)
        {
            found_descendant = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    assert(found_descendant);
    process.wait();
    assert(process.exit_code() == 0);
}
}

void run_sampler_tests()
{
    sampler_runs_collectors_until_process_exits();
    sampler_records_time_only_as_final_sample();
    sampler_records_elapsed_metric_only_on_final_sample();
    process_tree_includes_spawned_descendants();
}
