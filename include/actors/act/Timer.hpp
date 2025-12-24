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

#include <thread>
#include <chrono>
#include <functional>
#include "actors/msg/Timeout.hpp"
#include "actors/Actor.hpp"

namespace actors
{
  /**
   * Timer - Simple timer utility for actors
   *
   * Usage:
   *   Timer::wake_up_in(my_actor, 5, 0);  // Wake up in 5 seconds
   *   Timer::wake_up_at(my_actor, 1000);  // Wake up at next 1-second boundary
   */
  class Timer
  {
  public:
    /// Wake up actor after specified delay
    static void wake_up_in(Actor* subscriber, int seconds, int msecs = 0, int data = 0)
    {
      std::thread([=]() {
        sleep(seconds, msecs);
        subscriber->send(new msg::Timeout(data), nullptr);
      }).detach();
    }

    /// Wake up actor at next interval boundary (e.g., every 1000ms)
    static void wake_up_at(Actor* subscriber, int interval_ms, int data = 0)
    {
      using namespace std::chrono;
      auto now = system_clock::now();
      auto today = floor<days>(now);
      auto time_since_midnight = duration_cast<milliseconds>(now - today);
      auto curr_ms = time_since_midnight.count();
      auto rounded_down = curr_ms - (curr_ms % interval_ms);
      auto next_timeout = rounded_down + interval_ms;
      auto time_to_wait = next_timeout - curr_ms;

      std::thread([=]() {
        sleep(0, int(time_to_wait));
        subscriber->send(new msg::Timeout(data), nullptr);
      }).detach();
    }

    static void sleep(int seconds, int msecs = 0)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(seconds * 1000 + msecs));
    }

  private:
    Timer() = delete;
  };
}
