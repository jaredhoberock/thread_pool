#include <thread>
#include <iostream>

#include "thread_pool.hpp"
#include "thread_pool_with_affinity.hpp"

int main()
{
  {
    thread_pool pool;

    auto f = pool.executor().async_execute([]
    {
      std::cout << "Hello, world!" << std::endl;
    });

    f.wait();
  }

  {
    thread_pool_with_affinity pool;

    // execute this task on any core
    auto anywhere_executor = pool.executor();

    auto f1 = anywhere_executor.async_execute([]
    {
      std::cout << "Hello, world!" << std::endl;
    });

    std::cout << "calling f1.wait()" << std::endl;

    f1.wait();

    // execute this task on either core 2 or 3
    auto cores_two_or_three = pool.executor({2,3});

    auto f2 = cores_two_or_three.async_execute([]
    {
      std::cout << "Hello, world from core " << this_core::get_id() << std::endl;
    });

    f2.wait();
  }

  return 0;
}

