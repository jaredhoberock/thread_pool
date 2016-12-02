#pragma once

#include <stdexcept>
#include <cassert>
#include <utility>
#include <type_traits>
#include <memory>


namespace detail
{


template<class T, class Alloc, class Deleter, class... Args>
std::unique_ptr<T,Deleter> allocate_unique_with_deleter(const Alloc& alloc, const Deleter& deleter, Args&&... args)
{
  using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
  allocator_type alloc_copy = alloc;
  Deleter deleter_copy = deleter;

  std::unique_ptr<T,Deleter> result(alloc_copy.allocate(1), deleter_copy);

  std::allocator_traits<allocator_type>::construct(alloc_copy, result.get(), std::forward<Args>(args)...);

  return std::move(result);
}


class bad_function_call : public std::exception
{
  public:
    virtual const char* what() const noexcept
    {
      return "bad_function_call: unique_function has no target";
    }
};


template<class>
class unique_function;

template<class Result, class... Args>
class unique_function<Result(Args...)>
{
  public:
    using result_type = Result;

    unique_function() = default;

    unique_function(std::nullptr_t)
      : f_ptr_(nullptr)
    {}

    unique_function(unique_function&& other) = default;

    template<class Function>
    unique_function(Function&& f)
      : unique_function(std::allocator_arg, std::allocator<typename std::decay<Function>::type>{}, std::forward<Function>(f))
    {}

    template<class Alloc>
    unique_function(std::allocator_arg_t, const Alloc& alloc, std::nullptr_t)
      : unique_function(nullptr)
    {}

    template<class Alloc>
    unique_function(std::allocator_arg_t, const Alloc& alloc)
      : unique_function(std::allocator_arg, alloc, nullptr)
    {}

    template<class Alloc>
    unique_function(std::allocator_arg_t, const Alloc& alloc, unique_function&& other)
      : f_ptr_(std::move(other.f_ptr_))
    {}

    template<class Alloc, class Function>
    unique_function(std::allocator_arg_t, const Alloc& alloc, Function&& f)
      : f_ptr_(allocate_function_pointer(alloc, std::forward<Function>(f)))
    {}

    unique_function& operator=(unique_function&& other) = default;

    Result operator()(Args... args) const
    {
      if(!*this)
      {
        throw bad_function_call();
      }

      return (*f_ptr_)(args...);
    }

    operator bool () const
    {
      return static_cast<bool>(f_ptr_);
    }

  private:
    // this is the abstract base class for a type
    // which is both
    // 1. callable like a function and
    // 2. deallocates itself inside its destructor
    struct callable_self_deallocator_base
    {
      using self_deallocate_function_type = void(*)(callable_self_deallocator_base*);

      self_deallocate_function_type self_deallocate_function;

      template<class Function>
      callable_self_deallocator_base(Function callback)
        : self_deallocate_function(callback)
      {}

      virtual ~callable_self_deallocator_base()
      {
        self_deallocate_function(this);
      }

      virtual Result operator()(Args... args) const = 0;
    };

    template<class Function, class Alloc>
    struct callable : callable_self_deallocator_base
    {
      using super_t = callable_self_deallocator_base;
      using allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<callable>;

      mutable Function f_;

      ~callable() = default;

      template<class OtherFunction,
               class = typename std::enable_if<
                 std::is_constructible<Function,OtherFunction&&>::value
               >::type>
      callable(const Alloc& alloc, OtherFunction&& f)
        : super_t(deallocate),
          f_(std::forward<OtherFunction>(f))
      {}

      virtual Result operator()(Args... args) const
      {
        return f_(args...);
      }

      static void deallocate(callable_self_deallocator_base* ptr)
      {
        // upcast to the right type of pointer
        callable* self = static_cast<callable*>(ptr);

        // XXX seems like creating a new allocator here is cheating
        //     we should use some member allocator, but it's not clear where to put it
        allocator_type alloc_;
        alloc_.deallocate(self, 1);
      }
    };

    // this deleter calls the destructor of its argument but does not
    // deallocate the ptr
    // T will deallocate itself inside ~T()
    struct self_deallocator_deleter
    {
      template<class T>
      void operator()(T* ptr) const
      {
        ptr->~T();
      }
    };

    using function_pointer = std::unique_ptr<callable_self_deallocator_base, self_deallocator_deleter>;

    template<class Alloc, class Function>
    static function_pointer allocate_function_pointer(const Alloc& alloc, Function&& f)
    {
      using concrete_function_type = callable<typename std::decay<Function>::type, Alloc>;
      return allocate_unique_with_deleter<concrete_function_type>(alloc, self_deallocator_deleter(), alloc, std::forward<Function>(f));
    }

    function_pointer f_ptr_; 
};


} // end detail

