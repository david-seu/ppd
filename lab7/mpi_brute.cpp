#include <iostream>
#include <vector>
#include <mpi.h>
#include <cstdlib>
#include <cassert>
#include <algorithm>

// Constants
constexpr int MAX_COEFFICIENT = 100;

// MPI Message Tags
enum MPI_TAGS {
    TAG_SIZE = 0,
    TAG_START = 1,
    TAG_END = 2,
    TAG_POLY_A = 3,
    TAG_POLY_B = 4,
    TAG_RESULT = 5
};

// Function Prototypes
void brute_force_multiply(const int* a, const int* b, int* result, int n);
void karatsuba_multiply(const int* a, const int* b, int* result, int n, int* temp);
void generate_polynomials(std::vector<int>& a, std::vector<int>& b, unsigned n);
void distribute_work(const std::vector<int>& a, const std::vector<int>& b, int num_procs);
void gather_results(int n, int num_procs, std::vector<int>& result);
void verify_result(const std::vector<int>& a, const std::vector<int>& b, const std::vector<int>& result);
void worker_process(int rank);
void master_process(unsigned n, int num_procs);

// Brute Force Polynomial Multiplication
void brute_force_multiply(const int* a, const int* b, int* result, int n) {
    std::fill(result, result + 2 * n, 0);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            result[i + j] += a[i] * b[j];
        }
    }
}

// Karatsuba Polynomial Multiplication
void karatsuba_multiply(const int* a, const int* b, int* result, int n, int* temp) {
    if (n <= 4) {
        brute_force_multiply(a, b, result, n);
        return;
    }

    int mid = n / 2;

    const int* a_low = a;
    const int* a_high = a + mid;
    const int* b_low = b;
    const int* b_high = b + mid;

    int* sum_a = temp;
    int* sum_b = temp + mid;

    // Calculate sums of the low and high parts
    for (int i = 0; i < mid; ++i) {
        sum_a[i] = a_high[i] + a_low[i];
        sum_b[i] = b_high[i] + b_low[i];
    }

    // Allocate space in result for intermediate products
    int* z0 = result;                  // a_low * b_low
    int* z1 = result + n;              // a_high * b_high
    int* z2 = result + 2 * n;          // (a_low + a_high) * (b_low + b_high)

    // Recursive calls
    karatsuba_multiply(a_low, b_low, z0, mid, temp + 2 * mid);
    karatsuba_multiply(a_high, b_high, z1, mid, temp + 2 * mid);
    karatsuba_multiply(sum_a, sum_b, z2, mid, temp + 2 * mid);

    // Combine the results
    for (int i = 0; i < n; ++i) {
        z2[i] -= z0[i] + z1[i];
    }
    for (int i = 0; i < n; ++i) {
        result[i + mid] += z2[i];
    }
}

// Generate Random Polynomials
void generate_polynomials(std::vector<int>& a, std::vector<int>& b, unsigned n) {
    a.resize(n, 0);
    b.resize(n, 0);
    for (unsigned i = 0; i < n; ++i) {
        a[i] = rand() % MAX_COEFFICIENT;
        b[i] = rand() % MAX_COEFFICIENT;
    }
}

// Master: Distribute Work to Slaves
void distribute_work(const std::vector<int>& a, const std::vector<int>& b, int num_procs) {
    int n = a.size();
    for (int rank = 1; rank < num_procs; ++rank) {
        int start = rank * n / num_procs;
        int end = std::min(n, (rank + 1) * n / num_procs);

        // Send size of polynomials
        MPI_Send(&n, 1, MPI_INT, rank, TAG_SIZE, MPI_COMM_WORLD);
        // Send start and end indices
        MPI_Send(&start, 1, MPI_INT, rank, TAG_START, MPI_COMM_WORLD);
        MPI_Send(&end, 1, MPI_INT, rank, TAG_END, MPI_COMM_WORLD);
        // Send segment of polynomial A
        MPI_Send(a.data() + start, end - start, MPI_INT, rank, TAG_POLY_A, MPI_COMM_WORLD);
        // Send entire polynomial B
        MPI_Send(b.data(), n, MPI_INT, rank, TAG_POLY_B, MPI_COMM_WORLD);
    }
}

// Master: Gather Results from Slaves
void gather_results(int n, int num_procs, std::vector<int>& result) {
    std::vector<int> temp_result(2 * n, 0);
    for (int rank = 1; rank < num_procs; ++rank) {
        std::vector<int> partial_result(2 * n, 0);
        MPI_Status status;
        MPI_Recv(partial_result.data(), 2 * n, MPI_INT, rank, TAG_RESULT, MPI_COMM_WORLD, &status);
        for (int i = 0; i < 2 * n; ++i) {
            temp_result[i] += partial_result[i];
        }
    }

    // Combine master's own computation
    for (int i = 0; i < 2 * n; ++i) {
        result[i] += temp_result[i];
    }
}

// Master: Verify the Result
void verify_result(const std::vector<int>& a, const std::vector<int>& b, const std::vector<int>& result) {
    std::vector<int> expected(a.size() + b.size() - 1, 0);
    for (size_t i = 0; i < a.size(); ++i) {
        for (size_t j = 0; j < b.size(); ++j) {
            expected[i + j] += a[i] * b[j];
        }
    }
    assert(expected.size() == result.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        assert(expected[i] == result[i]);
    }
    std::cerr << "> Verification passed: The result is correct.\n";
}

// Worker: Receive Data, Compute, and Send Back Result
void worker_process(int rank) {
    std::cerr << "> Worker " << rank << " started.\n";

    // Receive size of polynomials
    int n;
    MPI_Recv(&n, 1, MPI_INT, 0, TAG_SIZE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Receive start and end indices
    int start, end;
    MPI_Recv(&start, 1, MPI_INT, 0, TAG_START, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&end, 1, MPI_INT, 0, TAG_END, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Receive segment of polynomial A
    std::vector<int> a_segment(end - start, 0);
    MPI_Recv(a_segment.data(), end - start, MPI_INT, 0, TAG_POLY_A, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Receive entire polynomial B
    std::vector<int> b(n, 0);
    MPI_Recv(b.data(), n, MPI_INT, 0, TAG_POLY_B, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Prepare full polynomials with zeros outside the segment
    std::vector<int> a_full(n, 0);
    std::copy(a_segment.begin(), a_segment.end(), a_full.begin() + start);

    // Allocate result array (size 2n)
    std::vector<int> result(2 * n, 0);

    // Allocate temporary space for Karatsuba
    std::vector<int> temp(3 * (n / 2), 0); // Adjust size as needed

    // Perform multiplication
    karatsuba_multiply(a_full.data(), b.data(), result.data(), n, temp.data());

    // Send back the result
    MPI_Send(result.data(), 2 * n, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);

    std::cerr << "> Worker " << rank << " finished computation.\n";
}

// Master: Execute Multiplication and Coordination
void master_process(unsigned n, int num_procs) {
    // Generate polynomials
    std::vector<int> poly_a, poly_b;
    generate_polynomials(poly_a, poly_b, n);

    // Adjust n to the next power of two
    unsigned original_n = n;
    while ((n & (n - 1)) != 0) {
        poly_a.push_back(0);
        poly_b.push_back(0);
        ++n;
    }

    std::cerr << "> Master: Polynomials generated and padded to size " << n << ".\n";

    // Distribute work to workers
    distribute_work(poly_a, poly_b, num_procs);
    std::cerr << "> Master: Work distributed to workers.\n";

    // Master computes its own part
    int start = 0;
    int end = n / num_procs;
    std::vector<int> a_segment(poly_a.begin() + start, poly_a.begin() + end);
    std::vector<int> b_full = poly_b; // Entire polynomial B

    // Prepare full polynomial A with zeros outside the segment
    std::vector<int> a_full(n, 0);
    std::copy(a_segment.begin(), a_segment.end(), a_full.begin() + start);

    // Allocate result array
    std::vector<int> final_result(2 * n, 0);

    // Allocate temporary space for Karatsuba
    std::vector<int> temp(3 * (n / 2), 0); // Adjust size as needed

    // Perform multiplication
    karatsuba_multiply(a_full.data(), b_full.data(), final_result.data(), n, temp.data());

    // Gather results from workers
    gather_results(n, num_procs, final_result);
    std::cerr << "> Master: Results gathered from workers.\n";

    // Resize result to original size before padding
    final_result.resize(2 * original_n - 1);

    // Verify the result
    verify_result(poly_a, poly_b, final_result);
}

// Main Function
int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    if (argc != 2) {
        if (rank == 0) {
            std::cerr << "Usage: mpirun -n <num_procs> " << argv[0] << " <polynomial_size>\n";
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    unsigned n;
    try {
        n = std::stoul(argv[1]);
    } catch (const std::exception& e) {
        if (rank == 0) {
            std::cerr << "Invalid polynomial size: " << argv[1] << "\n";
        }
        MPI_Finalize();
        return EXIT_FAILURE;
    }

    if (rank == 0) {
        master_process(n, num_procs);
    } else {
        worker_process(rank);
    }

    MPI_Finalize();
    return EXIT_SUCCESS;
}
