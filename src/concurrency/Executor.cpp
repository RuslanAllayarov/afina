#include <afina/concurrency/Executor.h>

namespace Afina {
namespace Concurrency {

void Executor::Start() {
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kRun;
    for (int i = 0; i < low_watermark; i++) {
        std::thread t(&(perform), this);
        threads.insert(std::move(std::make_pair(t.get_id(), std::move(t))));
    }
}

void Executor::Stop(bool await) {
    if (state == State::kStopped) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex);
    state = State::kStopping;
    while (tasks.size() > 0){
        empty_condition.notify_all();
    }
    if (await) {
        while (state == State::kStopping){
            stop_condition.wait(lock);
        }
    }
    state = State::kStopped;
}


void perform(Executor *executor) {
    // new thread
    while (executor->state == Executor::State::kRun) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(executor->mutex);
            auto time_until = std::chrono::system_clock::now() + std::chrono::milliseconds(executor->idle_time);
            while (executor->tasks.empty() && executor->state == Executor::State::kRun) {
                // waiting
                executor->free_threads++;
                if (executor->empty_condition.wait_until(lock, time_until) == std::cv_status::timeout) {
                    if (executor->threads.size() > executor->low_watermark) {
                        executor->_erase_thread();
                        return;
                    } else {
                        executor->empty_condition.wait(lock);
                    }
                }
                executor->free_threads--;
            }
            // stop waiting
            if (executor->tasks.empty()) {
                continue;
            }
            task = executor->tasks.front();
            executor->tasks.pop_front();
        }
        task();
    }
    {
        std::unique_lock<std::mutex> lock(executor->mutex);
        executor->_erase_thread();
        if (executor->threads.empty()) {
            executor->stop_condition.notify_all();
        }
    }
}

void Executor::_erase_thread() {
    std::thread::id cur_thread_id = std::this_thread::get_id();
    auto it = threads.find(std::this_thread::get_id());
    if (iter != threads.end()) {
        //iter->detach();
        free_threads--;
        threads.erase(iter);
        return;
    }
    throw std::runtime_error("error while erasing thread");
}



}
} // namespace Afina
