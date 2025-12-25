/*

THIS SOFTWARE IS OPEN SOURCE UNDER THE MIT LICENSE

Copyright 2025 Vincent Maciejewski, & M2 Tech

Reject message - sent back when a remote message cannot be processed.

*/

#pragma once

#include <string>
#include "actors/Message.hpp"
#include "actors/remote/Serialization.hpp"

namespace actors::msg {

/**
 * Reject - Sent when a remote message cannot be processed
 *
 * Reasons for rejection:
 * - Unknown message type (not registered)
 * - Target actor not found
 * - Deserialization failure
 *
 * Message ID: 9 (matches Rust/Python convention)
 */
class Reject : public Message_N<9> {
public:
    std::string message_type;   // Type of the rejected message
    std::string reason;         // Why it was rejected
    std::string rejected_by;    // Name of actor/receiver that rejected it

    Reject() = default;

    Reject(std::string msg_type, std::string reason, std::string rejected_by)
        : message_type(std::move(msg_type))
        , reason(std::move(reason))
        , rejected_by(std::move(rejected_by)) {}
};

} // namespace actors::msg

// Register Reject for remote serialization
namespace {
    static bool Reject_registered_ = []() {
        actors::serialization::register_message(9, "Reject",
            // Serialize
            [](const actors::Message* m) -> nlohmann::json {
                const actors::msg::Reject* msg = static_cast<const actors::msg::Reject*>(m);
                return nlohmann::json{
                    {"message_type", msg->message_type},
                    {"reason", msg->reason},
                    {"rejected_by", msg->rejected_by}
                };
            },
            // Deserialize
            [](const nlohmann::json& j) -> actors::Message* {
                return new actors::msg::Reject(
                    j["message_type"].get<std::string>(),
                    j["reason"].get<std::string>(),
                    j["rejected_by"].get<std::string>()
                );
            });
        return true;
    }();
}
