#include <mpi.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <random>

std::vector<int> generateRandomVector(size_t size, int minValue = -10, int maxValue = 10) {
    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::uniform_int_distribution<int> distribution(minValue, maxValue);
    std::vector<int> randomVector(size);
    std::generate(randomVector.begin(), randomVector.end(), [&]() { return distribution(generator); });
    return randomVector;
}

std::vector<int> multiplyNaive(const std::vector<int>& A, const std::vector<int>& B) {
    std::vector<int> C(A.size() + B.size() - 1, 0);
    for (size_t i = 0; i < A.size(); i++)
        for (size_t j = 0; j < B.size(); j++)
            C[i + j] += A[i] * B[j];
    return C;
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int n = 10000;
    std::vector<int> A, B, C, naiveResult;
    double seqTime = 0.0, parTime = 0.0;

    if (rank == 0) {
        A = generateRandomVector(n + 1);
        B = generateRandomVector(n + 1);
        auto seqStart = std::chrono::high_resolution_clock::now();
        naiveResult = multiplyNaive(A, B);
        auto seqEnd = std::chrono::high_resolution_clock::now();
        seqTime = std::chrono::duration<double>(seqEnd - seqStart).count();
    }

    MPI_Bcast(&n, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (rank != 0) {
        A.resize(n + 1);
        B.resize(n + 1);
    }
    MPI_Bcast(A.data(), n + 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(B.data(), n + 1, MPI_INT, 0, MPI_COMM_WORLD);

    int resultSize = 2 * n + 1;
    if (rank == 0) C.resize(resultSize, 0);

    int chunk = std::ceil((double)resultSize / size);
    int start = rank * chunk;
    int end = std::min(start + chunk, resultSize);
    std::vector<int> C_partial(end > start ? end - start : 0, 0);

    MPI_Barrier(MPI_COMM_WORLD);
    auto parStart = std::chrono::high_resolution_clock::now();

    for (int i = start; i < end; ++i) {
        int coeff = 0;
        for (int k = 0; k <= i; ++k)
            if (k <= n && (i - k) <= n)
                coeff += A[k] * B[i - k];
        C_partial[i - start] = coeff;
    }
    if (rank == 0) {
        for (int i = start; i < end; ++i)
            C[i] = C_partial[i - start];
        for (int p = 1; p < size; ++p) {
            int pstart = p * chunk;
            int pend = std::min(pstart + chunk, resultSize);
            if (pstart >= resultSize) continue;
            std::vector<int> temp(pend - pstart);
            MPI_Recv(temp.data(), pend - pstart, MPI_INT, p, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = 0; i < (int)temp.size(); ++i)
                C[pstart + i] = temp[i];
        }
    } else if (!C_partial.empty()) {
        MPI_Send(C_partial.data(), C_partial.size(), MPI_INT, 0, 0, MPI_COMM_WORLD);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    auto parEnd = std::chrono::high_resolution_clock::now();
    parTime = std::chrono::duration<double>(parEnd - parStart).count();

    if (rank == 0) {
        std::cout << "Sequential naive time: " << seqTime << " seconds\n";
        std::cout << "MPI time: " << parTime << " seconds\n";

        bool equal = (C == naiveResult);
        if (equal) {
            std::cout << "Results are equal.\n";
        } else {
            std::cout << "Results differ.\n";
        }
    }

    MPI_Finalize();
    return 0;
}
