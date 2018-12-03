#include "solid/system/exception.hpp"
#include "solid/utility/function.hpp"
#include "solid/utility/workpool.hpp"
#include <atomic>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <thread>

using namespace solid;
using namespace std;

using thread_t      = std::thread;
using mutex_t       = std::mutex;
using unique_lock_t = std::unique_lock<mutex_t>;

namespace {

mutex_t       mtx;
const LoggerT logger("test_context");

struct Context {
    string text_;
    size_t count_;

    Context(const std::string& _txt, size_t _count)
        : text_(_txt)
        , count_(_count)
    {
    }
    ~Context()
    {
        solid_log(generic_logger, Verbose, this << " count = " << count_);
    }
};

using FunctionJobT = std::function<void(Context&)>;
//using FunctionJobT = solid::Function<32, void(Context&)>;

} // namespace

int test_workpool_context(int argc, char* argv[])
{
    solid::log_start(std::cerr, {".*:EWS", "test_context:VIEWS"});
    using WorkPoolT  = WorkPool<FunctionJobT>;
    using AtomicPWPT = std::atomic<WorkPoolT*>;

    solid_log(logger, Statistic, "thread concurrency: " << thread::hardware_concurrency());

    int                 wait_seconds = 500;
    int                 loop_cnt     = 5;
    const size_t        cnt{5000000};
    std::atomic<size_t> val{0};
    AtomicPWPT          pwp{nullptr};
    const size_t        v = ((cnt - 1) * cnt) / 2;

    if (argc > 1) {
        loop_cnt = atoi(argv[1]);
    }

    auto lambda = [&]() {
        for (int i = 0; i < loop_cnt; ++i) {
            {
                WorkPoolT wp{
                    WorkPoolConfiguration(),
                    [](FunctionJobT& _rj, Context& _rctx) {
                        _rj(_rctx);
                    },
                    "simple text",
                    0UL};

                solid_log(generic_logger, Verbose, "wp started");
                pwp = &wp;
                for (size_t i = 0; i < cnt; ++i) {
                    auto l = [i, &val](Context& _rctx) {
                        //this_thread::sleep_for(std::chrono::seconds(2));
                        ++_rctx.count_;
                        val += i;
                    };
                    wp.push(l);
                };
                pwp = nullptr;
            }
            solid_log(logger, Verbose, "after loop");
            solid_check(v == val, "val = " << val << " v = " << v);
            val = 0;
        }
    };
    solid_check(async(launch::async, lambda).wait_for(chrono::seconds(wait_seconds)) == future_status::ready, " Test is taking too long - waited " << wait_seconds << " secs");
    solid_log(logger, Verbose, "after async wait");

    return 0;
}
