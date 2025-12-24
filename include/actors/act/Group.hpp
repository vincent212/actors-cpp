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

#include <list>
#include <map>
#include <string>
#include <cstring>
#include "actors/Actor.hpp"
#include "actors/msg/Start.hpp"
#include "actors/msg/Shutdown.hpp"

namespace actors
{
  /**
   * Group - Run multiple actors in a single thread
   *
   * Use when you have lightweight actors that don't need
   * separate threads. All actors in a group share one thread
   * and process messages sequentially.
   *
   * Usage:
   *   actors::Group grp("my_group");
   *   grp.add(new LightActor1());
   *   grp.add(new LightActor2());
   *   mgr.manage(&grp);  // All run in single thread
   */
  class Group : public Actor
  {
    friend class Manager;

    char name[256];
    std::list<actor_ptr> members;
    std::map<std::string, actor_ptr> name_to_actor;

  public:
    explicit Group(const std::string& group_name)
      : Actor()
    {
      strncpy(name, group_name.c_str(), sizeof(name) - 1);
      name[sizeof(name) - 1] = '\0';
    }

    virtual ~Group() = default;

    bool is_group() const override { return true; }

    void add(actor_ptr actor);

  protected:
    void process_message(const Message* m) override;
    void init() override {}
    void end() override {}

    const char* get_name() const override { return name; }

    void start_handler(const msg::Start* m);
    void shutdown_handler(const msg::Shutdown* m);
    void forward(const Message* m);
  };
}
