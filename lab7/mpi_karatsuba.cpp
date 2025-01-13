#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <algorithm>
#include "mpi.h"

// Function to generate a random integer vector
std::vector<int> generate_random_vector(size_t size, int min_value = 0, int max_value = 100) {
    std::random_device rand_dev;
    std::mt19937 rand_gen(rand_dev());
    std::uniform_int_distribution<int> dist(min_value, max_value);
    std::vector<int> rand_vec(size);
    std::generate(rand_vec.begin(), rand_vec.end(), [&]() { return dist(rand_gen); });
    return rand_vec;
}

// Naive multiplication of two vectors
std::vector<int> multiply_naive(const std::vector<int>& vec_a, const std::vector<int>& vec_b) {
    size_t size_a = vec_a.size(), size_b = vec_b.size();
    std::vector<int> result(size_a + size_b - 1, 0);
    for (size_t i = 0; i < size_a; ++i) {
        for (size_t j = 0; j < size_b; ++j) {
            result[i + j] += vec_a[i] * vec_b[j];
        }
    }
    return result;
}

// Karatsuba multiplication of two vectors
std::vector<int> multiply_karatsuba(const std::vector<int>& vec_a, const std::vector<int>& vec_b) {
    int size = vec_a.size();
    if (size < 64) return multiply_naive(vec_a, vec_b);

    int mid = size / 2;
    std::vector<int> vec_a_low(vec_a.begin(), vec_a.begin() + mid);
    std::vector<int> vec_a_high(vec_a.begin() + mid, vec_a.end());
    std::vector<int> vec_b_low(vec_b.begin(), vec_b.begin() + mid);
    std::vector<int> vec_b_high(vec_b.begin() + mid, vec_b.end());

    auto low_result = multiply_karatsuba(vec_a_low, vec_b_low);
    auto high_result = multiply_karatsuba(vec_a_high, vec_b_high);

    std::vector<int> vec_a_sum(vec_a_low.size(), 0), vec_b_sum(vec_b_low.size(), 0);
    for (int i = 0; i < mid; ++i) {
        vec_a_sum[i] = vec_a_low[i] + vec_a_high[i];
        vec_b_sum[i] = vec_b_low[i] + vec_b_high[i];
    }

    auto middle_result = multiply_karatsuba(vec_a_sum, vec_b_sum);
    std::vector<int> result(2 * size - 1, 0);

    for (size_t i = 0; i < low_result.size(); ++i) result[i] += low_result[i];
    for (size_t i = 0; i < high_result.size(); ++i) result[i + 2 * mid] += high_result[i];
    for (size_t i = 0; i < middle_result.size(); ++i) {
        int temp = middle_result[i];
        if (i < low_result.size()) temp -= low_result[i];
        if (i < high_result.size()) temp -= high_result[i];
        result[i + mid] += temp;
    }

    return result;
}

// Recursive Karatsuba multiplication using MPI
void karatsuba_recursive_mpi(const std::vector<int>& vec_a, const std::vector<int>& vec_b,
                             int rank, int process_count, std::vector<int>& result) {
    int size = vec_a.size();
    if (size == 1) {
        result[0] = vec_a[0] * vec_b[0];
        return;
    }

    int mid = size / 2, mid_left = size - mid;
    std::vector<int> vec_a_low(vec_a.begin(), vec_a.begin() + mid);
    std::vector<int> vec_a_high(vec_a.begin() + mid, vec_a.end());
    std::vector<int> vec_b_low(vec_b.begin(), vec_b.begin() + mid);
    std::vector<int> vec_b_high(vec_b.begin() + mid, vec_b.end());

    std::vector<int> low_result(2 * mid - 1, 0);
    std::vector<int> high_result(2 * mid_left - 1, 0);
    std::vector<int> middle_result(2 * mid_left - 1, 0);

    std::vector<int> vec_a_sum(mid_left, 0), vec_b_sum(mid_left, 0);
    for (int i = 0; i < mid_left; ++i) {
        vec_a_sum[i] = (i < mid ? vec_a_low[i] : 0) + (i < static_cast<int>(vec_a_high.size()) ? vec_a_high[i] : 0);
        vec_b_sum[i] = (i < mid ? vec_b_low[i] : 0) + (i < static_cast<int>(vec_b_high.size()) ? vec_b_high[i] : 0);
    }

    int child1 = rank + process_count / 3;
    int child2 = rank + 2 * process_count / 3;
    int size1 = child2 - child1;
    int size2 = (rank + process_count) - child2;

    if (process_count == 2) {
        MPI_Send(&mid, 1, MPI_INT, child2, 0, MPI_COMM_WORLD);
        int zero = 0;
        MPI_Send(&zero, 1, MPI_INT, child2, 0, MPI_COMM_WORLD);
        MPI_Send(vec_a_low.data(), mid, MPI_INT, child2, 0, MPI_COMM_WORLD);
        MPI_Send(vec_b_low.data(), mid, MPI_INT, child2, 0, MPI_COMM_WORLD);

        high_result = multiply_karatsuba(vec_a_high, vec_b_high);
        middle_result = multiply_karatsuba(vec_a_sum, vec_b_sum);

        MPI_Status status;
        MPI_Recv(low_result.data(), static_cast<int>(low_result.size()), MPI_INT, child2, 0,
                 MPI_COMM_WORLD, &status);
    } else if (child1 < process_count && child2 < process_count) {
        MPI_Send(&mid, 1, MPI_INT, child1, 0, MPI_COMM_WORLD);
        MPI_Send(&mid_left, 1, MPI_INT, child2, 0, MPI_COMM_WORLD);
        MPI_Send(&size1, 1, MPI_INT, child1, 0, MPI_COMM_WORLD);
        MPI_Send(&size2, 1, MPI_INT, child2, 0, MPI_COMM_WORLD);

        MPI_Send(vec_a_low.data(), mid, MPI_INT, child1, 0, MPI_COMM_WORLD);
        MPI_Send(vec_b_low.data(), mid, MPI_INT, child1, 0, MPI_COMM_WORLD);

        MPI_Send(vec_a_high.data(), mid_left, MPI_INT, child2, 0, MPI_COMM_WORLD);
        MPI_Send(vec_b_high.data(), mid_left, MPI_INT, child2, 0, MPI_COMM_WORLD);

        middle_result = multiply_karatsuba(vec_a_sum, vec_b_sum);

        MPI_Status status;
        MPI_Recv(low_result.data(), static_cast<int>(low_result.size()), MPI_INT, child1, 0, MPI_COMM_WORLD, &status);
        MPI_Recv(high_result.data(), static_cast<int>(high_result.size()), MPI_INT, child2, 0, MPI_COMM_WORLD, &status);
    } else {
        low_result = multiply_karatsuba(vec_a_low, vec_b_low);
        high_result = multiply_karatsuba(vec_a_high, vec_b_high);
        middle_result = multiply_karatsuba(vec_a_sum, vec_b_sum);
    }

    for (size_t i = 0; i < low_result.size(); ++i) result[i] += low_result[i];
    for (size_t i = 0; i < high_result.size(); ++i) result[i + 2 * mid] += high_result[i];
    for (size_t i = 0; i < middle_result.size(); ++i) {
        int temp = middle_result[i];
        if (i < low_result.size()) temp -= low_result[i];
        if (i < high_result.size()) temp -= high_result[i];
        result[i + mid] += temp;
    }
}

// MPI worker for Karatsuba multiplication
void karatsuba_mpi_worker(int rank) {
    MPI_Status status;
    int data_size = 0, new_size = 0;

    MPI_Recv(&data_size, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
    int parent = status.MPI_SOURCE;
    MPI_Recv(&new_size, 1, MPI_INT, parent, 0, MPI_COMM_WORLD, &status);

    std::vector<int> vec_a(data_size), vec_b(data_size);
    MPI_Recv(vec_a.data(), data_size, MPI_INT, parent, 0, MPI_COMM_WORLD, &status);
    MPI_Recv(vec_b.data(), data_size, MPI_INT, parent, 0, MPI_COMM_WORLD, &status);

    std::vector<int> result(2 * data_size - 1, 0);
    karatsuba_recursive_mpi(vec_a, vec_b, rank, new_size, result);

    MPI_Send(result.data(), static_cast<int>(result.size()), MPI_INT, parent, 0, MPI_COMM_WORLD);
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, process_count;
    MPI_Comm_size(MPI_COMM_WORLD, &process_count);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int data_size = 10000;

    if (rank == 0) {
        std::vector<int> vec_a = generate_random_vector(data_size);
        std::vector<int> vec_b = generate_random_vector(data_size);

        auto start_time = std::chrono::high_resolution_clock::now();
        std::vector<int> result(2 * data_size - 1, 0);
        karatsuba_recursive_mpi(vec_a, vec_b, rank, process_count, result);
        auto end_time = std::chrono::high_resolution_clock::now();
        std::cout << "MPI Karatsuba Time: " 
                  << std::chrono::duration<double>(end_time - start_time).count() << " seconds\n";

        start_time = std::chrono::high_resolution_clock::now();
        auto naive_result = multiply_naive(vec_a, vec_b);
        end_time = std::chrono::high_resolution_clock::now();
        std::cout << "Naive Multiplication Time: " 
                  << std::chrono::duration<double>(end_time - start_time).count() << " seconds\n";

        std::cout << "Results equal: " << std::boolalpha << (result == naive_result) << "\n";
    } else {
        karatsuba_mpi_worker(rank);
    }

    MPI_Finalize();
    return 0;
}
