#pragma once

#include <iostream>
#include <mutex>
#include <string>
#include "globals.h"

namespace Utils {

    inline std::mutex cout_mutex;

    template<typename T>
    void println(T&& message) {
        std::lock_guard<std::mutex> lock(cout_mutex);
        
        int id = Globals::get_current_id();
        
        std::cout << id << ": " << std::forward<T>(message) << "\n";
        std::cout.flush();
    }

}
