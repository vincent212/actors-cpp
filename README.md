# Actors - High-Performance Actor Framework for C++

A lightweight, high-performance actor framework for building concurrent systems in C++20.

## Overview

This is part of a **multi-language actor framework** spanning C++, Rust, and Python. All three implementations share a common JSON-over-ZMQ wire protocol, enabling seamless cross-language communication while leveraging each language's unique strengths.

The C++ implementation is designed for scenarios where every microsecond matters—high-frequency trading, real-time systems, and performance-critical infrastructure.

**When to use C++**: Financial trading systems, game engines, embedded real-time systems, or any application where you need deterministic latency and maximum throughput.

For the full project documentation and blog post, see: https://m2te.ch/blog/opensource/actor-model

**Related repositories:**
- [actors-rust](https://github.com/anthropics/actors-rust) - Rust implementation (safety + performance)
- [actors-py](https://github.com/anthropics/actors-py) - Python implementation (rapid prototyping)

## Features

- **Actor Model**: Independent entities processing messages sequentially
- **Message-Driven**: All communication via typed message passing
- **Thread-Safe**: Each actor runs in its own thread with isolated state
- **Low-Latency**: Designed for high-frequency trading systems
- **Simple API**: Easy to learn, minimal boilerplate

## Quick Start

### 1. Define Messages

```cpp
#include "actors/Message.hpp"

// Each message type has a unique ID (0-511 for optimal performance)
struct Ping : public actors::Message_N<100> {
  int count;
  Ping(int c) : count(c) {}
};

struct Pong : public actors::Message_N<101> {
  int count;
  Pong(int c) : count(c) {}
};
```

### 2. Create Actors

```cpp
#include "actors/Actor.hpp"

class PongActor : public actors::Actor {
public:
  PongActor() {
    MESSAGE_HANDLER(Ping, on_ping);  // Register handler
  }

private:
  void on_ping(const Ping* m) {
    std::cout << "Received ping " << m->count << std::endl;
    reply(new Pong(m->count));  // Reply to sender
  }
};
```

### 3. Set Up Manager

```cpp
#include "actors/act/Manager.hpp"

class MyManager : public actors::Manager {
public:
  MyManager() {
    auto* pong = new PongActor();
    auto* ping = new PingActor(pong, this);

    manage(pong);  // Register actors
    manage(ping);
  }
};

int main() {
  MyManager mgr;
  mgr.init();  // Start all actors
  mgr.end();   // Wait for all actors to finish
}
```

## Core Concepts

### Actor
Base class for all actors. Override `process_message()` or use `MESSAGE_HANDLER` macro.

### Message
All messages inherit from `Message_N<ID>` where ID is a unique integer (0-511 preferred).

### Manager
Manages actor lifecycle, thread creation, CPU affinity, and thread priority.

### Group
Run multiple lightweight actors in a single thread.

## Messaging

### Async Send (Fire-and-Forget)
```cpp
other_actor->send(new MyMessage(data), this);
```
Message is queued and processed later by the receiver's thread.

### Sync Send (RPC-style)
```cpp
auto reply = other_actor->fast_send(new Request(), this);
auto response = dynamic_cast<const Response*>(reply.get());
```
Handler runs immediately in caller's thread. Use for request/response patterns.

### Reply
```cpp
void on_request(const Request* m) {
  reply(new Response(m->id));  // Works for both send() and fast_send()
}
```

## CPU Affinity & Priority

```cpp
// Pin actor to CPU core 2 with FIFO scheduling, priority 50
int prio = sched_get_priority_max(SCHED_FIFO);
manage(my_actor, {2}, prio, SCHED_FIFO);
```

## Building

### Build the Library

```bash
cd src
make
```

This creates `libactors.a` static library.

### Build Examples

```bash
cd examples
g++ -std=c++20 -O2 -I../include ping_pong.cpp -L../src -lactors -lpthread -o ping_pong
./ping_pong
```

### Requirements

- C++20 compiler (GCC 10+ or Clang 12+)
- Boost (circular_buffer only)
- pthreads



## License

MIT License
