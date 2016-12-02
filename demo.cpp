#include "thread_pool.hpp"
#include <iostream>

int main()
{
  thread_pool pool;

  auto f = pool.async([]
  {
    std::cout << "Hello, world!" << std::endl;
  });

  f.wait();

  return 0;
}

