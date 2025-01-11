#include "DistributedSharedMemory.h"
#include "globals.h"
#include "mpi_comms.h"
#include "utils.h"
#include <stdexcept>

DistributedSharedMemory::DistributedSharedMemory() {
    variables_["a"] = std::make_unique<Variable>();
    variables_["b"] = std::make_unique<Variable>();
    variables_["c"] = std::make_unique<Variable>();
}

Variable& DistributedSharedMemory::get_variable(const std::string& var) {
    auto it = variables_.find(var);

    if (it != variables_.end() && it->second) return *(it->second);

    throw std::invalid_argument("Variable '" + var + "' not found.");
}

const Variable& DistributedSharedMemory::get_variable(const std::string& var) const {
    auto it = variables_.find(var);

    if (it != variables_.end() && it->second) return *(it->second);
        
    throw std::invalid_argument("Variable '" + var + "' not found.");
}

void DistributedSharedMemory::set_variable(const std::string& var, int new_value) {
    get_variable(var).set_value(new_value);
}

int DistributedSharedMemory::get_var(const std::string& var) const {
    return get_variable(var).get_value();
}

void DistributedSharedMemory::subscribe(const std::string& var) {
    auto vt = MPIComms::validate_variable(var);

    if (vt == VariableType::Unknown) {
        Utils::println("Invalid variable name for subscription: " + var);
        throw std::invalid_argument("Invalid variable name for subscription: " + var);
    }

    for (int id = 0; id < Globals::get_procs(); ++id) {
        if (id != Globals::get_current_id()) { 
            MPIComms::send_subscribe_message(vt, Globals::get_current_id(), id);
        }
    }
}

void DistributedSharedMemory::update_subscription(const std::string& var, int id) {
    get_variable(var).add_subscriber(id);
}

void DistributedSharedMemory::update_variable(const std::string& var, int new_value) {
    auto vt = MPIComms::validate_variable(var);
    if (vt == VariableType::Unknown) {
        Utils::println("Invalid variable name for update: " + var);
        throw std::invalid_argument("Invalid variable name for update: " + var);
    }

    auto& x = get_variable(var);

    if (x.get_value() == new_value) return;

    x.set_value(new_value);

    for (int id : x.get_subscribers()) {
        MPIComms::send_update_message(vt, new_value, id);
    }
}

void DistributedSharedMemory::check_and_replace(const std::string& var, int old_value, int new_value) {
    auto vt = MPIComms::validate_variable(var);
    if (vt == VariableType::Unknown) {
        Utils::println("Invalid variable name for check_and_replace: " + var);
        throw std::invalid_argument("Invalid variable name for check_and_replace: " + var);
    }

    auto& x = get_variable(var);

    if (x.get_value() != old_value) 
        return;

    x.set_value(new_value);

    for (int id : x.get_subscribers())
        MPIComms::send_update_message(vt, new_value, id);
}

void DistributedSharedMemory::close() {
    for (int id = 0; id < Globals::get_procs(); ++id)
        MPIComms::send_close_message(id);
}