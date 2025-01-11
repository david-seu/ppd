#pragma once
#include <vector>
#include <mutex>

class Variable {
public:
    Variable() = default;
    ~Variable() = default;
    Variable(const Variable&) = delete;
    Variable& operator=(const Variable&) = delete;
    Variable(Variable&&) = delete;
    Variable& operator=(Variable&&) = delete;

    [[nodiscard]] int get_value() const;
    void set_value(int new_value);
    std::vector<int> get_subscribers() const;
    void add_subscriber(int id);

private:
    int value = 0;
    std::vector<int> subscribers;
    mutable std::mutex mtx;
};
