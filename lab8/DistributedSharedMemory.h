#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "Variable.h"

class DistributedSharedMemory {
public:
    DistributedSharedMemory();
    ~DistributedSharedMemory() = default;

    void set_variable(const std::string& var, int new_value);
    int get_var(const std::string& var) const;
    void subscribe(const std::string& var);
    void update_subscription(const std::string& var, int id);
    void update_variable(const std::string& var, int new_value);
    void check_and_replace(const std::string& var, int old_value, int new_value);
    void close();

private:
    Variable& get_variable(const std::string& var);
    const Variable& get_variable(const std::string& var) const;
    std::unordered_map<std::string, std::unique_ptr<Variable>> variables_;
};
