#include <afina/concurrency/Executor.h>
#include <algorithm>
#include <iostream>

namespace Afina {
namespace Concurrency {
void Executor::Start() {
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kRun;
    for (size_t i = 0; i < _low_watermark; i++) {
        std::thread t(&(perform), this);
        t.detach();
        _count_threads++;
    }
}

void Executor::Stop(bool await) {
    std::unique_lock<std::mutex> lock(mutex);
    if (state == State::kRun) {
        state = State::kStopping;
    }
    if (state == State::kStopping){
        // await == True -> wait
        if (await && _count_threads > 0) {
            stop_condition.wait(lock, [&]() { return _count_threads == 0; });
        }
        // if law_watermark == 0
        else if (_count_threads == 0) {
            state = State::kStopped;
        }
    }
}

void perform(Afina::Concurrency::Executor *ex) {
    std::function<void()> task;
    //have -> task.empty()
    bool have;
    while (true){
        {
            std::unique_lock<std::mutex> lock(ex->mutex);
            if (ex->state != Executor::State::kRun){
                break;
            }
            ex->_free_threads++;
            auto time = std::chrono::system_clock::now() + std::chrono::milliseconds(ex->_idle_time);
            if (!ex->empty_condition.wait_until(lock, time, [&]() { return ex->tasks.empty() == 0; })) {
                // no tasks
                if (ex->_count_threads < ex->_low_watermark) {
                    // wait new tasks
                } else {
                    ex->_free_threads--;
                    break;
                }
                have = false;
            } 
            else {
                task = (ex->tasks.front());
                ex->tasks.pop_front();
                have = true;
            }
            ex->_free_threads--;
        }
        if (have) {
            try {
                task();
            } catch (...) {
                std::terminate();
            }
        }
        have = false;
    }

    // check on Stop and --_count_threads
    {
        std::unique_lock<std::mutex> lock(ex->mutex);
        ex->_count_threads--;
        if (ex->state == Executor::State::kStopping && ex->_count_threads == 0) {
            ex->state = Executor::State::kStopped;
            ex->stop_condition.notify_all();
        }
    }
}
} // namespace Concurrency
} // namespace Afina