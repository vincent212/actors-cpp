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

/**
 * Ping-Pong Example - Two actors exchanging messages
 *
 * Demonstrates:
 * - Creating custom actors
 * - Defining custom messages
 * - Using MESSAGE_HANDLER macro
 * - send() for async messaging
 * - fast_send() for sync messaging
 */

#include <iostream>
#include "actors/Actor.hpp"
#include "actors/act/Manager.hpp"
#include "actors/msg/Start.hpp"

using namespace actors;
using namespace std;

// Custom message with a counter
struct Ping : public Message_N<100> {
  int count;
  Ping(int c) : count(c) {}
};

struct Pong : public Message_N<101> {
  int count;
  Pong(int c) : count(c) {}
};

// Actor that receives Pong and sends Ping
class PingActor : public Actor {
  Actor* pong_actor;
  Actor* manager;
  int max_count;

public:
  PingActor(Actor* pong, Actor* mgr, int max = 10)
    : pong_actor(pong), manager(mgr), max_count(max)
  {
    strncpy(name, "PingActor", sizeof(name));
    MESSAGE_HANDLER(msg::Start, on_start);
    MESSAGE_HANDLER(Pong, on_pong);
  }

  void on_start(const msg::Start*) {
    cout << "PingActor: Starting ping-pong" << endl;
    pong_actor->send(new Ping(1), this);
  }

  void on_pong(const Pong* m) {
    cout << "PingActor: Received pong " << m->count << endl;
    if (m->count >= max_count) {
      cout << "PingActor: Done!" << endl;
      manager->terminate();
    } else {
      pong_actor->send(new Ping(m->count + 1), this);
    }
  }
};

// Actor that receives Ping and sends Pong
class PongActor : public Actor {
public:
  PongActor() {
    strncpy(name, "PongActor", sizeof(name));
    MESSAGE_HANDLER(Ping, on_ping);
  }

  void on_ping(const Ping* m) {
    cout << "PongActor: Received ping " << m->count << ", sending pong" << endl;
    reply(new Pong(m->count));
  }
};

// Manager that sets up and runs the actors
class PingPongManager : public Manager {
public:
  PingPongManager() {
    auto* pong = new PongActor();
    auto* ping = new PingActor(pong, this, 5);

    manage(pong);
    manage(ping);
  }
};

int main() {
  cout << "=== Ping-Pong Actor Example ===" << endl;

  PingPongManager mgr;
  mgr.init();  // Start all managed actors
  mgr.end();   // Wait for all actors to finish

  return 0;
}
