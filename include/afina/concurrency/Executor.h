#ifndef AFINA_CONCURRENCY_EXECUTOR_H
#define AFINA_CONCURRENCY_EXECUTOR_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

namespace Afina {
namespace Concurrency {
class Executor;
void perform(Afina::Concurrency::Executor *executor);
/**
 * # Thread pool
 */
class Executor {
public:
    enum class State {
        // Threadpool is fully operational, tasks could be added and get executed
        kRun,

        // Threadpool is on the way to be shutdown, no ned task could be added, but existing will be
        // completed as requested
        kStopping,

        // Threadppol is stopped
        kStopped
    };

    Executor(std::string name, std::size_t size, std::size_t high = 6, std::size_t low = 0, std::size_t timeout = 100)
        : _max_queue_size(size), _high_watermark(high), _low_watermark(low), _idle_time(timeout), _free_threads(0),
          _count_threads(0) {}
    
    ~Executor(){
        std::unique_lock<std::mutex> lock(mutex);
        if (state == State::kRun) {
            Stop(true);
        }
    }

    /**
     * Signal thread pool to stop, it will stop accepting new jobs and close threads just after each become
     * free. All enqueued jobs will be complete.
     *
     * In case if await flag is true, call won't return until all background jobs are done and all threads are stopped
     */
    void Stop(bool await = false);

    void Start(); // add threads

private:
    // No copy/move/assign allowed
    Executor(const Executor &);            // = delete;
    Executor(Executor &&);                 // = delete;
    Executor &operator=(const Executor &); // = delete;
    Executor &operator=(Executor &&);      // = delete;

    /**
     * Main function that all pool threads are running. It polls internal task queue and execute tasks
     */
    friend void perform(Executor *executor);

    /**
     * Mutex to protect state below from concurrent modification
     */
    std::mutex mutex;

    /**
     * Conditional variable to await new data in case of empty queue
     */
    std::condition_variable empty_condition;
    std::condition_variable stop_condition;// wait in Stop

    /**
     * Vector of actual threads that perorm execution
     */
    //std::vector<std::thread> threads;
    /*
    * Count of threads
    */
    std::size_t _count_threads;

    /**
    * Count of free threads
    */
    std::size_t _free_threads;
    /**
     * Task queue
     */
    std::deque<std::function<void()>> tasks;

    /**
     * Flag to stop bg threads
     */
    State state;
    /**
     * Some features of Executor
    */
    std::size_t _max_queue_size;
    std::size_t _high_watermark;
    std::size_t _low_watermark;
    std::size_t _idle_time;
public:
    /**
     * Add function to be executed on the threadpool. Method returns true in case if task has been placed
     * onto execution queue, i.e scheduled for execution and false otherwise.
     *
     * That function doesn't wait for function result. Function could always be written in a way to notify caller about
     * execution finished by itself
     */
    template <typename F, typename... Types> bool Execute(F &&func, Types... args) {
        // Prepare "task"
        auto exec = std::bind(std::forward<F>(func), std::forward<Types>(args)...);

        std::unique_lock<std::mutex> lock(this->mutex);
        if (state != State::kRun) {
            return false;
        }
        // count
        if (tasks.size() > _max_queue_size){
            return false;
        }
        // can add thread and no free thread
        if (_free_threads == 0 && _count_threads <_high_watermark){
            std::thread t(&(perform), this);
            t.detach();
            _count_threads ++;
        }
        // Enqueue new task
        tasks.push_back(exec);
        empty_condition.notify_one();
        return true;
    }
};

} // namespace Concurrency
} // namespace Afina
#endif // AFINA_CONCURRENCY_EXECUTOR_H