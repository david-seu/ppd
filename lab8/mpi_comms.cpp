#include "mpi_comms.h"
#include "MessageTypes.h"
#include "Message.h"
#include "utils.h"
#include "globals.h"

#include <mpi.h>
#include <stdexcept>
#include <array>
#include <string>
#include <sstream>
#include <vector>
#include <atomic>
#include <string>
#include <queue>
#include "DistributedSharedMemory.h"


constexpr int TAG_TIMESTAMP_REQUEST = 99;
constexpr int TAG_TERMINATION_SIGNAL = 100;

static std::atomic<int> g_timestamp_counter = 0;

namespace MPIComms {
    MPI_Comm timestamp_comm;

    void initialize() {
        int initialized;
        MPI_Initialized(&initialized);
        if (!initialized) {
            int provided;
            MPI_Init_thread(nullptr, nullptr, MPI_THREAD_MULTIPLE, &provided);
            if (provided < MPI_THREAD_MULTIPLE)
                throw std::runtime_error("MPI does not support required threading level");
        }

        int rank, size;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        Globals::set_current_id(rank);
        Globals::set_procs(size);

        MPI_Comm_split(MPI_COMM_WORLD, 0, rank, &timestamp_comm);
    }

    void finalize() {
        if (timestamp_comm != MPI_COMM_NULL) {
            MPI_Comm_free(&timestamp_comm);
        }
        int finalized;
        MPI_Finalized(&finalized);
        if (!finalized) {
            MPI_Finalize();
        }
    }

    VariableType validate_variable(const std::string& var) {
        if (var.size() != 1) return VariableType::Unknown;
        switch (var[0]) {
            case 'a': return VariableType::A;
            case 'b': return VariableType::B;
            case 'c': return VariableType::C;
            default: return VariableType::Unknown;
        }
    }

    void send_message(const Message& msg, int destination_id) {
        auto data = msg.to_array();
        std::stringstream ss;
        ss << "Sending " << static_cast<int>(msg.type) << " "
           << static_cast<int>(msg.var) << " "
           << msg.value << " " << msg.timestamp << " to " << destination_id;
        Utils::println(ss.str());

        int result = MPI_Send(data.data(), data.size(), MPI_INT, destination_id, 0, MPI_COMM_WORLD);
        if (result != MPI_SUCCESS) {
            std::stringstream err;
            err << "MPI_Send failed to " << destination_id << " with result " << result;
            throw std::runtime_error(err.str());
        }
    }

    void send_update_message(VariableType var, int new_value, int destination_id) {
        if (var == VariableType::Unknown) throw std::invalid_argument("Invalid VariableType for update message");
        Message msg;
        msg.type = MessageType::Update;
        msg.var  = var;
        msg.value = new_value;
        msg.timestamp = get_global_timestamp();

        send_message(msg, destination_id);
    }

    void send_subscribe_message(VariableType var, int subscriber_id, int destination_id) {
        Message msg;
        msg.type = MessageType::Subscribe;
        msg.var = var;
        msg.value = subscriber_id;
        msg.timestamp = get_global_timestamp();

        send_message(msg, destination_id);
    }

    void send_close_message(int destination_id) {
        Message msg;
        msg.type = MessageType::Close;
        msg.var = VariableType::Unknown;
        msg.value = 0;
        msg.timestamp = get_global_timestamp();

        send_message(msg, destination_id);
    }

    Message get_message() {
        std::array<int, 4> data;
        MPI_Status status;

        int result = MPI_Recv(data.data(), data.size(), MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        if (result != MPI_SUCCESS) throw std::runtime_error("MPI_Recv failed");

        Message msg = Message::from_array(data);

        std::stringstream ss;
        ss << "Received " << static_cast<int>(msg.type) << " "
           << static_cast<int>(msg.var) << " "
           << msg.value << " from " << status.MPI_SOURCE;
        Utils::println(ss.str());

        return msg;
    }

    int get_global_timestamp() {
        int rank = Globals::get_current_id();

        if (rank == 0) {
            return g_timestamp_counter.fetch_add(1, std::memory_order_relaxed);
        } else {
            int dummy = 0;
            MPI_Send(&dummy, 1, MPI_INT, 0, TAG_TIMESTAMP_REQUEST, MPIComms::timestamp_comm);

            int ts;
            MPI_Recv(&ts, 1, MPI_INT, 0, TAG_TIMESTAMP_REQUEST, MPIComms::timestamp_comm, MPI_STATUS_IGNORE);

            return ts;
        }
    }


   void run_timestamp_server() {
        if (Globals::get_current_id() != 0 || MPIComms::timestamp_comm == MPI_COMM_NULL) {
            throw std::runtime_error("run_timestamp_server() called on non-zero rank or without timestamp communicator");
        }

        Utils::println("Rank 0 Timestamp Server started.");
        while (true) {
            MPI_Status status;
            int dummy;

            MPI_Recv(&dummy, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPIComms::timestamp_comm, &status);

            if (status.MPI_TAG == TAG_TIMESTAMP_REQUEST) {
                int ts = g_timestamp_counter.fetch_add(1, std::memory_order_relaxed);
                MPI_Send(&ts, 1, MPI_INT, status.MPI_SOURCE, TAG_TIMESTAMP_REQUEST, MPIComms::timestamp_comm);
            } else if (status.MPI_TAG == TAG_TERMINATION_SIGNAL && dummy == 0) {
                break;
            } else {
                Utils::println("Unexpected message with tag: " + std::to_string(status.MPI_TAG));
            }
        }
        Utils::println("Rank 0 Timestamp Server stopping.");
    }


    void listener(std::priority_queue<Message, std::vector<Message>, MessageCompare>& message_queue,
              DistributedSharedMemory& dsm,
              std::mutex& dsm_mutex) {
        bool received_close = false;

        while (!received_close) {
            Message message = MPIComms::get_message();

            {
                std::lock_guard<std::mutex> lock(dsm_mutex);

                if (message.type == MessageType::Subscribe) {
                    std::string var;
                    switch (message.var) {
                        case VariableType::A: var = "a"; break;
                        case VariableType::B: var = "b"; break;
                        case VariableType::C: var = "c"; break;
                        default:
                            continue;
                    }
                    dsm.update_subscription(var, message.value);

                } else {
                    message_queue.push(message); 

                    if (message.type == MessageType::Close) {
                        received_close = true;
                    }
                }
            }
        }

        while (!message_queue.empty()) {
            std::lock_guard<std::mutex> lock(dsm_mutex);

            const Message& top = message_queue.top();
            Utils::println("Processing message Type: " + std::to_string(static_cast<int>(top.type)) +
                        ", Var: " + std::to_string(static_cast<int>(top.var)) +
                        ", Value: " + std::to_string(top.value));

            std::string var;
            switch (top.var) {
                case VariableType::A: var = "a"; break;
                case VariableType::B: var = "b"; break;
                case VariableType::C: var = "c"; break;
                default:
                    break;
            }

            if (top.type == MessageType::Update) {
                dsm.set_variable(var, top.value);
            }

            message_queue.pop();
        }
    }
}