/*

THIS SOFTWARE IS OPEN SOURCE UNDER THE MIT LICENSE

Copyright 2025 Vincent Maciejewski,  & M2 Tech
Contact:
v@m2te.ch
mayeski@gmail.com
https://www.linkedin.com/in/vmayeski/
http://m2te.ch/

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

https://opensource.org/licenses/MIT

*/

#pragma once

#include <mutex>
#include <condition_variable>
#include <boost/circular_buffer.hpp>
#include <deque>
#include <tuple>
#include "actors/Queue.hpp"
#include <type_traits>

namespace actors
{
  /**
   * BQueue - Blocking Queue
   *
   * Uses condition variables for efficient waiting.
   * Low CPU usage when idle.
   */
  template <class T>
  class BQueue : public Queue<T>
  {
  private:
    mutable std::mutex mut;
    mutable std::condition_variable cv;
    boost::circular_buffer<T> cb_;
    std::deque<T> overflow_;

  public:
    explicit BQueue(size_t n) : cb_(n) {}

    std::tuple<T, bool> pop() noexcept override
    {
      std::unique_lock<std::mutex> lock(mut);
      cv.wait(lock, [this]() {
        return !cb_.empty() || !overflow_.empty();
      });

      T ret;
      if (!cb_.empty()) {
        ret = cb_.front();
        cb_.pop_front();
      } else {
        ret = overflow_.front();
        overflow_.pop_front();
      }
      bool last = cb_.empty() && overflow_.empty();
      return std::make_tuple(ret, last);
    }

    T peek() const noexcept override
    {
      std::lock_guard<std::mutex> lock(mut);
      if (cb_.empty() && overflow_.empty()) {
        if constexpr (std::is_pointer<T>::value)
          return nullptr;
        else
          return T{};
      }
      return !cb_.empty() ? cb_.front() : overflow_.front();
    }

    void push(const T& x) noexcept override
    {
      {
        std::lock_guard<std::mutex> lock(mut);
        if (!overflow_.empty() || cb_.full()) {
          overflow_.push_back(x);
        } else {
          cb_.push_back(x);
        }
      }
      cv.notify_one();
    }

    bool is_empty() const noexcept override
    {
      std::lock_guard<std::mutex> lock(mut);
      return cb_.empty() && overflow_.empty();
    }

    std::size_t length() const noexcept override
    {
      std::lock_guard<std::mutex> lock(mut);
      return cb_.size() + overflow_.size();
    }
  };
}
