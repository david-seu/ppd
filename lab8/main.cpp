#include <mpi.h> 
#include <thread> 
#include <mutex> 
#include <queue> 
#include <vector>
#include <string> 
#include <chrono> 
#include <functional> 
#include <iostream> 

#include "mpi_comms.h"
#include "DistributedSharedMemory.h" 
#include "globals.h"
#include "utils.h"
#include "Message.h" 
#include "MessageTypes.h"




int main() {
    MPIComms::initialize();
    int id = Globals::get_current_id();
    int comm_size;
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);

    DistributedSharedMemory dsm;
    std::mutex dsm_mutex;

    std::thread ts_thread;
    if (id == 0) {
        ts_thread = std::thread([]() {
            MPIComms::run_timestamp_server();
        });
    }

    std::priority_queue<Message, std::vector<Message>, MessageCompare> message_queue;

    std::thread listener_thread([&]() {
        MPIComms::listener(message_queue, dsm, dsm_mutex);
    });

    if (id == 0) {
        {
            std::lock_guard<std::mutex> lock(dsm_mutex);
            dsm.subscribe("a");
            dsm.subscribe("b");
            dsm.subscribe("c");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        {
            std::lock_guard<std::mutex> lock(dsm_mutex);
            dsm.update_variable("a", 2);
            dsm.update_variable("c", 4);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        {
            std::lock_guard<std::mutex> lock(dsm_mutex);
            dsm.check_and_replace("c", 4, 6);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        for (int r = 0; r < comm_size; r++)
            MPIComms::send_close_message(r);

        int terminate_signal = 0;
        MPI_Send(&terminate_signal, 1, MPI_INT, 0, 100, MPIComms::timestamp_comm);
        ts_thread.join();
    } else if (id == 1) {
        {
            std::lock_guard<std::mutex> lock(dsm_mutex);
            dsm.subscribe("a");
            dsm.subscribe("b");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        {
            std::lock_guard<std::mutex> lock(dsm_mutex);
            dsm.update_variable("a", 6);
            dsm.update_variable("b", 5);
        }

    } else if (id == 2) {
        {
            std::lock_guard<std::mutex> lock(dsm_mutex);
            dsm.subscribe("c");
            dsm.subscribe("a");
        }
    }

    listener_thread.join();

    {
        std::lock_guard<std::mutex> lock(dsm_mutex);
        Utils::println("a: " + std::to_string(dsm.get_var("a")));
        Utils::println("b: " + std::to_string(dsm.get_var("b")));
        Utils::println("c: " + std::to_string(dsm.get_var("c")));
    }

    MPIComms::finalize();
    return 0;
}
