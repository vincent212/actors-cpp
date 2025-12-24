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
#include <map>
#include <string>
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include "actors/Actor.hpp"
#include "actors/act/Group.hpp"
#include "actors/msg/Start.hpp"
#include "actors/msg/Shutdown.hpp"
#include "actors/act/Manager.hpp"

using namespace actors;
using namespace std;

static int set_thread_affinity(set<int> core_ids, pthread_t thread)
{
  if (core_ids.empty())
    return 0;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

  for (auto core_id : core_ids)
  {
    if (core_id < 0 || core_id >= num_cores)
    {
      cerr << "bad core id: " << core_id << endl;
      return EINVAL;
    }
    CPU_SET(core_id, &cpuset);
  }

  auto rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
  return rc;
}

Manager::Manager() {}

Manager::~Manager()
{
  for (auto p : thread_list)
    delete p;
}

void Manager::init()
{
  for (auto actor : actor_list)
  {
    auto initmsg = new actors::msg::Start();
    cout << "Manager::init sending start to " << actor->get_name() << endl;
    actor->fast_send(initmsg, nullptr);
  }

  for (auto actor : actor_list)
  {
    auto t = new std::thread([actor]() { (*actor)(); });
    thread_list.push_back(t);

    if (!actor->affinity.empty())
    {
      cout << actor->get_name() << " setting affinity" << endl;
      if (set_thread_affinity(actor->affinity, t->native_handle()) != 0)
      {
        perror("could not assign affinity\n");
      }
    }

    if (actor->priority > 0)
    {
      cout << actor->get_name() << " setting priority to SCHED_FIFO " << actor->priority << endl;
      struct sched_param sp;
      sp.sched_priority = actor->priority;
      if (pthread_setschedparam(t->native_handle(), SCHED_FIFO, &sp) != 0)
      {
        perror("sched_setscheduler");
        cerr << "could not set priority for " << actor->get_name() << endl;
      }
      else
        cout << " priority set ok\n";
    }
    else
    {
      cout << actor->get_name() << " NOT setting priority " << actor->priority << endl;
    }
  }

  this->send(new msg::Start());
}

void Manager::end()
{
  for (auto t : thread_list)
  {
    if (t->joinable())
      t->join();
  }
}

void Manager::process_message(const Message *m)
{
  if (typeid(*m) == typeid(actors::msg::Start))
  {
    // Manager started
  }
  else if (typeid(*m) == typeid(actors::msg::Shutdown))
  {
    for (auto actor : actor_list)
    {
      actor->end();
      actor->fast_terminate();
      actor->terminated = true;
    }
    exit(0);
  }
}

void Manager::manage(actor_ptr actor, set<int> affinity, int priority, int priority_type)
{
  assert(actor != nullptr && "cannot manage null actor");

  if (actor->is_managed || managed_name_map.find(actor->get_name()) != managed_name_map.end())
  {
    cout << "actors already managed:\n";
    for (const auto &p : managed_name_map)
    {
      cout << p.first << endl;
    }
    assert(false && "actor with this name already managed");
  }

  if (expanded_name_map.find(actor->get_name()) != expanded_name_map.end())
  {
    assert(false && "actor cannot be managed because it's part of a group that was already managed");
  }

  // Check affinity
  for (auto core_id : affinity)
  {
    if (core_id < 0 || core_id >= sysconf(_SC_NPROCESSORS_ONLN))
    {
      cerr << "bad core id: " << core_id << endl;
      assert(false && "core id out of range");
    }
  }

  managed_name_map[actor->get_name()] = actor;
  expanded_name_map[actor->get_name()] = actor;

  actor->set_manager(this);
  if (actor->is_group())
  {
    Group *g = static_cast<Group *>(actor);
    for (auto a : g->members)
      a->set_manager(this);
  }

  actor_list.push_back(actor);

  if (actor->is_group())
  {
    auto g = static_cast<Group *>(actor);
    assert(!g->name_to_actor.empty() && "add actors to group before managing group");

    for (auto it = g->name_to_actor.begin(); it != g->name_to_actor.end(); ++it)
    {
      auto it2 = expanded_name_map.find(it->first);
      assert(it2 == expanded_name_map.end() && "actor (part of a group) already managed somewhere else");
      expanded_name_map[it->first] = it->second;
    }
  }

  actor->is_managed = true;
  actor->affinity = affinity;
  actor->priority = priority;
  actor->priority_type = priority_type;
}

map<string, size_t> Manager::get_queue_lengths() const noexcept
{
  map<string, size_t> ret;
  for (auto &[name, actor] : managed_name_map)
  {
    ret[name] = actor->queue_length();
  }
  return ret;
}

map<string, tuple<pid_t, int>> Manager::get_message_counts() const noexcept
{
  map<string, tuple<pid_t, int>> ret;
  for (auto &[name, actor] : managed_name_map)
    ret[name] = make_tuple(actor->tid, int(actor->msg_cnt));
  return ret;
}

list<string> Manager::get_managed_names() const noexcept
{
  list<string> ret;
  for (auto &[name, _] : expanded_name_map)
    ret.push_back(name);
  return ret;
}

actor_ptr Manager::get_actor_by_name(const string &name) const noexcept
{
  for (auto actor : actor_list)
  {
    if (actor->get_name() == name)
      return actor;
    else if (actor->is_group())
    {
      Group *g = static_cast<Group *>(actor);
      for (auto a : g->members)
      {
        if (a->get_name() == name)
          return a;
      }
    }
  }
  return nullptr;
}

size_t Manager::total_queue_length()
{
  size_t total = 0;
  for (auto actor : actor_list)
  {
    total += actor->queue_length();
  }
  return total;
}
