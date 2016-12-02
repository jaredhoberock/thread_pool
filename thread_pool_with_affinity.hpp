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


static core_id get_id()
{
  return sched_getcpu();
}


}


class thread_pool_with_affinity
{
  public:
    thread_pool_with_affinity()
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

    affinity anywhere() const
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

    // XXX the executor should deal with this rather than reimplementing it in every thread pool type
    template<class Function, class... Args>
    std::future<typename std::result_of<Function(Args...)>::type>
      async(const affinity& where, Function&& f, Args&&... args)
    {
      // bind f & args together
      auto g = std::bind(std::forward<Function>(f), std::forward<Args>(args)...);

      using result_type = typename std::result_of<Function(Args...)>::type;

      // create a packaged task
      std::packaged_task<result_type()> task(std::move(g));

      // get the packaged task's future so we can return it at the end
      auto result_future = task.get_future();

      // move the packaged task into the thread pool
      submit(where, std::move(task));

      return std::move(result_future);
    }

    // XXX the executor should deal with this rather than reimplementing it in every thread pool type
    template<class Function, class... Args>
    std::future<typename std::result_of<Function(Args...)>::type>
      async(Function&& f, Args&&... args)
    {
      return async(anywhere(), std::forward<Function>(f), std::forward<Args>(args)...);
    }


  private:
    thread_pool& select_pool(const affinity& choices)
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

