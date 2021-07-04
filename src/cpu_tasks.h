#pragma once
#include <memory>

#include <ftl/task_scheduler.h>
#include <ftl/task_counter.h>

namespace zec
{
    class TaskScheduler;

    typedef void (*TaskFunction)(TaskScheduler* taskScheduler, void* arg);

    struct Task
    {
        TaskFunction function;
        void* arg;
    };

    class TaskCounter
    {
    public:
        friend class TaskScheduler;

        TaskCounter() = default;
        TaskCounter(TaskScheduler& task_scheduler);
    private:
        std::unique_ptr<ftl::TaskCounter> internal_counter = nullptr;
    };

    class TaskScheduler
    {
    public:
        friend class TaskCounter;
                
        // call this only from the Window thread!
        void init();

        void add_tasks(const size_t num_tasks, Task* tasks, TaskCounter& counter);

        void wait_on_counter(TaskCounter& task_counter);
    
    private:

        DWORD window_thread_id = UINT32_MAX;
        std::unique_ptr<ftl::TaskScheduler> task_scheduler = nullptr;
    };

}

