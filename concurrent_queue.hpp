#pragma once

#include <queue>
#include <atomic>
#include <condition_variable>


namespace detail
{


// this type increments a counter when it is constructed
// and decrements it upon destruction
template<class T>
struct scope_bumper
{
  scope_bumper(std::atomic<T>& counter)
    : counter_(counter)
  {
    ++counter_;
  }

  ~scope_bumper()
  {
    --counter_;
  }

  std::atomic<T>& counter_;
};


template<class T>
void wait_until_equal(const std::atomic<T>& a, const T& value)
{
  // implement this with a spin loop
  while(a != value)
  {
    // spin
  }
}


enum queue_status
{
  open_and_empty = 0,
  open_and_ready = 1,
  closed = 2
};


template<class T>
class concurrent_queue
{
  public:
    concurrent_queue()
      : is_closed_(false),
        num_poppers_(0)
    {
    }

    ~concurrent_queue()
    {
      close();
    }

    void close()
    {
      {
        std::unique_lock<std::mutex> lock(mutex_);
        is_closed_ = true;
      }

      // wake everyone up
      wake_up_.notify_all();

      // wait until all the poppers have finished with wait_and_pop() 
      detail::wait_until_equal(num_poppers_, 0);
    }

    bool is_closed()
    {
      std::unique_lock<std::mutex> lock(mutex_);

      return is_closed_;
    }

    template<class... Args>
    queue_status emplace(Args&&... args)
    {
      {
        std::unique_lock<std::mutex> lock(mutex_);

        if(is_closed_)
        {
          return queue_status::closed;
        }

        items_.emplace(std::forward<Args>(args)...);
      }

      wake_up_.notify_one(); 

      return queue_status::open_and_ready;
    }

    queue_status push(const T& item)
    {
      return emplace(item);
    }

    // XXX this should return queue_status
    bool wait_and_pop(T& item)
    {
      scope_bumper<int> popping_(num_poppers_);

      while(true)
      {
        bool needs_notify = true;

        {
          std::unique_lock<std::mutex> lock(mutex_);
          wake_up_.wait(lock, [this]
          {
            return is_closed_ || !items_.empty();
          });

          // if the queue is closed, return
          if(is_closed_)
          {
            break;
          }

          // if there are no items go back to sleep
          if(items_.empty()) continue;

          // get the next item
          item = std::move(items_.front());
          items_.pop();

          needs_notify = !items_.empty();
        }

        // wake someone up
        if(needs_notify)
        {
          wake_up_.notify_one();
        }

        return true;
      }

      return false;
    }

  private:
    bool is_closed_;
    std::queue<T> items_;
    std::mutex mutex_;
    std::condition_variable wake_up_;
    std::atomic<int> num_poppers_;
};


} // end detail

