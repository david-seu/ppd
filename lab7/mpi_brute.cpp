#include <iostream>
#include <fstream>
#include <mpi.h>
#include <vector>
#include <ctime>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <algorithm>
#include <chrono>

using namespace std;

void generatePolynomials(vector<int> &a, vector<int> &b, unsigned n) {
    a.resize(n);
    b.resize(n);
    for (unsigned i = 0; i < n; ++i) {
        a[i] = rand() % 100; 
        b[i] = rand() % 100; 
    }
}


void distributeWork(const vector<int> &a, const vector<int> &b, int nrProcs) {
    int n = (int) a.size();
    int resultLength = (int)(a.size() + b.size() - 1);

    for (int proc = 1; proc < nrProcs; ++proc) {
        int startIndex = proc * resultLength / nrProcs;
        int endIndex = min(resultLength, (proc + 1) * resultLength / nrProcs);

        MPI_Send(&n, 1, MPI_INT, proc, 0, MPI_COMM_WORLD);
        MPI_Send(&startIndex, 1, MPI_INT, proc, 1, MPI_COMM_WORLD);
        MPI_Send(&endIndex, 1, MPI_INT, proc, 2, MPI_COMM_WORLD);
        MPI_Send(a.data(), min(endIndex, n), MPI_INT, proc, 3, MPI_COMM_WORLD);
        MPI_Send(b.data(), min(endIndex, n), MPI_INT, proc, 4, MPI_COMM_WORLD);
    }
}


void computeSegment(int st, int dr, const vector<int> &a, const vector<int> &b, vector<int> &res) {
    int aSize = (int) a.size();
    int bSize = (int) b.size();

    for (int i = st; i < dr; ++i) {
        int startX = max(0, i - (bSize - 1));
        int endX = min(i, aSize - 1);

        int sum = 0;
        for (int x = startX; x <= endX; ++x) {
            int y = i - x; 
            sum += a[x] * b[y];
        }
        res[i - st] = sum;
    }
}


void collectResults(int nrProcs, vector<int> &res) {
    int length = (int) res.size();

    for (int proc = 1; proc < nrProcs; ++proc) {
        MPI_Status status;
        int startIndex = proc * length / nrProcs;
        int endIndex = min(length, (proc + 1) * length / nrProcs);
        MPI_Recv(res.data() + startIndex, endIndex - startIndex, MPI_INT, proc, 5, MPI_COMM_WORLD, &status);
    }
}


bool validateResult(const vector<int> &a, const vector<int> &b, const vector<int> &res) {
    vector<int> expected(a.size() + b.size() - 1, 0);

    for (int i = 0; i < (int) a.size(); ++i) {
        for (int j = 0; j < (int) b.size(); ++j) {
            expected[i + j] += a[i] * b[j];
        }
    }

    if (expected.size() != res.size()) return false;

    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i] != res[i]) return false;
    }

    return true;
}


void slaveProcess(int me) {
    int n, startIndex, endIndex;
    MPI_Status status;

    MPI_Recv(&n, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    MPI_Recv(&startIndex, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
    MPI_Recv(&endIndex, 1, MPI_INT, 0, 2, MPI_COMM_WORLD, &status);

    int aCount = min(endIndex, n);
    int bCount = min(endIndex, n);

    vector<int> a(aCount, 0);
    vector<int> b(bCount, 0);

    MPI_Recv(a.data(), aCount, MPI_INT, 0, 3, MPI_COMM_WORLD, &status);
    MPI_Recv(b.data(), bCount, MPI_INT, 0, 4, MPI_COMM_WORLD, &status);

    vector<int> partialRes(endIndex - startIndex, 0);

    int aSize = n; 
    int bSize = n; 

    for (int i = startIndex; i < endIndex; ++i) {
        int startX = max(0, i - (bSize - 1));
        int endX = min(i, aSize - 1);

        int sum = 0;
        for (int x = startX; x <= endX; ++x) {
            int y = i - x;
            if (x < aCount && y < bCount) {
                sum += a[x] * b[y];
            }
        }
        partialRes[i - startIndex] = sum;
    }

    MPI_Send(partialRes.data(), endIndex - startIndex, MPI_INT, 0, 5, MPI_COMM_WORLD);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int me, nrProcs;
    MPI_Comm_rank(MPI_COMM_WORLD, &me);
    MPI_Comm_size(MPI_COMM_WORLD, &nrProcs);

    srand((unsigned)time(NULL) + me);

    if (argc != 2) {
        if (me == 0) {
            cerr << "Usage: " << argv[0] << " <n>\n";
        }
        MPI_Finalize();
        return 1;
    }

    unsigned int n;
    if (1 != sscanf(argv[1], "%u", &n)) {
        if (me == 0) {
            cerr << "Invalid input. Please provide a positive integer for n.\n";
        }
        MPI_Finalize();
        return 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);
    auto start_time = std::chrono::high_resolution_clock::now();

    if (me == 0) {
        vector<int> a, b;
        generatePolynomials(a, b, n);

        distributeWork(a, b, nrProcs);

        int resultLength = (int)(2 * n - 1);
        int startIndex = 0;
        int endIndex = resultLength / nrProcs;
        if (endIndex == 0) endIndex = resultLength;
        vector<int> result(resultLength, 0);

        computeSegment(startIndex, endIndex, a, b, result);

        collectResults(nrProcs, result);

        bool valid = validateResult(a, b, result);

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = end_time - start_time;

        if (valid) {
            cout << "Result is valid." << endl;
        } else {
            cout << "Result is invalid." << endl;
        }
        cout << "Time taken: " << elapsed.count() << " seconds." << endl;

    } else {
        slaveProcess(me);
    }

    MPI_Finalize();
    return 0;
}
