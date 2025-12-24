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

namespace actors
{
  class Actor;

  /**
   * Base class for all messages in the actor system
   *
   * Messages are the only way actors communicate.
   * Each message type has a unique ID (0-511 for optimal performance).
   */
  struct Message
  {
    virtual int get_message_id() const = 0;
    mutable Actor *sender = nullptr;
    mutable Actor *destination = nullptr;
    mutable bool is_fast = false;
    mutable bool last = false;

    Message() = default;

    Message(const Message& other)
      : sender(other.sender)
      , destination(nullptr)
      , is_fast(other.is_fast)
      , last(other.last)
    {}

    Message& operator=(const Message& other) {
      if (this != &other) {
        sender = other.sender;
        destination = nullptr;
        is_fast = other.is_fast;
        last = other.last;
      }
      return *this;
    }

    virtual ~Message() = default;
  };

  /**
   * Template for creating message types with a specific ID
   *
   * Usage:
   *   struct MyMessage : public actors::Message_N<100> {
   *     int data;
   *     MyMessage(int d) : data(d) {}
   *   };
   *
   * Use IDs 0-511 for best performance (handler cache optimization)
   */
  template <int N>
  struct Message_N : public Message
  {
    constexpr int get_message_id() const override { return N; }
  };
}

typedef actors::Message* msg_ptr;
typedef const actors::Message* const_msg_ptr;
