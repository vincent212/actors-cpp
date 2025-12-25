/*

THIS SOFTWARE IS OPEN SOURCE UNDER THE MIT LICENSE

Copyright 2025 Vincent Maciejewski, & M2 Tech

ZmqReceiver - Receives messages from remote actors via ZeroMQ PULL socket.
Implemented as an Actor that polls the socket and routes messages.

*/

#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <zmq.hpp>
#include <nlohmann/json.hpp>
#include "actors/Actor.hpp"
#include "actors/ActorRef.hpp"
#include "actors/msg/Start.hpp"
#include "actors/msg/Continue.hpp"
#include "actors/remote/Serialization.hpp"
#include "actors/remote/Reject.hpp"
#include "actors/remote/ZmqSender.hpp"

namespace actors {

/**
 * ZmqReceiver - Actor that receives and routes remote messages
 *
 * Binds to a ZMQ PULL socket and routes incoming messages to
 * registered local actors. Sends Reject messages for errors.
 *
 * Usage:
 *   auto sender = std::make_shared<ZmqSender>("tcp://localhost:5001");
 *   auto receiver = new ZmqReceiver("tcp://0.0.0.0:5001", sender);
 *
 *   receiver->register_actor("pong", pong_actor);
 *
 *   mgr.manage("zmq_receiver", receiver);
 *   mgr.init();
 */
/**
 * RemoteReplyProxy - Proxy actor that forwards replies to remote actors
 *
 * When a remote message arrives, we create a proxy that the local actor
 * can use as reply_to. When the local actor calls reply(), the proxy
 * intercepts it and forwards via ZMQ.
 */
class RemoteReplyProxy : public Actor {
    std::shared_ptr<ZmqSender> sender_;
    std::string remote_actor_;
    std::string remote_endpoint_;

public:
    RemoteReplyProxy(std::shared_ptr<ZmqSender> sender,
                     std::string actor, std::string endpoint)
        : sender_(std::move(sender))
        , remote_actor_(std::move(actor))
        , remote_endpoint_(std::move(endpoint)) {
        strncpy(name, "RemoteReplyProxy", sizeof(name));
    }

    // Override send() to forward directly via ZMQ instead of queuing
    // This proxy is never started with a thread, so we handle it synchronously
    void send(const Message* m, Actor* /*sender*/ = nullptr) noexcept override {
        // Forward this message to the remote actor
        sender_->send_to(remote_endpoint_, remote_actor_, m, nullptr);
        // Note: ZmqSender::send_to deletes the message
    }
};

class ZmqReceiver : public Actor {
public:
    /**
     * Create a ZmqReceiver
     *
     * @param bind_endpoint Endpoint to bind to (e.g., "tcp://0.0.0.0:5001")
     * @param sender ZmqSender for sending Reject messages
     */
    ZmqReceiver(const std::string& bind_endpoint, std::shared_ptr<ZmqSender> sender)
        : context_(1)
        , socket_(context_, zmq::socket_type::pull)
        , sender_(std::move(sender))
        , bind_endpoint_(bind_endpoint)
        , running_(false) {
        strncpy(name, "ZmqReceiver", sizeof(name));

        // Register message handlers
        MESSAGE_HANDLER(msg::Start, on_start);
        MESSAGE_HANDLER(msg::Continue, on_continue);

        // Bind socket
        std::string bind_addr = bind_endpoint_;
        // Convert tcp://*:PORT to tcp://0.0.0.0:PORT
        size_t pos = bind_addr.find("*:");
        if (pos != std::string::npos) {
            bind_addr.replace(pos, 1, "0.0.0.0");
        }
        socket_.bind(bind_addr);

        // Set receive timeout for non-blocking polls
        socket_.set(zmq::sockopt::rcvtimeo, 10);  // 10ms timeout
    }

    ~ZmqReceiver() {
        // Clean up proxy actors
        for (auto* proxy : proxies_) {
            delete proxy;
        }
    }

    /**
     * Register a local actor to receive remote messages
     */
    void register_actor(const std::string& name, Actor* actor) {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        registry_[name] = actor;
    }

    /**
     * Unregister an actor
     */
    void unregister_actor(const std::string& name) {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        registry_.erase(name);
    }

private:
    void on_start(const msg::Start*) noexcept {
        running_ = true;
        // Send ourselves a Continue to start polling
        send(new msg::Continue(), this);
    }

    void on_continue(const msg::Continue*) noexcept {
        if (!running_) return;

        // Poll for messages (non-blocking due to timeout)
        try {
            zmq::message_t message;
            auto result = socket_.recv(message, zmq::recv_flags::none);

            if (result.has_value()) {
                // Parse JSON
                std::string data(static_cast<char*>(message.data()), message.size());
                try {
                    nlohmann::json envelope = nlohmann::json::parse(data);
                    handle_remote_message(envelope);
                } catch (const nlohmann::json::exception& e) {
                    // JSON parse error - can't send reject (don't know sender)
                }
            }
        } catch (const zmq::error_t& e) {
            // ZMQ error - ignore timeouts
            if (e.num() != EAGAIN) {
                // Log error?
            }
        }

        // Continue polling
        if (running_) {
            send(new msg::Continue(), this);
        }
    }

    void handle_remote_message(const nlohmann::json& envelope) {
        std::string receiver_name = envelope["receiver"].get<std::string>();
        std::string msg_type = envelope["message_type"].get<std::string>();

        // Get sender info for replies
        std::string sender_actor;
        std::string sender_endpoint;
        bool has_sender = !envelope["sender_actor"].is_null();
        if (has_sender) {
            sender_actor = envelope["sender_actor"].get<std::string>();
            sender_endpoint = envelope["sender_endpoint"].get<std::string>();
        }

        // Find target actor
        Actor* target = nullptr;
        {
            std::lock_guard<std::mutex> lock(registry_mutex_);
            auto it = registry_.find(receiver_name);
            if (it != registry_.end()) {
                target = it->second;
            }
        }

        if (!target) {
            // Actor not found - send Reject
            if (has_sender) {
                send_reject(sender_endpoint, sender_actor, msg_type,
                           "Actor '" + receiver_name + "' not found",
                           receiver_name);
            }
            return;
        }

        // Deserialize message
        Message* msg = serialization::deserialize(msg_type, envelope["message"]);
        if (!msg) {
            // Unknown message type - send Reject
            if (has_sender) {
                send_reject(sender_endpoint, sender_actor, msg_type,
                           "Unknown message type: " + msg_type,
                           receiver_name);
            }
            return;
        }

        // Create proxy for reply routing
        Actor* reply_actor = nullptr;
        if (has_sender) {
            // Create a proxy that forwards replies to the remote sender
            auto* proxy = new RemoteReplyProxy(sender_, sender_actor, sender_endpoint);
            proxies_.push_back(proxy);
            reply_actor = proxy;
        }

        // Send to target actor
        target->send(msg, reply_actor);
    }

    void send_reject(const std::string& endpoint,
                     const std::string& actor_name,
                     const std::string& msg_type,
                     const std::string& reason,
                     const std::string& rejected_by) {
        auto* reject = new msg::Reject(msg_type, reason, rejected_by);
        sender_->send_to(endpoint, actor_name, reject, nullptr);
    }

    void terminate() noexcept override {
        running_ = false;
        Actor::terminate();
    }

private:
    zmq::context_t context_;
    zmq::socket_t socket_;
    std::shared_ptr<ZmqSender> sender_;
    std::string bind_endpoint_;
    std::unordered_map<std::string, Actor*> registry_;
    std::mutex registry_mutex_;
    bool running_;
    std::vector<RemoteReplyProxy*> proxies_;
};

} // namespace actors
