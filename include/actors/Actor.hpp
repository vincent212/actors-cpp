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

#include <typeinfo>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include "actors/Message.hpp"
#include <mutex>
#include <typeindex>
#include <atomic>
#include <cstring>
#include <cassert>

#define ACTOR_BQUEUE_SIZE 64
#define ACTOR_HANDLER_CACHE_SIZE 512

// Register a message handler for this actor
// Usage: MESSAGE_HANDLER(MessageType, handler_method)
#define MESSAGE_HANDLER(message_type, function_name)                            \
  {                                                                             \
    typedef typename std::remove_reference<decltype(*this)>::type ActorT;      \
    actors::register_handler<ActorT, message_type>(this)(&ActorT::function_name); \
  }

namespace actors
{
  class Actor;
  class Manager;
  class Group;
}

// Pointer to an Actor
typedef actors::Actor* actor_ptr;

namespace actors
{

  typedef void (Actor::*generic_handler_t)(const Message *);
  template <class T> class Queue;

  /**
   * Actor - Base class for all actors in the system
   *
   * An Actor is an independent entity that:
   * - Runs in its own thread
   * - Processes messages sequentially from its queue
   * - Communicates with other actors only via messages
   * - Has isolated state (no shared mutable state)
   *
   * Usage:
   *   class MyActor : public actors::Actor {
   *   public:
   *     MyActor() {
   *       MESSAGE_HANDLER(msg::Start, on_start);
   *       MESSAGE_HANDLER(msg::MyMessage, on_my_message);
   *     }
   *   private:
   *     void on_start(const msg::Start*) noexcept { ... }
   *     void on_my_message(const msg::MyMessage* m) noexcept { ... }
   *   };
   */
  class Actor
  {
    friend class Manager;
    friend class Group;

  public:
    Actor();
    virtual ~Actor();

    // Non-copyable
    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;

    /**
     * Send a message asynchronously (fire-and-forget)
     * Message is queued and processed later by receiver's thread
     * @param m Message to send (must be heap-allocated, Actor takes ownership)
     * @param sender The sending actor (for reply routing)
     */
    void send(const Message *m, Actor *sender = nullptr) noexcept;

    /**
     * Send a message synchronously and wait for reply
     * Handler runs immediately in caller's thread
     * @param m Message to send (can be stack-allocated)
     * @param sender The sending actor
     * @return Reply message, or nullptr if no reply
     */
    std::unique_ptr<const Message> fast_send(const Message *m, Actor *sender) noexcept;

    /**
     * Reply to the current message
     * Works for both async (send) and sync (fast_send) messages
     */
    void reply(const Message *m) noexcept;

    virtual const char* get_name() const { return name; }
    std::size_t queue_length() const noexcept;
    const Message* peek() const;

    /**
     * Main processing loop - runs in dedicated thread
     * Called by Manager via std::thread
     */
    void operator()() noexcept;

    /// Initiate graceful shutdown
    virtual void terminate() noexcept;

  protected:
    bool terminated = false;
    Actor *reply_to = nullptr;
    long long msg_cnt = 0;
    char name[256];

    /**
     * Override to handle messages not registered via MESSAGE_HANDLER
     */
    virtual void process_message(const Message *) {}

    /**
     * Called before actor starts processing messages
     */
    virtual void init() {}

    /**
     * Called after actor stops processing messages
     */
    virtual void end() {}

    virtual bool is_group() const { return false; }
    virtual void fast_terminate() noexcept;

    // For Group support
    void set_group(Actor *pgroup);
    Actor *get_group() const;
    void process_message_internal(const Message *m, bool dontdel = false) noexcept;

  private:
    Queue<const Message *> *msgq;
    std::mutex fast_send_mutex;
    bool using_fast_send = false;
    const Message *reply_message = nullptr;
    Actor *group = nullptr;
    inline static bool terminate_called = false;
    std::vector<generic_handler_t> handler_cache;
    std::vector<bool> dont_have_handler;
    bool is_managed = false;
    bool is_part_of_group = false;
    std::set<int> affinity;
    int priority = 0;
    int priority_type = 0;
    Manager *manager = nullptr;
    pid_t tid = 0;

    // Handler registration (public for macro, but only used internally)
  public:
    std::map<std::type_index, generic_handler_t> handlers;

  private:
    void add_message_to_queue(const Message *m);
    bool call_handler(const Message *m) noexcept;

    void set_manager(Manager *mgr) { manager = mgr; }
    Manager *get_manager() const { return manager; }
  };

  // Helper template for registering handlers
  template <typename ActorT, typename MsgT>
  struct register_handler
  {
    Actor *actor;
    register_handler(Actor *a) : actor(a) {}
    typedef void (ActorT::*handler_t)(const MsgT *);

    void operator()(handler_t ptr) const
    {
      generic_handler_t generic_ptr = reinterpret_cast<generic_handler_t>(ptr);
      actor->handlers[std::type_index(typeid(MsgT))] = generic_ptr;
    }
  };

}
