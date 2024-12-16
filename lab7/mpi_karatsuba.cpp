#include <iostream>
#include <fstream>
#include <vector>
#include <mpi.h>
#include <ctime>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <algorithm>

using namespace std;

const int MAXVALUE = 100;

/**
 * @brief Brute force multiplication of two polynomials a and b.
 * 
 * @param a Input polynomial A.
 * @param b Input polynomial B.
 * @param ret Output array for the result (size must be at least 2*n).
 * @param n Size of A and B.
 */
void brute(const int *a, const int *b, int *ret, int n) {
    for (int i = 0; i < 2 * n; ++i) {
        ret[i] = 0;
    }
    for (int i = 0; i < n; ++i) {
        int ai = a[i];
        for (int j = 0; j < n; ++j) {
            ret[i + j] += ai * b[j];
        }
    }
}

/**
 * @brief Karatsuba multiplication of polynomials.
 * 
 * Uses extra space in ret to store intermediate sums for recursion. The layout:
 * - x1 at ret[n*0]
 * - x2 at ret[n*1]
 * - x3 at ret[n*2]
 * - asum at ret[n*5]
 * - bsum at ret[n*5 + n/2]
 *
 * @param a Input polynomial A.
 * @param b Input polynomial B.
 * @param ret Workspace and output, must have size at least 6*n.
 * @param n Size of A and B (power-of-two padded).
 */
void karatsuba(int *a, int *b, int *ret, int n) {
    if (n <= 4) {
        brute(a, b, ret, n);
        return;
    }

    int half = n / 2;
    int *ar = a;
    int *al = a + half;
    int *br = b;
    int *bl = b + half;

    int *asum = &ret[5 * n];       // sum of a's halves
    int *bsum = &ret[5 * n + half];// sum of b's halves
    int *x1   = &ret[0 * n];       // ar*br
    int *x2   = &ret[1 * n];       // al*bl
    int *x3   = &ret[2 * n];       // (ar+al)*(br+bl)

    for (int i = 0; i < half; ++i) {
        asum[i] = ar[i] + al[i];
        bsum[i] = br[i] + bl[i];
    }

    karatsuba(ar, br, x1, half);
    karatsuba(al, bl, x2, half);
    karatsuba(asum, bsum, x3, half);

    for (int i = 0; i < n; i++) {
        x3[i] = x3[i] - x1[i] - x2[i];
    }

    for (int i = 0; i < n; i++) {
        ret[i + half] += x3[i];
    }
}

/**
 * @brief Generate two polynomials of size n with random coefficients [0,99].
 */
void generate_poly(vector<int> &a, vector<int> &b, unsigned n) {
    a.resize(n);
    b.resize(n);
    for (unsigned i = 0; i < n; ++i) {
        a[i] = rand() % MAXVALUE;
        b[i] = rand() % MAXVALUE;
    }
}

/**
 * @brief Send work from master to slaves.
 * 
 * Master sends the full polynomial B and the partial segment of A needed for each slave.
 */
void send_work(const vector<int> &a, const vector<int> &b, int nrProcs) {
    int n = (int) a.size();
    for (int rank = 1; rank < nrProcs; ++rank) {
        int st = rank * n / nrProcs;
        int dr = (int)min<unsigned>(n, (unsigned)((rank + 1) * n / nrProcs));
        MPI_Send(&n, 1, MPI_INT, rank, 0, MPI_COMM_WORLD);
        MPI_Send(&st, 1, MPI_INT, rank, 1, MPI_COMM_WORLD);
        MPI_Send(&dr, 1, MPI_INT, rank, 2, MPI_COMM_WORLD);
        MPI_Send(a.data() + st, dr - st, MPI_INT, rank, 3, MPI_COMM_WORLD);
        MPI_Send(b.data(), n, MPI_INT, rank, 4, MPI_COMM_WORLD);
    }
}

/**
 * @brief Compute the result using Karatsuba on the master's assigned segment.
 */
void master_compute(int st, int dr, const vector<int> &a, const vector<int> &b, vector<int> &res) {
    // Since this code currently just does the full karatsuba on the entire A and B,
    // the st and dr parameters are not used to limit computation.
    karatsuba((int*)a.data(), (int*)b.data(), res.data(), (int)a.size());
}

/**
 * @brief Collect results from slaves and accumulate them into the master result.
 */
void collect(int n, int nrProcs, vector<int> &res) {
    vector<int> aux(2 * n - 1);
    for (int rank = 1; rank < nrProcs; ++rank) {
        MPI_Status status;
        MPI_Recv(aux.data(), (int)aux.size(), MPI_INT, rank, 5, MPI_COMM_WORLD, &status);
        for (size_t i = 0; i < aux.size(); ++i) {
            res[i] += aux[i];
        }
    }
}

/**
 * @brief Check correctness by brute force and assert correctness.
 */
void verify_result(const vector<int> &a, const vector<int> &b, const vector<int> &res) {
    vector<int> check(a.size() + b.size() - 1, 0);
    for (size_t i = 0; i < a.size(); ++i) {
        int ai = a[i];
        for (size_t j = 0; j < b.size(); ++j) {
            check[i + j] += ai * b[j];
        }
    }
    assert(check.size() == res.size());
    for (size_t i = 0; i < check.size(); ++i) {
        assert(check[i] == res[i]);
    }
}

/**
 * @brief Slave process routine: receive assigned segment and compute partial results.
 */
void slave_process(int me) {
    int n, st, dr;
    MPI_Status status;
    MPI_Recv(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    MPI_Recv(&st, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
    MPI_Recv(&dr, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, &status);

    vector<int> a(n, 0);
    vector<int> b(n, 0);
    MPI_Recv(a.data() + st, dr - st, MPI_INT, 0, 3, MPI_COMM_WORLD, &status);
    MPI_Recv(b.data(), n, MPI_INT, 0, 4, MPI_COMM_WORLD, &status);

    // Prepare result array
    vector<int> res(6 * n, 0);
    // Compute full karatsuba - no partial logic implemented here, it computes full and merges later
    karatsuba(a.data(), b.data(), res.data(), n);

    // Send partial results (2*n - 1 is the final poly length)
    MPI_Send(res.data(), 2 * n - 1, MPI_INT, 0, 5, MPI_COMM_WORLD);
}


int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int me, nrProcs;
    MPI_Comm_rank(MPI_COMM_WORLD, &me);
    MPI_Comm_size(MPI_COMM_WORLD, &nrProcs);
    srand((unsigned)time(NULL) + me);

    if (argc != 2) {
        if (me == 0) {
            fprintf(stderr, "usage: mpi_kar <n>\n");
        }
        MPI_Finalize();
        return 1;
    }

    unsigned int n;
    if (sscanf(argv[1], "%u", &n) != 1) {
        if (me == 0) {
            fprintf(stderr, "Invalid input\n");
        }
        MPI_Finalize();
        return 1;
    }

    if (me == 0) {
        // Master process
        vector<int> a, b;
        generate_poly(a, b, n);

        // Pad n to next power of two if needed
        while (n & (n - 1)) {
            ++n;
            a.push_back(0);
            b.push_back(0);
        }

        send_work(a, b, nrProcs);

        int st = 0;
        int dr = (nrProcs > 1) ? (int)(n / nrProcs) : (int)n;
        vector<int> aux(a);
        // Zero out unused part of aux
        for (int i = dr; i < (int)aux.size(); ++i) {
            aux[i] = 0;
        }

        vector<int> res(6 * n, 0);
        master_compute(st, dr, aux, b, res);
        collect(n, nrProcs, res);

        // Resize to final size of polynomial (2*n - 1)
        res.resize(2 * n - 1);
        verify_result(a, b, res);
        cout << "Result verified successfully.\n";
    } else {
        // Slave process
        slave_process(me);
    }

    MPI_Finalize();
    return 0;
}
