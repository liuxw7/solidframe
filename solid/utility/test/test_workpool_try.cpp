#include "solid/system/crashhandler.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/workpool.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;
namespace {
const LoggerT logger("test_basic");
}

int test_workpool_try(int argc, char* argv[])
{
    install_crash_handler();
    solid::log_start(std::cerr, {".*:EWXS", "test_basic:VIEWS"});
    using WorkPoolT  = WorkPool<size_t>;
    using AtomicPWPT = std::atomic<WorkPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());

    const int           wait_seconds = 500;
    int                 loop_cnt     = 5;
    const size_t        cnt{5000000};
    const size_t        v = (((cnt - 1) * cnt)) / 2;
    std::atomic<size_t> val{0};
    AtomicPWPT          pwp{nullptr};

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }
    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            size_t failed_push = 0;
            size_t check_v     = v;
            {
                WorkPoolT wp{
                    WorkPoolConfiguration(1, 1000), 1,
                    [&val](const size_t _v) {
                        val += _v;
                    }};
                pwp = &wp;
                for (size_t i = 0; i < cnt; ++i) {
                    if (!wp.tryPush(i)) {
                        check_v -= i;
                        this_thread::sleep_for(chrono::milliseconds(3));
                        ++failed_push;
                    }
                };
                pwp = nullptr;
            }
            solid_log(logger, Verbose, "after loop failed_push: " << failed_push);
            solid_check(check_v == val, "val = " << val << " v = " << check_v);
            val = 0;
        }
    };
    if (async(launch::async, lambda).wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {
        if (pwp != nullptr) {
            pwp.load()->dumpStatistics();
        }
        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
