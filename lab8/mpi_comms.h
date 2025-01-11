#pragma once
#include "Message.h"
#include <string>
#include <mpi.h>
#include <queue>
#include "DistributedSharedMemory.h"

namespace MPIComms {
    extern MPI_Comm timestamp_comm;

    void initialize();
    void finalize();

    int get_global_timestamp();
    void run_timestamp_server();
    void listener(std::priority_queue<Message, std::vector<Message>, MessageCompare>& message_queue,
                  DistributedSharedMemory& dsm, std::mutex& dsm_mutex);

    void send_update_message(VariableType var, int new_value, int destination_id);
    void send_subscribe_message(VariableType var, int subscriber_id, int destination_id);
    void send_close_message(int destination_id);
    Message get_message();
    VariableType validate_variable(const std::string& var);
}
