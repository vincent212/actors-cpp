# C++ Remote Actors Guide

This guide covers remote actor communication in the C++ actors framework, enabling interoperability with Rust and Python actor systems.

## Overview

Remote actors communicate via ZeroMQ (ZMQ) using a JSON wire protocol. The framework uses:
- **PUSH/PULL sockets** for reliable message delivery
- **JSON serialization** via nlohmann/json library
- **Wire protocol compatible** with Rust and Python actors

## Dependencies

The remote actor system requires:

1. **ZeroMQ** - High-performance messaging library
   ```bash
   # Install on RHEL/Rocky/Fedora:
   sudo dnf install zeromq-devel cppzmq-devel
   ```

2. **nlohmann/json** - Header-only C++ JSON library
   ```bash
   # Install on RHEL/Rocky/Fedora:
   sudo dnf install json-devel
   ```

## Wire Format

All remote messages use the same JSON envelope format as Rust and Python:

```json
{
    "sender_actor": "ping",
    "sender_endpoint": "tcp://localhost:5002",
    "receiver": "pong",
    "message_type": "Ping",
    "message": {"count": 1}
}
```

| Field | Description |
|-------|-------------|
| `sender_actor` | Name of the sending actor (for replies), or null |
| `sender_endpoint` | ZMQ endpoint of the sender process, or null |
| `receiver` | Name of the target actor |
| `message_type` | Message class name (e.g., "Ping", "Pong") |
| `message` | JSON object with message fields |

## Message Serialization

### Understanding nlohmann/json

[nlohmann/json](https://github.com/nlohmann/json) is a header-only C++ library that provides intuitive JSON handling. Key features:

```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// Create JSON objects
json obj = {{"name", "Alice"}, {"age", 30}};

// Access values
std::string name = obj["name"];
int age = obj["age"].get<int>();

// Convert to string
std::string str = obj.dump();  // {"age":30,"name":"Alice"}
```

### Registering Messages for Remote Communication

Messages must be registered for serialization. Use the simple macros for common cases:

#### Simple Macros (Recommended)

```cpp
#include "actors/remote/Serialization.hpp"

// Define your messages
class Ping : public Message_N<100> {
public:
    int count;
    Ping(int c = 0) : count(c) {}
};

class Pong : public Message_N<101> {
public:
    int count;
    Pong(int c = 0) : count(c) {}
};

// Register with one line each!
REGISTER_REMOTE_MESSAGE_1(Ping, count, int)
REGISTER_REMOTE_MESSAGE_1(Pong, count, int)
```

That's it! The macro automatically:
- Gets the message ID from `Ping().get_message_id()`
- Uses the class name "Ping" as the wire format type
- Generates serialize/deserialize functions

#### Available Macros

| Macro | Fields | Usage |
|-------|--------|-------|
| `REGISTER_REMOTE_MESSAGE_0(Type)` | 0 | Messages with no data |
| `REGISTER_REMOTE_MESSAGE_1(Type, f, t)` | 1 | Single field |
| `REGISTER_REMOTE_MESSAGE_2(Type, f1, t1, f2, t2)` | 2 | Two fields |
| `REGISTER_REMOTE_MESSAGE_3(Type, f1, t1, ..., f3, t3)` | 3 | Three fields |
| `REGISTER_REMOTE_MESSAGE_4(Type, f1, t1, ..., f4, t4)` | 4 | Four fields |
| `REGISTER_REMOTE_MESSAGE_5(Type, f1, t1, ..., f5, t5)` | 5 | Five fields |
| `REGISTER_REMOTE_MESSAGE_6(Type, f1, t1, ..., f6, t6)` | 6 | Six fields |
| `REGISTER_REMOTE_MESSAGE_7(Type, f1, t1, ..., f7, t7)` | 7 | Seven fields |
| `REGISTER_REMOTE_MESSAGE_8(Type, f1, t1, ..., f8, t8)` | 8 | Eight fields |
| `REGISTER_REMOTE_MESSAGE_9(Type, f1, t1, ..., f9, t9)` | 9 | Nine fields |
| `REGISTER_REMOTE_MESSAGE_10(Type, f1, t1, ..., f10, t10)` | 10 | Ten fields |

For messages with more than 10 fields, use `REGISTER_REMOTE_MESSAGE` with custom serialize/deserialize.

#### Examples

```cpp
// No fields
class Heartbeat : public Message_N<50> {};
REGISTER_REMOTE_MESSAGE_0(Heartbeat)

// One field
class Ping : public Message_N<100> {
public:
    int count;
    Ping(int c = 0) : count(c) {}
};
REGISTER_REMOTE_MESSAGE_1(Ping, count, int)

// Two fields
class Quote : public Message_N<200> {
public:
    double bid;
    double ask;
    Quote(double b = 0, double a = 0) : bid(b), ask(a) {}
};
REGISTER_REMOTE_MESSAGE_2(Quote, bid, double, ask, double)

// Three fields
class Trade : public Message_N<201> {
public:
    std::string symbol;
    double price;
    int quantity;
    Trade(std::string s = "", double p = 0, int q = 0)
        : symbol(std::move(s)), price(p), quantity(q) {}
};
REGISTER_REMOTE_MESSAGE_3(Trade, symbol, std::string, price, double, quantity, int)
```

### Manual Registration (Advanced)

For complex messages or custom serialization, use the manual approach:

```cpp
REGISTER_REMOTE_MESSAGE(ComplexMsg,
    // Serialize body (msg is available as const ComplexMsg*)
    { return nlohmann::json{{"data", msg->get_data()}}; },
    // Deserialize body (j is available as const nlohmann::json&)
    { return new ComplexMsg(j["data"].get<std::string>()); }
)
```

### Understanding the Registration Pattern

The macros expand to a static initialization pattern:

```cpp
// REGISTER_REMOTE_MESSAGE_1(Ping, count, int) expands to:
namespace {
    static bool Ping_registered_ = []() {
        actors::serialization::register_message(
            Ping().get_message_id(),  // Gets ID from message class
            "Ping",                    // Type name for wire format
            [](const actors::Message* m) -> nlohmann::json {
                const Ping* msg = static_cast<const Ping*>(m);
                return nlohmann::json{{"count", msg->count}};
            },
            [](const nlohmann::json& j) -> actors::Message* {
                return new Ping(j["count"].get<int>());
            });
        return true;
    }();
}
```

Key parts:
- `static bool ... = []() { ... }();` - Immediately-invoked lambda (runs at startup)
- `Ping().get_message_id()` - Gets ID from message (no hardcoding!)
- `"Ping"` - Wire format name (must match Rust/Python)
- Serialize lambda: Message* → JSON
- Deserialize lambda: JSON → Message*

## ActorRef - Unified Local/Remote References

The `ActorRef` class provides a unified interface for sending messages to both local and remote actors:

```cpp
#include "actors/ActorRef.hpp"

// Local actor reference
ActorRef local_ref(some_actor);

// Remote actor reference
auto zmq_sender = std::make_shared<ZmqSender>("tcp://localhost:5002");
ActorRef remote_ref = zmq_sender->remote_ref("pong", "tcp://localhost:5001");

// Same send() syntax for both!
local_ref.send(new Ping(1), this);
remote_ref.send(new Ping(1), this);
```

### Implementation Details

`ActorRef` uses `std::variant` for zero-overhead polymorphism:

```cpp
class ActorRef {
    std::variant<LocalActorRef, RemoteActorRef> ref_;
public:
    void send(const Message* m, Actor* sender = nullptr) {
        std::visit([&](auto& r) { r.send(m, sender); }, ref_);
    }

    bool is_local() const;
    bool is_remote() const;
};
```

## Setting Up Remote Actors

### 1. Create ZmqSender

`ZmqSender` is an Actor that runs on its own thread. Sends are async and never block the caller - messages are serialized on the caller's thread and queued to the sender's thread for ZMQ transmission.

```cpp
#include "actors/remote/ZmqSender.hpp"

// Local endpoint for reply routing
// ZmqSender must be managed - it runs on its own thread
auto zmq_sender = std::make_shared<ZmqSender>("tcp://localhost:5001");
manage(zmq_sender.get());  // Must be managed!
```

### 2. Create ZmqReceiver

```cpp
#include "actors/remote/ZmqReceiver.hpp"

// Bind to receive messages
auto* zmq_receiver = new ZmqReceiver("tcp://0.0.0.0:5001", zmq_sender);

// Register local actors to receive remote messages
zmq_receiver->register_actor("pong", pong_actor);
```

### 3. Create Remote Actor Reference

```cpp
// Reference to actor "ping" on another process
ActorRef remote_ping = zmq_sender->remote_ref("ping", "tcp://localhost:5002");
```

### 4. Manager Setup

```cpp
class MyManager : public Manager {
    std::shared_ptr<ZmqSender> zmq_sender_;

public:
    MyManager() {
        // Create ZmqSender (now an Actor - must be managed!)
        zmq_sender_ = std::make_shared<ZmqSender>("tcp://localhost:5001");
        manage(zmq_sender_.get());

        auto* my_actor = new MyActor();
        manage(my_actor);

        auto* zmq_receiver = new ZmqReceiver("tcp://0.0.0.0:5001", zmq_sender_);
        zmq_receiver->register_actor("my_actor", my_actor);
        manage(zmq_receiver);
    }
};
```

## Complete Example: Remote Ping-Pong

### Pong Process (Receiver)

```cpp
#include <iostream>
#include "actors/Actor.hpp"
#include "actors/act/Manager.hpp"
#include "actors/msg/Start.hpp"
#include "actors/remote/ZmqSender.hpp"
#include "actors/remote/ZmqReceiver.hpp"
#include "actors/remote/Serialization.hpp"

using namespace actors;

// Messages
class Ping : public Message_N<100> {
public:
    int count;
    Ping(int c = 0) : count(c) {}
};

class Pong : public Message_N<101> {
public:
    int count;
    Pong(int c = 0) : count(c) {}
};

// Register messages - just one line each!
REGISTER_REMOTE_MESSAGE_1(Ping, count, int)
REGISTER_REMOTE_MESSAGE_1(Pong, count, int)

// Pong Actor
class PongActor : public Actor {
public:
    PongActor() {
        strncpy(name, "pong", sizeof(name));
        MESSAGE_HANDLER(Ping, on_ping);
    }

    void on_ping(const Ping* msg) noexcept {
        std::cout << "Received ping " << msg->count << std::endl;
        reply(new Pong(msg->count));
    }
};

// Manager
class PongManager : public Manager {
    std::shared_ptr<ZmqSender> sender_;
public:
    PongManager() {
        // ZmqSender is an Actor - must be managed!
        sender_ = std::make_shared<ZmqSender>("tcp://localhost:5001");
        manage(sender_.get());

        auto* pong = new PongActor();
        manage(pong);

        auto* receiver = new ZmqReceiver("tcp://0.0.0.0:5001", sender_);
        receiver->register_actor("pong", pong);
        manage(receiver);
    }
};

int main() {
    PongManager mgr;
    mgr.init();
    mgr.end();
    return 0;
}
```

## Interoperability with Rust and Python

The wire format is identical across all three languages. Key requirements:

1. **Message type names must match exactly** (case-sensitive)
   - C++: `"Ping"` in `register_message()`
   - Rust: `"Ping"` in `register_remote_message::<Ping>("Ping")`
   - Python: `@register_message class Ping`

2. **Field names must match**
   - JSON field names are used for serialization
   - All languages must use the same field names

3. **Port conventions** (by example):
   - Pong process: port 5001
   - Ping process: port 5002

### Testing C++ with Rust

Terminal 1 (Rust pong):
```bash
cd /home/vm/actors-rust
cargo run --example remote_pong
```

Terminal 2 (C++ ping):
```bash
cd /home/vm/actors-cpp
./examples/remote_ping
```

### Testing C++ with Python

Terminal 1 (Python pong):
```bash
cd /home/vm/actors-py/examples/remote_ping_pong
python3 pong_process.py
```

Terminal 2 (C++ ping):
```bash
cd /home/vm/actors-cpp
./examples/remote_ping
```

## Error Handling: Reject Messages

When a message cannot be processed, a `Reject` message is sent back:

```cpp
#include "actors/remote/Reject.hpp"

// Handle rejection
class MyActor : public Actor {
public:
    MyActor() {
        MESSAGE_HANDLER(msg::Reject, on_reject);
    }

    void on_reject(const msg::Reject* r) noexcept {
        std::cerr << "Message rejected: " << r->message_type << std::endl;
        std::cerr << "Reason: " << r->reason << std::endl;
        std::cerr << "Rejected by: " << r->rejected_by << std::endl;
    }
};
```

Reject reasons:
- Unknown message type (not registered)
- Target actor not found
- Deserialization failure

## Building

Add to your Makefile:
```makefile
CXXFLAGS = -std=c++20 -O2 -I../include
LDFLAGS = -lpthread -lzmq

remote_example: remote_example.cpp
    $(CXX) $(CXXFLAGS) $< -o $@ -L. -lactors $(LDFLAGS)
```

## API Reference

### ZmqSender

`ZmqSender` is an Actor that runs on its own thread. Sends are async and never block the caller - messages are serialized on the caller's thread and queued to the sender's thread for ZMQ transmission.

```cpp
class ZmqSender : public Actor {
    // Create sender with local endpoint for reply routing.
    // Spawns a dedicated sender thread for async sending.
    // Must be managed: manage(zmq_sender.get());
    ZmqSender(const std::string& local_endpoint);

    // Send message to endpoint/actor (async - returns immediately)
    // Message is serialized on caller's thread, queued to sender thread.
    void send_to(endpoint, actor_name, msg, sender);

    // Create a remote actor reference
    ActorRef remote_ref(name, endpoint);
};
```

### ZmqReceiver
```cpp
class ZmqReceiver : public Actor {
    ZmqReceiver(bind_endpoint, zmq_sender);
    void register_actor(name, actor);
    void unregister_actor(name);
};
```

### ActorRef
```cpp
class ActorRef {
    ActorRef(Actor* local);
    ActorRef(name, endpoint, zmq_sender);
    void send(msg, sender);
    bool is_local() const;
    bool is_remote() const;
};
```

### Serialization
```cpp
namespace serialization {
    void register_message(msg_id, type_name, serialize_fn, deserialize_fn);
    std::string get_type_name(msg_id);
    json serialize(msg);
    Message* deserialize(type_name, json);
}
```
