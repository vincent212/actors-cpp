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

#include <list>
#include <string>
#include <typeinfo>
#include <exception>
#include <memory>
#include <iostream>
#include <cassert>
#include <thread>
#include "actors/Queue.hpp"
#include "actors/BQueue.hpp"
#include "actors/msg/Shutdown.hpp"
#include "actors/Actor.hpp"

#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

using namespace std;
using namespace actors;

Actor::~Actor()
{
  delete msgq;
}

Actor::Actor()
{
  msgq = new BQueue<const Message *>(ACTOR_BQUEUE_SIZE);
  handler_cache.resize(ACTOR_HANDLER_CACHE_SIZE, nullptr);
  dont_have_handler.resize(ACTOR_HANDLER_CACHE_SIZE, false);

  // Initialize name with typeid
  const char* type_name = typeid(*this).name();
  strncpy(name, type_name, sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
}

void Actor::send(const Message *m, Actor *sender) noexcept
{
  assert(this != nullptr && "send to null actor");

  if (terminated)
    return;

  assert(m != nullptr && "null message");
  assert(m->destination == nullptr && "cannot reuse message");

  m->is_fast = false;
  m->last = false;
  m->sender = sender;
  m->destination = this;

  if (is_part_of_group) {
    group->add_message_to_queue(m);
  } else {
    add_message_to_queue(m);
  }
}

bool Actor::call_handler(const Message *m) noexcept
{
  auto id = m->get_message_id();
  auto f0 = handler_cache[id];
  if (f0) {
    (this->*f0)(m);
    return true;
  }
  if (dont_have_handler[id]) {
    return false;
  }
  auto midx = std::type_index(typeid(*m));
  auto p = handlers.find(midx);
  if (p == handlers.end()) {
    dont_have_handler[id] = true;
    return false;
  }
  auto f = p->second;
  (this->*f)(m);
  handler_cache[id] = f;
  return true;
}

void Actor::process_message_internal(const Message *m, bool dontdel) noexcept
{
  std::lock_guard<std::mutex> lock(fast_send_mutex);
  assert(this != nullptr && "no actor to handle message");

  msg_cnt++;
  using_fast_send = false;

  bool called = call_handler(m);
  if (!called)
    process_message(m);

  if (!dontdel) {
    delete m;
  }
}

std::unique_ptr<const Message> Actor::fast_send(const Message *m, Actor *sender) noexcept
{
  std::lock_guard<std::mutex> lock(fast_send_mutex);

  assert(this != nullptr && "fast send to null actor");
  assert(m != nullptr && "fast send with no message");
  assert(this != sender && "fast send to itself");

  m->sender = sender;
  m->is_fast = true;
  m->last = true;
  reply_message = nullptr;
  using_fast_send = true;
  msg_cnt++;

  if (terminated)
    return std::unique_ptr<const Message>(reply_message);

  bool called = call_handler(m);
  if (!called)
    process_message(m);

  return std::unique_ptr<const Message>(reply_message);
}

void Actor::operator()() noexcept
{
  tid = syscall(SYS_gettid);
  std::cerr << endl << get_name() << " tid: " << tid << endl;
  init();

  while (true) {
    auto r = msgq->pop();
    auto *m = std::get<0>(r);
    auto last = std::get<1>(r);
    m->last = last;
    reply_to = m->sender;

    bool is_shutdown = m->get_message_id() == 5;

    process_message_internal(m);

    if (is_shutdown || terminated) {
      break;
    }
  }

  terminated = true;
  end();
}

void Actor::reply(const Message *m) noexcept
{
  if (using_fast_send) {
    m->sender = this;
    reply_message = m;
  } else {
    assert(reply_to != nullptr && "no return address");
    reply_to->send(m, this);
  }
}

void Actor::terminate() noexcept
{
  terminate_called = true;
  this->send(new msg::Shutdown());
  sleep(3);
}

void Actor::fast_terminate() noexcept
{
  terminate_called = true;
  this->fast_send(new msg::Shutdown(), nullptr);
}

void Actor::add_message_to_queue(const Message *m)
{
  msgq->push(m);
}

std::size_t Actor::queue_length() const noexcept
{
  return msgq->length();
}

const Message* Actor::peek() const
{
  return msgq->peek();
}

void Actor::set_group(Actor *pgroup)
{
  is_part_of_group = true;
  group = pgroup;
}

Actor* Actor::get_group() const
{
  assert(is_part_of_group && "not part of group");
  return group;
}
