#pragma once

#include "concurrent_queue.hpp"
#include "unique_function.hpp"

#include <thread>
#include <algorithm>
#include <vector>
#include <memory>
#include <future>
#include <type_traits>


class thread_pool
{
  private:
    struct joining_thread : std::thread
    {
      using std::thread::thread;

      joining_thread(joining_thread&&) = default;

      ~joining_thread()
      {
        if(joinable()) join();
      }
    };

  public:
    explicit thread_pool(size_t num_threads = std::max(1u, std::thread::hardware_concurrency()))
    {
      for(size_t i = 0; i < num_threads; ++i)
      {
        threads_.emplace_back([this]
        {
          work();
        });
      }
    }
    
    ~thread_pool()
    {
      tasks_.close();
      threads_.clear();
    }

    template<class Function,
             class = typename std::result_of<Function()>>
    inline void submit(Function&& f)
    {
      auto is_this_thread = [=](const joining_thread& t)
      {
        return t.get_id() == std::this_thread::get_id();
      };

      // guard against self-submission which may result in deadlock
      // XXX it might be faster to compare this to a thread_local variable
      if(std::find_if(threads_.begin(), threads_.end(), is_this_thread) == threads_.end())
      {
        tasks_.emplace(std::forward<Function>(f));
      }
      else
      {
        // the submitting thread is part of this pool so execute immediately 
        std::forward<Function>(f)();
      }
    }

    inline size_t size() const
    {
      return threads_.size();
    }

    template<class Function, class... Args>
    std::future<typename std::result_of<Function(Args...)>::type>
      async(Function&& f, Args&&... args)
    {
      // bind f & args together
      auto g = std::bind(std::forward<Function>(f), std::forward<Args>(args)...);

      using result_type = typename std::result_of<Function(Args...)>::type;

      // create a packaged task
      std::packaged_task<result_type()> task(std::move(g));

      // get the packaged task's future so we can return it at the end
      auto result_future = task.get_future();

      // move the packaged task into the thread pool
      submit(std::move(task));

      return std::move(result_future);
    }


  private:
    inline void work()
    {
      detail::unique_function<void()> task;

      while(tasks_.wait_and_pop(task))
      {
        task();
      }
    }

    detail::concurrent_queue<detail::unique_function<void()>> tasks_;
    std::vector<joining_thread> threads_;
};


class thread_pool_executor
{
  public:
    thread_pool_executor(thread_pool& pool)
      : thread_pool_(pool)
    {}

    template<class Function>
    void execute(Function&& f)
    {
      thread_pool_.submit(std::forward<Function>(f));
    }

  private:
    thread_pool& thread_pool_;
};

