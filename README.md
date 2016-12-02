# thread_pool
A demonstration of the relationship between thread pools and executors.

An executor is a light-weight view of heavy-weight thread pool:

    thread_pool pool;
    
    thread_pool::executor_type executor = pool.executor();
    
Unlike the thread pool, the executor only needs to persist as long as it takes to launch a task:

    thread_pool pool;
    
    // get an executor from the thread pool and use it to execute a task
    // the executor is a temporary object
    pool.executor().execute([]
    {
      std::cout << "Hello, world!" << std::endl;
    });

We can use special types of executors to encode task affinity:

    thread_pool_with_affinity pool;
    
    // get an executor from the thread pool with affinity to CPU cores 2 and 3:
    auto cores_two_and_three = pool.executor({2,3});
    
    // create a task which is allowed to execute on either CPU cores 2 or 3, but nowhere else
    cores_two_and_three.execute([]
    {
      std::cout << "Hello, world from core " << sched_getcpu() << std::endl;
    });
