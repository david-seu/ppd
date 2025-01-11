#include <iostream>
#include <vector>
#include <mpi.h>
#include <cassert>
#include <algorithm>
#include <cstdlib>

using namespace std;

const int MAXVALUE = 100;

// Brute force polynomial multiplication
inline void brute(const int *a, const int *b, int *ret, int n) {
    fill(ret, ret + 2 * n - 1, 0); // Corrected to 2n -1
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            ret[i + j] += a[i] * b[j];
        }
    }
}

// Karatsuba polynomial multiplication
void karatsuba(const int *a, const int *b, int *ret, int n) {
    if (n <= 4) {
        brute(a, b, ret, n);
        return;
    }

    int mid = n / 2;
    // Allocate temporary storage with size 2*mid -1
    vector<int> x1(2 * mid - 1, 0);
    vector<int> x2(2 * mid - 1, 0);
    vector<int> x3(2 * mid - 1, 0);
    vector<int> asum(mid, 0);
    vector<int> bsum(mid, 0);

    // Calculate a_low + a_high and b_low + b_high
    for (int i = 0; i < mid; ++i) {
        asum[i] = a[i] + a[mid + i];
        bsum[i] = b[i] + b[mid + i];
    }

    // Recursive calls
    karatsuba(a, b, x1.data(), mid);
    karatsuba(a + mid, b + mid, x2.data(), mid);
    karatsuba(asum.data(), bsum.data(), x3.data(), mid);

    // Combine results
    for (int i = 0; i < 2 * mid - 1; ++i) {
        x3[i] -= x1[i] + x2[i];
    }

    // Assemble the final result
    fill(ret, ret + 2 * n - 1, 0); // Corrected to 2n -1
    for (int i = 0; i < 2 * mid - 1; ++i) {
        ret[i] += x1[i];
        ret[mid + i] += x3[i];
        ret[2 * mid + i] += x2[i];
    }
}

// Generate random polynomials
void generate_poly(vector<int> &a, vector<int> &b, unsigned n) {
    a.resize(n, 0);
    b.resize(n, 0);
    for (unsigned i = 0; i < n; ++i) {
        a[i] = rand() % MAXVALUE;
        b[i] = rand() % MAXVALUE;
    }
}

// Check the result
void check_result(const vector<int> &a, const vector<int> &b, const vector<int> &res) {
    vector<int> expected(a.size() + b.size() - 1, 0);
    for (size_t i = 0; i < a.size(); ++i)
        for (size_t j = 0; j < b.size(); ++j)
            expected[i + j] += a[i] * b[j];
    assert(expected.size() == res.size());
    for (size_t i = 0; i < expected.size(); ++i)
        assert(expected[i] == res[i]);
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int me, nrProcs;
    MPI_Comm_rank(MPI_COMM_WORLD, &me);
    MPI_Comm_size(MPI_COMM_WORLD, &nrProcs);

    unsigned int n;
    if (argc != 2 || sscanf(argv[1], "%u", &n) != 1) {
        if (me == 0) {
            fprintf(stderr, "Usage: mpi_karatsuba <n>\n");
        }
        MPI_Finalize();
        return 1;
    }

    // Ensure n is a power of two for Karatsuba
    unsigned int original_n = n;
    if ((n & (n - 1)) != 0) {
        unsigned int power = 1;
        while (power < n) power <<= 1;
        n = power;
    }

    vector<int> a, b, res;
    if (me == 0) {
        generate_poly(a, b, original_n);
        // Pad with zeros to the next power of two
        a.resize(n, 0);
        b.resize(n, 0);
        fprintf(stderr, "> Master: Input generated and padded to %u\n", n);
    }

    // Broadcast the padded size to all processes
    MPI_Bcast(&n, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);

    // Resize vectors on all processes
    if (me != 0) { // Non-master processes need to resize
        a.resize(n, 0);
        b.resize(n, 0);
    }

    // Calculate send counts and displacements for 'a'
    vector<int> send_counts(nrProcs, n / nrProcs);
    vector<int> displs(nrProcs, 0);
    int remainder = n % nrProcs;
    for (int i = 0; i < remainder; ++i) {
        send_counts[i]++;
    }
    for (int i = 1; i < nrProcs; ++i) {
        displs[i] = displs[i - 1] + send_counts[i - 1];
    }

    // Allocate receive buffer for each process
    int local_n = send_counts[me];
    vector<int> local_a_chunk(local_n, 0);

    // Scatter 'a' polynomial
    MPI_Scatterv(a.data(), send_counts.data(), displs.data(), MPI_INT,
                 local_a_chunk.data(), local_n, MPI_INT, 0, MPI_COMM_WORLD);

    // Broadcast 'b' polynomial to all processes
    MPI_Bcast(b.data(), n, MPI_INT, 0, MPI_COMM_WORLD);

    // Each process prepares a padded 'a' with its chunk positioned correctly
    vector<int> local_a_padded(n, 0);
    for(int i = 0; i < local_n; ++i){
        local_a_padded[displs[me] + i] = local_a_chunk[i];
    }

    // Perform Karatsuba multiplication
    // The result size is 2n -1
    vector<int> local_res(2 * n - 1, 0); // Corrected to 2n -1
    karatsuba(local_a_padded.data(), b.data(), local_res.data(), n); // n is the size after padding

    // Gather all partial results to the master
    // Each partial result is of size 2n -1
    vector<int> all_partial_res;
    if (me == 0) {
        all_partial_res.resize(nrProcs * (2 * n - 1), 0);
    }

    MPI_Gather(local_res.data(), 2 * n - 1, MPI_INT,
               all_partial_res.data(), 2 * n - 1, MPI_INT,
               0, MPI_COMM_WORLD);

    if (me == 0) {
        // Initialize the final result vector
        res.assign(2 * n - 1, 0);

        // Sum all partial results with proper offsets
        for (int p = 0; p < nrProcs; ++p) {
            int st = displs[p];
            for(unsigned int i = 0; i < 2 * n - 1; ++i){
                if (st + i < res.size()) { // Prevent out-of-bounds
                    res[st + i] += all_partial_res[p * (2 * n - 1) + i];
                }
            }
        }

        // Check the result
        check_result(a, b, res);

        // Optionally, print a success message
        cout << "Polynomial multiplication successful and verified.\n";

        // Optionally, print the result
        /*
        for (size_t i = 0; i < res.size(); ++i) {
            cout << res[i] << ' ';
        }
        cout << endl;
        */
    }

    MPI_Finalize();
    return 0;
}
