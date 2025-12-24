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
#include <set>
#include <string>
#include <thread>

#include "actors/Actor.hpp"

namespace actors
{
  /**
   * Manager - Manages the lifecycle of actors
   *
   * The Manager:
   * - Registers actors and starts their threads
   * - Handles CPU affinity and thread priority
   * - Coordinates startup and shutdown
   *
   * Usage:
   *   class MyManager : public actors::Manager {
   *   public:
   *     MyManager() {
   *       manage(new MyActor(), {0}, 50, SCHED_FIFO);  // Pin to CPU 0
   *     }
   *   };
   *
   *   MyManager mgr;
   *   mgr.init();  // Start all actors
   *   // ... run ...
   *   mgr.end();   // Wait for actors to finish
   */
  class Manager : public Actor
  {
    std::list<actor_ptr> actor_list;
    std::list<std::thread*> thread_list;
    std::map<std::string, actor_ptr> managed_name_map;
    std::map<std::string, actor_ptr> expanded_name_map;

  protected:
    Manager();
    ~Manager();

    /// Handle internal messages (Start, Shutdown)
    void process_message(const Message* m) override;

  public:
    /**
     * Start all managed actors
     * Sends Start message to each actor and launches their threads.
     * Call this after registering all actors with manage().
     */
    void init();

    /**
     * Wait for all actors to finish
     * Blocks until all actor threads have terminated.
     */
    void end();

    /**
     * Register an actor to be managed
     * @param actor The actor to manage (takes ownership)
     * @param affinity Set of CPU cores to pin the actor to (empty = no pinning)
     * @param priority Thread priority 1-99 (requires CAP_SYS_NICE, 0 = default)
     * @param priority_type SCHED_OTHER (default), SCHED_FIFO, or SCHED_RR
     */
    void manage(actor_ptr actor,
                std::set<int> affinity = {},
                int priority = 0,
                int priority_type = SCHED_OTHER);

    /**
     * Find an actor by name
     * @param name Actor name to search for
     * @return Pointer to actor, or nullptr if not found
     */
    actor_ptr get_actor_by_name(const std::string& name) const noexcept;

    /**
     * Get map of all actor names to actor pointers
     * Includes actors inside groups.
     */
    const std::map<std::string, actor_ptr>& get_name_map() const noexcept {
      return expanded_name_map;
    }

    /**
     * Get list of all managed actor names
     * Includes actors inside groups.
     */
    std::list<std::string> get_managed_names() const noexcept;

    /**
     * Get list of all top-level managed actors
     * Groups are returned as single entries (not expanded).
     */
    std::list<actor_ptr> get_managed_actors() const noexcept { return actor_list; }

    /**
     * Get total pending messages across all actors
     * Useful for monitoring backpressure.
     */
    std::size_t total_queue_length();

    /**
     * Get pending message count per actor
     * @return Map of actor name to queue length
     */
    std::map<std::string, std::size_t> get_queue_lengths() const noexcept;

    /**
     * Get thread ID and message count per actor
     * @return Map of actor name to (tid, message_count) tuple
     */
    std::map<std::string, std::tuple<pid_t, int>> get_message_counts() const noexcept;
  };
}
