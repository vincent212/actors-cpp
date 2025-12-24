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

#include "actors/msg/Start.hpp"
#include "actors/msg/Shutdown.hpp"
#include "actors/act/Group.hpp"

using namespace actors;

void Group::add(actor_ptr a)
{
  assert(a != nullptr && "adding null actor");
  a->set_group(this);
  members.push_back(a);
  name_to_actor[a->get_name()] = a;
  MESSAGE_HANDLER(actors::msg::Start, start_handler);
  MESSAGE_HANDLER(actors::msg::Shutdown, shutdown_handler);
}

void Group::start_handler(const actors::msg::Start *m)
{
  if (m->sender != this)
  {
    for (auto a : members)
    {
      assert(a != nullptr && "null actor in group");
      std::cout << get_name() << " Group::start_handler sending start to "
                << a->get_name() << std::endl;
      a->init();
      a->fast_send(new msg::Start(), this);
    }
  }
  else
  {
    forward(m);
  }
}

void Group::shutdown_handler(const actors::msg::Shutdown *m)
{
  if (m->sender != this)
  {
    for (auto a : members)
    {
      a->fast_send(new msg::Shutdown(), this);
      a->end();
    }
  }
  else
  {
    forward(m);
  }
}

void Group::process_message(const Message *m)
{
  forward(m);
}

void Group::forward(const Message *m)
{
  assert(!m->is_fast && "cannot fast send here");
  m->destination->reply_to = m->sender;
  m->destination->process_message_internal(m, true);
}
