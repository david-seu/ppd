#pragma once
#include <atomic>

namespace Globals {
    inline std::atomic<int> current_id{0};
    inline std::atomic<int> procs{1};

    inline void set_current_id(int new_id) {
        current_id.store(new_id, std::memory_order_relaxed);
    }

    inline void set_procs(int new_procs) {
        procs.store(new_procs, std::memory_order_relaxed);
    }

    inline int get_current_id() {
        return current_id.load(std::memory_order_relaxed);
    }

    inline int get_procs() {
        return procs.load(std::memory_order_relaxed);
    }
}
