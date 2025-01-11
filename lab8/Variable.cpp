#include "Variable.h"

int Variable::get_value() const {
    std::lock_guard<std::mutex> lock(mtx);
    return value;
}

void Variable::set_value(int new_value) {
    std::lock_guard<std::mutex> lock(mtx);
    value = new_value;
}

std::vector<int> Variable::get_subscribers() const {
    std::lock_guard<std::mutex> lock(mtx);
    return subscribers;
}

void Variable::add_subscriber(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    subscribers.push_back(id);
}
