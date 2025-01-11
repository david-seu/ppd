#pragma once

enum class MessageType : int {
    Update = 0,
    Subscribe = 1,
    Close = 2,
    Invalid = -1
};

enum class VariableType : int {
    A = 0,
    B = 1,
    C = 2,
    Unknown = -1
};
