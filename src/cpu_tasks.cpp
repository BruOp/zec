#include "cpu_tasks.h"

namespace zec
{
    TaskCounter::TaskCounter(TaskScheduler& task_scheduler) : internal_counter{ std::make_unique<ftl::TaskCounter>(task_scheduler.task_scheduler.get()) }
    { };

    void TaskScheduler::init()
    {
        if (task_scheduler == nullptr) {
            task_scheduler = std::make_unique<ftl::TaskScheduler>();
            task_scheduler->Init();

            window_thread_id = ftl::GetCurrentThread().Id;
        }
        else {
            throw std::runtime_error("Cannot initialize task scheduler twice!");
        }
    }

    void TaskScheduler::add_tasks(const size_t num_tasks, Task* tasks, TaskCounter& counter)
    {
        task_scheduler->AddTasks(num_tasks, reinterpret_cast<ftl::Task*>(tasks), ftl::TaskPriority::High, counter.internal_counter.get());
    }

    void TaskScheduler::wait_on_counter(TaskCounter& task_counter)
    {
        // This ensures our window doesn't wander between threads
        bool pin_to_current_thread = (ftl::GetCurrentThread().Id == window_thread_id);
        task_scheduler->WaitForCounter(task_counter.internal_counter.get(), pin_to_current_thread);
    }
}