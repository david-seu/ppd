#pragma once

#include "MessageTypes.h"
#include <array>

struct Message {
    MessageType type;
    VariableType var;
    int value;
    int timestamp;

    std::array<int, 4> to_array() const {
        return {
            static_cast<int>(type),
            static_cast<int>(var),
            value,
            timestamp
        };
    }

    static Message from_array(const std::array<int, 4>& arr) {
        Message msg;
        msg.type      = static_cast<MessageType>(arr[0]);
        msg.var       = static_cast<VariableType>(arr[1]);
        msg.value     = arr[2];
        msg.timestamp = arr[3];
        return msg;
    }
};

struct MessageCompare {
    bool operator()(const Message& a, const Message& b) const {
        return a.timestamp > b.timestamp;
        }
};
