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

    auto f1 = pool.async([]
    {
      std::cout << "Hello, world!" << std::endl;
    });

    std::cout << "calling f1.wait()" << std::endl;

    f1.wait();

    auto f2 = pool.async({2,3}, []
    {
      std::cout << "Hello, world from core " << this_core::get_id() << std::endl;
    });

    f2.wait();
  }

  return 0;
}

