#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <random>
#include <algorithm>
#include <set>
#include <chrono>
#include <memory>
#include <fstream>  // For file operations
#include <sstream>  // For building file names

struct OperationRecord {
    unsigned int serial_number;
    int amount;
    int from_account_id;
    int to_account_id;
};

class Account {
public:
    int id; // Unique identifier
    int balance;
    std::vector<OperationRecord> log;
    std::mutex mtx;

    Account(int id_, int initial_balance) : id(id_), balance(initial_balance) {}
};

std::vector<std::shared_ptr<Account>> accounts; // Use shared_ptr instead of Account objects
std::atomic<unsigned int> serial_number(0);

void transfer(int from_id, int to_id, int amount) {
    if (from_id == to_id) return;

    // Get references to the accounts
    Account& from_account = *accounts[from_id];
    Account& to_account = *accounts[to_id];

    // Lock accounts in consistent order to avoid deadlocks
    if (from_id < to_id) {
        from_account.mtx.lock();
        to_account.mtx.lock();
    } else {
        to_account.mtx.lock();
        from_account.mtx.lock();
    }

    // Perform the transfer
    from_account.balance -= amount;
    to_account.balance += amount;

    // Generate a unique serial number
    unsigned int sn = serial_number++;

    // Create an operation record
    OperationRecord op_record = { sn, amount, from_id, to_id };

    // Append the operation record to both accounts' logs
    from_account.log.push_back(op_record);
    to_account.log.push_back(op_record);

    // Unlock the accounts
    from_account.mtx.unlock();
    to_account.mtx.unlock();
}

void worker_thread(int num_operations, int num_accounts) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> account_dist(0, num_accounts - 1);
    std::uniform_int_distribution<> amount_dist(1, 100);

    for (int i = 0; i < num_operations; ++i) {
        int from_id = account_dist(gen);
        int to_id = account_dist(gen);
        int amount = amount_dist(gen);

        transfer(from_id, to_id, amount);
    }
}

void perform_consistency_check(int initial_balance) {
    bool consistent = true;

    // Lock all accounts for consistency check
    for (auto& account_ptr : accounts) {
        account_ptr->mtx.lock();
    }

    // Check balances and logs
    for (auto& account_ptr : accounts) {
        Account& account = *account_ptr;
        int calculated_balance = initial_balance;
        std::set<unsigned int> serial_numbers;

        // Build a set of serial numbers in this account's log
        for (auto& op : account.log) {
            serial_numbers.insert(op.serial_number);
            if (op.from_account_id == account.id) {
                calculated_balance -= op.amount;
            } else if (op.to_account_id == account.id) {
                calculated_balance += op.amount;
            }
        }

        if (calculated_balance != account.balance) {
            std::cout << "Inconsistency found in account " << account.id << std::endl;
            std::cout << "Expected balance: " << calculated_balance << ", Actual balance: " << account.balance << std::endl;
            consistent = false;
        }

        // Check that each operation appears in both accounts involved
        for (auto& op : account.log) {
            int other_account_id = (op.from_account_id == account.id) ? op.to_account_id : op.from_account_id;
            Account& other_account = *accounts[other_account_id];

            auto it = std::find_if(other_account.log.begin(), other_account.log.end(),
                [&op](const OperationRecord& o) {
                    return o.serial_number == op.serial_number;
                });

            if (it == other_account.log.end()) {
                std::cout << "Operation " << op.serial_number << " not found in account " << other_account_id << std::endl;
                consistent = false;
            }
        }
    }

    // Unlock accounts
    for (auto& account_ptr : accounts) {
        account_ptr->mtx.unlock();
    }

    if (consistent) {
        std::cout << "Consistency check passed." << std::endl;
    } else {
        std::cout << "Consistency check failed." << std::endl;
    }
}

// Function to write logs of each account to separate files
void write_account_logs_to_files() {
    for (auto& account_ptr : accounts) {
        Account& account = *account_ptr;

        // Lock the account mutex
        std::lock_guard<std::mutex> lg(account.mtx);

        // Sort the account's log based on serial number
        std::sort(account.log.begin(), account.log.end(), [](const OperationRecord& a, const OperationRecord& b) {
            return a.serial_number < b.serial_number;
        });

        // Build the filename
        std::ostringstream filename;
        filename << "account_" << account.id << "_logs.txt";

        std::ofstream outfile(filename.str());

        if (!outfile.is_open()) {
            std::cerr << "Error opening file for writing logs of account " << account.id << std::endl;
            continue;
        }

        // Write the logs to the file
        outfile << "Account " << account.id << " Transaction Logs:\n";
        for (const auto& op : account.log) {
            outfile << "Serial Number: " << op.serial_number
                    << ", Amount: " << op.amount
                    << ", From Account: " << op.from_account_id
                    << ", To Account: " << op.to_account_id << std::endl;
        }

        outfile.close();
    }
}

int main() {
    int num_accounts = 10;
    int initial_balance = 1000;

    // Initialize accounts
    for (int i = 0; i < num_accounts; ++i) {
        accounts.emplace_back(std::make_shared<Account>(i, initial_balance));
    }

    int num_threads = 4;
    int num_operations_per_thread = 1000;

    std::vector<std::thread> threads;

    auto start_time = std::chrono::steady_clock::now();

    std::atomic<bool> done_flag(false);
    std::thread checker_thread([&]() {
        while (!done_flag) {
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            perform_consistency_check(initial_balance);
        }
    });

    // Start worker threads
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker_thread, num_operations_per_thread, num_accounts);
    }

    // Wait for all worker threads to finish
    for (auto& t : threads) {
        t.join();
    }

    // Signal the consistency checker to stop
    done_flag = true;
    checker_thread.join();

    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    std::cout << "Total time: " << duration.count() << " ms" << std::endl;

    // Final consistency check
    perform_consistency_check(initial_balance);

    // Write the logs of each account to separate files
    write_account_logs_to_files();

    return 0;
}
