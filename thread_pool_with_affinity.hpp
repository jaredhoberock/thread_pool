#pragma once

#include <vector>
#include <thread>
#include <set>
#include <algorithm>
#include <iterator>
#include <random>
#include <chrono>

#include <sched.h>
#include <pthread.h>

#include "thread_pool.hpp"

using core_id = int;

namespace this_core
{


inline core_id get_id()
{
  return sched_getcpu();
}


}


class thread_pool_with_affinity
{
  public:
    inline thread_pool_with_affinity()
      : rng_(std::chrono::system_clock::now().time_since_epoch().count())
    {
      // for each cpu, create a thread pool with a single thread and pin it to its core
      for(int cpu = 0; cpu < std::thread::hardware_concurrency(); ++cpu)
      {
        // create a thread pool with a single thread
        pools_.emplace_back(1);

        // pin that thread to the given cpu
        pools_.back().async([=]
        {
          cpu_set_t cpu_set;
          CPU_ZERO(&cpu_set);
          CPU_SET(cpu, &cpu_set);

          pthread_setaffinity_np(pthread_self(), 1, &cpu_set);
        }).wait();
      }
    }

    using affinity = std::set<core_id>;

    template<class Function>
    void submit(const affinity& where, Function&& f)
    {
      thread_pool& pool = select_pool(where);
      pool.submit(std::forward<Function>(f));
    }

    inline affinity anywhere() const
    {
      affinity result;

      for(core_id id = 0; id < pools_.size(); ++id)
      {
        result.insert(id);
      }

      return result;
    }

    template<class Function>
    void submit(Function&& f)
    {
      return submit(anywhere(), std::forward<Function>(f));
    }

    class executor_type
    {
      public:
        executor_type(const executor_type&) = default;

        template<class Function>
        void execute(Function&& f) const
        {
          thread_pool_->submit(where_, std::forward<Function>(f));
        }

        template<class Function>
        std::future<typename std::result_of<Function()>::type>
          async_execute(Function&& f) const
        {
          using result_type = typename std::result_of<Function()>::type;

          // create a packaged task
          std::packaged_task<result_type()> task(std::move(f));

          // get the packaged task's future so we can return it at the end
          auto result_future = task.get_future();

          // move the packaged task into the thread pool
          thread_pool_->submit(where_, std::move(task));

          return std::move(result_future);
        }
    
      private:
        friend class thread_pool_with_affinity;

        inline executor_type(thread_pool_with_affinity& pool, const affinity& where)
          : thread_pool_(&pool),
            where_(where)
        {}

        thread_pool_with_affinity* thread_pool_;
        affinity where_;
    };

    inline executor_type executor(const affinity& where)
    {
      return executor_type(*this, where);
    }

    inline executor_type executor()
    {
      return executor(anywhere());
    }

  private:
    inline thread_pool& select_pool(const affinity& choices)
    {
      // choose a thread pool at random
      unsigned int selection = rng_() % choices.size();

      auto iter = choices.begin();
      std::advance(iter, selection);

      return pools_[*iter];
    }

    std::vector<thread_pool> pools_;
    std::default_random_engine rng_;
};

