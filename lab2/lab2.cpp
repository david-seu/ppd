#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <random>
#include <cassert>

std::mutex mtx;
std::condition_variable cond_var_producer;
std::condition_variable cond_var_consumer;

const size_t BUFFER_SIZE = 1000;

// Buffer to hold products
std::queue<long long> buffer;

// Flags to indicate production and consumption status
bool production_complete = false;

// Accumulated sum
long long sum = 0;

void producer(const std::vector<int>& A, const std::vector<int>& B) {
    size_t size_vectors = A.size();

    for (size_t i = 0; i < size_vectors; ++i) {
        long long product = static_cast<long long>(A[i]) * B[i];

        // Acquire lock before accessing the buffer
        std::unique_lock<std::mutex> lock(mtx);

        // Wait if buffer is full
        cond_var_producer.wait(lock, [] { return buffer.size() < BUFFER_SIZE; });

        // Push the product into the buffer
        buffer.push(product);

        // Notify the consumer that a new product is available
        cond_var_consumer.notify_one();
    }

    // After producing all products, set the production_complete flag
    std::unique_lock<std::mutex> lock(mtx);
    production_complete = true;

    // Notify the consumer in case it's waiting
    cond_var_consumer.notify_one();
}

void consumer() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx);

        // Wait until there's data in the buffer or production is complete
        cond_var_consumer.wait(lock, [] { return !buffer.empty() || production_complete; });

        // Process all available products
        while (!buffer.empty()) {
            long long product = buffer.front();
            buffer.pop();
            sum += product;
        }

        // If production is complete and buffer is empty, exit
        if (production_complete && buffer.empty()) {
            break;
        }

        // Notify the producer that buffer space is available
        cond_var_producer.notify_one();
    }
}

int main() {
    const size_t VECTOR_SIZE = 10000000;

    // Initialize vectors A and B with random integers
    std::vector<int> A(VECTOR_SIZE);
    std::vector<int> B(VECTOR_SIZE);

    // Use random number generation for vector initialization
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(1, 100);

    for (size_t i = 0; i < VECTOR_SIZE; ++i) {
        A[i] = dist(rng);
        B[i] = dist(rng);
    }

    // Compute scalar product using producer-consumer threads
    auto start_time = std::chrono::high_resolution_clock::now();

    std::thread prod_thread(producer, std::cref(A), std::cref(B));
    std::thread cons_thread(consumer);

    prod_thread.join();
    cons_thread.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    // Compute scalar product using single-threaded approach for verification
    auto verify_start = std::chrono::high_resolution_clock::now();

    long long expected_sum = 0;
    for (size_t i = 0; i < VECTOR_SIZE; ++i) {
        expected_sum += static_cast<long long>(A[i]) * B[i];
    }

    auto verify_end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> verify_elapsed = verify_end - verify_start;

    // Verify the result
    assert(sum == expected_sum);
    std::cout << "Verification passed: " << sum << " == " << expected_sum << std::endl;

    // Output the results
    std::cout << "Producer-Consumer Scalar Product: " << sum << std::endl;
    std::cout << "Time taken (Producer-Consumer): " << elapsed.count() << " seconds" << std::endl;
    std::cout << "Time taken (Single-threaded): " << verify_elapsed.count() << " seconds" << std::endl;

    return 0;
}
